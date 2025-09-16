#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct marauder_client_s {
	uint8_t mac[6];
	int8_t last_rssi;
} marauder_client_t;

typedef struct marauder_sniffer_s marauder_sniffer_t;

marauder_sniffer_t *marauder_sniff_start(const uint8_t ap_bssid[6], uint8_t channel);
int marauder_sniff_get_clients(marauder_sniffer_t *sniff, marauder_client_t *out, int max);
void marauder_sniff_stop(marauder_sniffer_t *sniff);


