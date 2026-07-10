#pragma once
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/types.h>

#include <cstdint>
#include <cstdio>

#define EVENT_MASK \
    (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

static net_mgmt_event_callback mgmt_cb;
static net_mgmt_event_callback ipv4_cb;
static net_mgmt_event_callback scan_cb;

static volatile bool connected;
static K_SEM_DEFINE(run_app, 0, 1);
static K_SEM_DEFINE(ip_sem, 0, 1);
static K_SEM_DEFINE(scan_done_sem, 0, 1);

void wifi_args_to_params(wifi_connect_req_params *params,
                         const char *ssid, const char *passwd) {
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
            printf("  AP: %-32s ch=%-3d rssi=%-4d security=%d\n",
                   entry->ssid, entry->channel, entry->rssi, entry->security);
        }
    }
    if (mgmt_event == NET_EVENT_WIFI_SCAN_DONE) {
        k_sem_give(&scan_done_sem);
    }
}

static void net_mgmt_event_handler(net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, net_if *iface) {
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        const wifi_status *status =
            (const wifi_status *)cb->info;
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

            printk("IPv4 address: %s\n",
                   net_addr_ntop(
                       AF_INET,
                       &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
                       buf, sizeof(buf)));
            printk("Subnet: %s\n",
                   net_addr_ntop(AF_INET,
                                 &iface->config.ip.ipv4->unicast[i].netmask,
                                 buf, sizeof(buf)));
            printk("Router: %s\n",
                   net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf,
                                 sizeof(buf)));
        }
        k_sem_give(&ip_sem);
        return;
    }
}

// TCP socket test
#include <sys/socket.h>
#include <arpa/inet.h>

namespace warduino {
    inline int socket_create(const char *ip, int port) {
        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            printk("Socket creation failed\n");
            return -1;
        }

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = inet_addr(ip);

        if (connect(
            sock,
            (sockaddr *)&server_addr,
            sizeof(server_addr)) < 0
            ) {
            printk("Failed to connect %s\n", strerror(errno));
            close(sock);
            return -1;
            }
        printk("Connected to %s:%d\n", ip, port);
        return sock;
    }

    inline int socket_send(int socket, const char* message) {
        return send(socket, message, strlen(message), 0);
    }

    inline int socket_close(int socket) {
        return close(socket);
    }
}

int tcp_send_message(const char *ip, int port, const char *message) {
    const int sock = warduino::socket_create(ip, port);
    if (sock < 0) {
        printf("Failed to create socket!\n");
        return -1;
    }

    if (warduino::socket_send(sock, message) < 0) {
        printk("Send failed\n");
    }

    warduino::socket_close(sock);
    printk("Connection closed\n");

    return 0;
}

inline int network_connect(const char *ssid, const char *passwd) {
    printf("Initializing Wi-Fi driver\n");
    k_sleep(K_SECONDS(5));

    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
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
    net_mgmt_init_event_callback(&scan_cb, scan_result_handler,
        NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
    net_mgmt_add_event_callback(&scan_cb);
    for (int i = 0; i < 3; i++) {
        printf("Scan %d/3 (looking for: %s)...\n", i + 1, ssid);
        k_sem_reset(&scan_done_sem);
        if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, nullptr, 0) == 0) {
            k_sem_take(&scan_done_sem, K_SECONDS(10));
        }
        if (i < 2) {
            k_msleep(2000);
        }
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
