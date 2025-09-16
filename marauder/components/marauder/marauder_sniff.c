#include "marauder_sniff.h"

#include <string.h>

#include <esp_wifi.h>

typedef struct __attribute__((packed)) {
    uint8_t frame_control[2];
    uint16_t duration;
    uint8_t da[6];
    uint8_t sa[6];
    uint8_t bssid[6];
    uint16_t seq_ctrl;
} wifi_mgmt_hdr_t;

struct marauder_sniffer_s {
    uint8_t ap_bssid[6];
    marauder_client_t clients[32];
    int count;
};

static marauder_sniffer_t g_sniff = {0};

static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT)
        return;

    const wifi_promiscuous_pkt_t *ppkt = (const wifi_promiscuous_pkt_t *)buf;
    const uint8_t *payload = ppkt->payload;
    const wifi_mgmt_hdr_t *hdr = (const wifi_mgmt_hdr_t *)payload;

    // Track stations whose bssid matches the AP
    if (memcmp(hdr->bssid, g_sniff.ap_bssid, 6) != 0)
        return;

    // Consider data/control/mgmt to associate station MAC
    const uint8_t *sta = NULL;
    sta = hdr->sa;
    if (!sta)
        return;

    // Add/update client
    for (int i = 0; i < g_sniff.count; i++)
    {
        if (memcmp(g_sniff.clients[i].mac, sta, 6) == 0)
        {
            g_sniff.clients[i].last_rssi = ppkt->rx_ctrl.rssi;
            return;
        }
    }
    if (g_sniff.count < 32)
    {
        memcpy(g_sniff.clients[g_sniff.count].mac, sta, 6);
        g_sniff.clients[g_sniff.count].last_rssi = ppkt->rx_ctrl.rssi;
        g_sniff.count++;
    }
}

marauder_sniffer_t *marauder_sniff_start(const uint8_t ap_bssid[6], uint8_t channel)
{
    memcpy(g_sniff.ap_bssid, ap_bssid, 6);
    g_sniff.count = 0;
    wifi_promiscuous_filter_t f = {0};
    f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&f);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    return &g_sniff;
}

int marauder_sniff_get_clients(marauder_sniffer_t *sniff, marauder_client_t *out, int max)
{
    (void)sniff;
    int n = g_sniff.count < max ? g_sniff.count : max;
    memcpy(out, g_sniff.clients, n * sizeof(marauder_client_t));
    return n;
}

void marauder_sniff_stop(marauder_sniffer_t *sniff)
{
    (void)sniff;
    esp_wifi_set_promiscuous(false);
}


