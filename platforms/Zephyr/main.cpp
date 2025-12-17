#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include <cstdint>
#include <cstdio>

LOG_MODULE_REGISTER(WARDuinoWifiTest, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

static struct net_mgmt_event_callback mgmt_cb;
static struct net_mgmt_event_callback ipv4_cb;

static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);
static K_SEM_DEFINE(ip_sem, 0, 1);

void wifi_args_to_params(struct wifi_connect_req_params *params, const char *ssid, const char *passwd) {
    params->ssid = (const uint8_t*) ssid;
    params->ssid_length = strlen((const char*) params->ssid);

    params->psk = (const uint8_t*) passwd;
    params->psk_length = strlen((const char*) params->psk);

    params->channel = WIFI_CHANNEL_ANY;
    params->security = WIFI_SECURITY_TYPE_PSK;
    params->mfp = WIFI_MFP_OPTIONAL;
    params->timeout = SYS_FOREVER_MS;
    params->band = WIFI_FREQ_BAND_UNKNOWN;
    memset(params->bssid, 0, sizeof(params->bssid));
}

int send_conn_request(const char *ssid, const char *passwd) {
    struct wifi_connect_req_params cnx_params;

    struct net_if *iface = net_if_get_first_wifi();
    if (iface == NULL) {
        printf("Returned network interface is NULL\n");
        return -1;
    }

    wifi_args_to_params(&cnx_params, ssid, passwd);

    printf("Connecting to Wi-Fi\n");
    net_if_up(iface);
    net_dhcpv4_start(iface);
    int err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params));
    if (err) {
        printf("Connecting to Wi-Fi failed, err: %d\n", err);
        return ENOEXEC;
    }
    return 0;
}

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
			  uint64_t mgmt_event, struct net_if *iface) {
    printf("Event %d\n", mgmt_event);
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        printf("Network connected\n");
        connected = true;
        k_sem_give(&run_app);
        return;
    }
    if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        if (connected == false) {
	    printf("Disconnected, waiting for network to be connected\n");
        } else {
	    printf("Network disconnected\n");
	    connected = false;
        }
        k_sem_reset(&run_app);
        return;
    }
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        printf("Got IP addr\n");
        for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
            char buf[NET_IPV4_ADDR_LEN];

            if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
                continue;
            }

            printk("IPv4 address: %s\n",
                    net_addr_ntop(AF_INET,
                                    &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
                                    buf, sizeof(buf)));
            printk("Subnet: %s\n",
                    net_addr_ntop(AF_INET,
                                    &iface->config.ip.ipv4->unicast[i].netmask,
                                    buf, sizeof(buf)));
            printk("Router: %s\n",
                    net_addr_ntop(AF_INET,
                                    &iface->config.ip.ipv4->gw,
                                    buf, sizeof(buf)));
        }
        k_sem_give(&ip_sem);
        return;
    }
}

int wifi_connect(const char *ssid, const char *passwd) {
    printf("Initializing Wi-Fi driver\n");
    // Sleep to allow initialization of Wi-Fi driver.
    k_sleep(K_SECONDS(1));

    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
    net_mgmt_add_event_callback(&mgmt_cb);

    net_mgmt_init_event_callback(&ipv4_cb, net_mgmt_event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);

    // Keep trying to connect until we are connected.
    while (!connected) {
        printf("Trying to connect...\n");
        if (send_conn_request(ssid, passwd)) {
            return -1;
        }
        k_sem_take(&run_app, K_FOREVER);
        if (!connected) {
            k_msleep(2000);
        }
    }

    printf("Connected sem finished! Waiting for IP...\n");

    k_sem_take(&ip_sem, K_FOREVER);

    return 0;
}

/*#include "../../src/WARDuino.h"
#include "upload.h"

WARDuino *wac = WARDuino::instance();*/

int main() {
    wifi_connect("Oneplus 7", "supertux");

    /*Module *m = wac->load_module(upload_wasm, upload_wasm_len, {});
    wac->run_module(m);
    wac->unload_module(m);*/
}
