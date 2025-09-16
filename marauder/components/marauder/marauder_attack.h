#pragma once

#include <stdint.h>
#include <stdbool.h>

bool marauder_set_channel(uint8_t primary);

bool marauder_send_deauth_broadcast(const uint8_t bssid[6], int times, int delay_ms);
bool marauder_send_deauth_to_client(const uint8_t ap_bssid[6], const uint8_t sta_mac[6], int times, int delay_ms);

bool marauder_send_beacon_spam(const char *ssid, const uint8_t bssid[6], uint8_t channel, int times, int delay_ms);


