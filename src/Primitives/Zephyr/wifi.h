#pragma once
#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/types.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

static net_mgmt_event_callback mgmt_cb;
static net_mgmt_event_callback ipv4_cb;
static net_mgmt_event_callback scan_cb;
static net_mgmt_event_callback disconnect_cb;

static volatile bool connected;
static K_SEM_DEFINE(run_app, 0, 1);
static K_SEM_DEFINE(ip_sem, 0, 1);
static K_SEM_DEFINE(scan_done_sem, 0, 1);
static K_SEM_DEFINE(disconnect_done, 0, 1);

void wifi_args_to_params(wifi_connect_req_params *params, const char *ssid,
                         const char *passwd) {
    memset(params, 0, sizeof(*params));
    params->ssid = (const uint8_t *)ssid;
    params->ssid_length = strlen((const char *)params->ssid);

    params->psk = (const uint8_t *)passwd;
    params->psk_length = strlen((const char *)params->psk);

    params->channel = WIFI_CHANNEL_ANY;
    params->security = WIFI_SECURITY_TYPE_PSK;
    params->mfp = WIFI_MFP_DISABLE;
    params->timeout = SYS_FOREVER_MS;
    params->band = WIFI_FREQ_BAND_2_4_GHZ;
    memset(params->bssid, 0, sizeof(params->bssid));
}

int send_conn_request(const char *ssid, const char *passwd) {
    wifi_connect_req_params cnx_params;

    net_if *iface = net_if_get_first_wifi();
    if (iface == NULL) {
        printf("No Wi-Fi interface found\n");
        return -1;
    }

    wifi_args_to_params(&cnx_params, ssid, passwd);

    printf("Connecting to %s\n", ssid);
    int err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params,
                       sizeof(struct wifi_connect_req_params));
    if (err) {
        printf("Connection request failed, err: %d\n", err);
        return err;
    }
    return 0;
}

static void scan_result_handler(net_mgmt_event_callback *cb,
                                uint64_t mgmt_event, net_if *iface) {
    if (mgmt_event == NET_EVENT_WIFI_SCAN_RESULT) {
        const wifi_scan_result *entry =
            (const struct wifi_scan_result *)cb->info;
        if (entry) {
            printf("  AP: %-32s ch=%-3d rssi=%-4d security=%d\n", entry->ssid,
                   entry->channel, entry->rssi, entry->security);
        }
    }
    if (mgmt_event == NET_EVENT_WIFI_SCAN_DONE) {
        k_sem_give(&scan_done_sem);
    }
}

static void net_mgmt_event_handler(net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, net_if *iface) {
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        const wifi_status *status = (const wifi_status *)cb->info;
        if (status && status->conn_status == WIFI_STATUS_CONN_SUCCESS) {
            printf("Network connected\n");
            connected = true;
        } else {
            printf("Connection failed (conn_status=%d), retrying\n",
                   status ? status->conn_status : -1);
        }
        k_sem_give(&run_app);
        return;
    }
    if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        if (connected == false) {
            printf("Connection failed, retrying\n");
            k_sem_give(&run_app);
        } else {
            printf("Network disconnected\n");
            connected = false;
        }
        return;
    }
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        printf("Got IP addr\n");
        for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
            char buf[NET_IPV4_ADDR_LEN];

            if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type !=
                NET_ADDR_DHCP) {
                continue;
            }

            printf("IPv4 address: %s\n",
                   net_addr_ntop(
                       AF_INET,
                       &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
                       buf, sizeof(buf)));
            printf("Subnet: %s\n",
                   net_addr_ntop(AF_INET,
                                 &iface->config.ip.ipv4->unicast[i].netmask,
                                 buf, sizeof(buf)));
            printf("Router: %s\n",
                   net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf,
                                 sizeof(buf)));
        }
        k_sem_give(&ip_sem);
        return;
    }
}

static void disconnect_result_handler(net_mgmt_event_callback *cb,
                                      uint64_t mgmt_event, net_if *iface) {
    if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        printf("Disconnect event received\n");
        k_sem_give(&disconnect_done);
    }
}

inline int network_connect(const char *ssid, const char *passwd) {
    printf("Initializing Wi-Fi driver\n");
    k_sleep(K_SECONDS(5));

    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT);
    net_mgmt_add_event_callback(&mgmt_cb);

    net_mgmt_init_event_callback(&ipv4_cb, net_mgmt_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);

    net_if *iface = net_if_get_first_wifi();
    if (iface == nullptr) {
        printf("No Wi-Fi interface found\n");
        return -1;
    }
    net_if_up(iface);

    // Scan networks in the area
    net_mgmt_init_event_callback(
        &scan_cb, scan_result_handler,
        NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
    net_mgmt_add_event_callback(&scan_cb);
    k_sem_reset(&scan_done_sem);
    if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, nullptr, 0) == 0) {
        k_sem_take(&scan_done_sem, K_SECONDS(10));
    }
    net_mgmt_del_event_callback(&scan_cb);

    // Keep trying to connect until we are connected.
    while (!connected) {
        printf("Trying to connect to %s...\n", ssid);
        k_sem_reset(&run_app);
        if (send_conn_request(ssid, passwd) != 0) {
            k_msleep(2000);
            continue;
        }
        k_sem_take(&run_app, K_SECONDS(30));
        if (!connected) {
            k_msleep(2000);
        }
    }

    printf("Connected! Starting DHCP...\n");
    net_dhcpv4_start(iface);
    k_sem_take(&ip_sem, K_SECONDS(30));

    return 0;
}

inline int network_disconnect() {
    printf("Request network disconnect\n");
    net_if *iface = net_if_get_first_wifi();
    if (iface == nullptr) {
        printf("No Wi-Fi interface found\n");
        return -1;
    }
    net_mgmt_init_event_callback(&disconnect_cb, disconnect_result_handler,
                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&disconnect_cb);
    k_sem_reset(&disconnect_done);
    net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, nullptr, 0);
    k_sem_take(&disconnect_done, K_SECONDS(30));
    printf("Network disconnected!\n");
    return 0;
}

inline void network_ip(char *buf) {
    const net_if *iface = net_if_get_first_wifi();
    net_addr_ntop(AF_INET,
                  &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr, buf,
                  NET_IPV4_ADDR_LEN);
}