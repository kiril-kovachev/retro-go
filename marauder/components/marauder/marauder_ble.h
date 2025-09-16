#pragma once

#include <stdint.h>

typedef struct marauder_ble_scan_result_s {
    uint8_t addr[6];
    int8_t rssi;
    char name[32];
} marauder_ble_scan_result_t;

typedef struct marauder_ble_s marauder_ble_t;

marauder_ble_t *marauder_ble_init(void);
int marauder_ble_scan(marauder_ble_t *ble, marauder_ble_scan_result_t *out, int max, int duration_seconds);
void marauder_ble_shutdown(marauder_ble_t *ble);


