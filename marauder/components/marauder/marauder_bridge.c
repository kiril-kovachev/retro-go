#include "marauder_bridge.h"

#include <rg_display.h>
#include <rg_surface.h>
#include <rg_gui.h>
#include <rg_input.h>
#include <rg_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "marauder_wifi.h"
#include "marauder_ble.h"
#include "marauder_attack.h"
#include "marauder_sniff.h"
#include <stdio.h>
#include <string.h>
#include <rg_settings.h>

// Ensure prototype is visible to avoid implicit-int on some build setups
char *rg_gui_input_str(const char *title, const char *message, const char *default_value);

typedef struct menu_item_s {
    const char *label;
} menu_item_t;

struct marauder_ctx {
    int selected_index;
    int num_items;
    const menu_item_t *items;
    uint32_t prev_buttons;
    marauder_wifi_t *wifi;
    marauder_wifi_scan_result_t scan_results[32];
    int scan_count;
    bool showing_scan;
    int scan_cursor;
    marauder_wifi_scan_result_t selected_ap;
    bool has_selected_ap;
    marauder_ble_t *ble;
    marauder_ble_scan_result_t ble_results[32];
    int ble_count;
    bool showing_ble;
    // clients
    bool showing_clients;
    marauder_sniffer_t *sniffer;
    marauder_client_t clients[32];
    int client_count;
    int client_cursor;
    // settings
    bool showing_settings;
    int settings_cursor;
    int deauth_times;
    int deauth_delay_ms;
    int beacon_times;
    int beacon_delay_ms;
    char beacon_ssid[33];
    bool loop_enabled;
    bool hop_enabled;
    bool target_all;
    // continuous task
    bool running;
    int running_mode; // 1=deauth, 2=beacon
    TaskHandle_t task;
};

static const menu_item_t s_menu_items[] = {
    {"WiFi Scan"},
    {"Bluetooth Scan"},
    {"Deauth"},
    {"Beacon Spam"},
    {"Clients"},
    {"Settings"},
};

marauder_ctx_t *marauder_init(void)
{
    static struct marauder_ctx ctx = {0};
    ctx.selected_index = 0;
    ctx.items = s_menu_items;
    ctx.num_items = sizeof(s_menu_items) / sizeof(s_menu_items[0]);
    ctx.prev_buttons = 0;
    ctx.wifi = marauder_wifi_init();
    ctx.scan_count = 0;
    ctx.showing_scan = false;
    ctx.scan_cursor = 0;
    ctx.has_selected_ap = false;
    ctx.ble = marauder_ble_init();
    ctx.ble_count = 0;
    ctx.showing_ble = false;
    ctx.showing_clients = false;
    ctx.sniffer = NULL;
    ctx.client_count = 0;
    ctx.client_cursor = 0;
    ctx.showing_settings = false;
    ctx.settings_cursor = 0;
    // Load settings
    ctx.deauth_times = (int)rg_settings_get_number(NS_APP, "deauth_times", 50);
    ctx.deauth_delay_ms = (int)rg_settings_get_number(NS_APP, "deauth_delay_ms", 20);
    ctx.beacon_times = (int)rg_settings_get_number(NS_APP, "beacon_times", 100);
    ctx.beacon_delay_ms = (int)rg_settings_get_number(NS_APP, "beacon_delay_ms", 10);
    char *ssid = rg_settings_get_string(NS_APP, "beacon_ssid", "Marauder-Spam");
    strncpy(ctx.beacon_ssid, ssid ? ssid : "Marauder-Spam", sizeof(ctx.beacon_ssid) - 1);
    ctx.beacon_ssid[sizeof(ctx.beacon_ssid)-1] = '\0';
    if (ssid) free(ssid);
    ctx.loop_enabled = rg_settings_get_number(NS_APP, "continuous", 0) != 0;
    ctx.hop_enabled = rg_settings_get_number(NS_APP, "hop", 0) != 0;
    ctx.target_all = rg_settings_get_number(NS_APP, "target_all", 0) != 0;
    ctx.running = false;
    ctx.running_mode = 0;
    ctx.task = NULL;
    return &ctx;
}

static void apply_channel_hop_and_send_deauth(const marauder_ctx_t *ctx)
{
    int times = ctx->deauth_times * (ctx->loop_enabled ? 5 : 1);
    int delay_ms = ctx->deauth_delay_ms;
    if (ctx->hop_enabled)
    {
        for (uint8_t ch = 1; ch <= 13; ch++)
        {
            marauder_set_channel(ch);
            marauder_send_deauth_broadcast(ctx->selected_ap.bssid, times / 13 + 1, delay_ms);
        }
    }
    else
    {
        marauder_set_channel(ctx->selected_ap.channel);
        marauder_send_deauth_broadcast(ctx->selected_ap.bssid, times, delay_ms);
    }
}

static void __attribute__((unused)) apply_channel_hop_and_send_beacon(const marauder_ctx_t *ctx)
{
    int times = ctx->beacon_times * (ctx->loop_enabled ? 5 : 1);
    int delay_ms = ctx->beacon_delay_ms;
    uint8_t fake_bssid[6] = {0xDE,0xAD,0xBE,0xEF,0x00,0x01};
    if (ctx->hop_enabled)
    {
        for (uint8_t ch = 1; ch <= 13; ch++)
        {
            marauder_set_channel(ch);
            marauder_send_beacon_spam(ctx->beacon_ssid, fake_bssid, ch, times / 13 + 1, delay_ms);
        }
    }
    else
    {
        uint8_t ch = ctx->has_selected_ap ? ctx->selected_ap.channel : 1;
        marauder_set_channel(ch);
        marauder_send_beacon_spam(ctx->beacon_ssid, fake_bssid, ch, times, delay_ms);
    }
}

static void save_settings(const marauder_ctx_t *ctx)
{
    rg_settings_set_number(NS_APP, "deauth_times", ctx->deauth_times);
    rg_settings_set_number(NS_APP, "deauth_delay_ms", ctx->deauth_delay_ms);
    rg_settings_set_number(NS_APP, "beacon_times", ctx->beacon_times);
    rg_settings_set_number(NS_APP, "beacon_delay_ms", ctx->beacon_delay_ms);
    rg_settings_set_string(NS_APP, "beacon_ssid", ctx->beacon_ssid);
    rg_settings_set_number(NS_APP, "continuous", ctx->loop_enabled ? 1 : 0);
    rg_settings_set_number(NS_APP, "hop", ctx->hop_enabled ? 1 : 0);
    rg_settings_set_number(NS_APP, "target_all", ctx->target_all ? 1 : 0);
}

static void deauth_once(const marauder_ctx_t *ctx, const marauder_wifi_scan_result_t *ap)
{
    marauder_set_channel(ap->channel);
    marauder_send_deauth_broadcast(ap->bssid, ctx->deauth_times, ctx->deauth_delay_ms);
}

static void beacon_once(const marauder_ctx_t *ctx, uint8_t channel)
{
    uint8_t fake_bssid[6] = {0xDE,0xAD,0xBE,0xEF,0x00,0x01};
    marauder_set_channel(channel);
    marauder_send_beacon_spam(ctx->beacon_ssid, fake_bssid, channel, ctx->beacon_times, ctx->beacon_delay_ms);
}

static void attack_task(void *arg)
{
    marauder_ctx_t *ctx = (marauder_ctx_t *)arg;
    while (ctx->running)
    {
        if (ctx->running_mode == 1) // deauth
        {
            if (ctx->target_all && ctx->scan_count > 0)
            {
                for (int i = 0; i < ctx->scan_count && ctx->running; i++)
                {
                    deauth_once(ctx, &ctx->scan_results[i]);
                }
            }
            else if (ctx->has_selected_ap)
            {
                deauth_once(ctx, &ctx->selected_ap);
            }
        }
        else if (ctx->running_mode == 2) // beacon
        {
            if (ctx->hop_enabled)
            {
                for (uint8_t ch = 1; ch <= 13 && ctx->running; ch++)
                    beacon_once(ctx, ch);
            }
            else
            {
                uint8_t ch = ctx->has_selected_ap ? ctx->selected_ap.channel : 1;
                beacon_once(ctx, ch);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ctx->task = NULL;
    vTaskDelete(NULL);
}

void marauder_tick(marauder_ctx_t *ctx, uint32_t buttons)
{
    uint32_t changed = (ctx->prev_buttons ^ buttons);

    // Global stop for running task
    if (ctx->running)
    {
        if ((changed & RG_KEY_B) && (buttons & RG_KEY_B))
        {
            ctx->running = false;
            rg_gui_alert("Marauder", "Stopped");
        }
    }

    if (!ctx->showing_scan && !ctx->showing_ble && !ctx->showing_clients && !ctx->showing_settings && !ctx->running)
    {
        if ((changed & RG_KEY_UP) && (buttons & RG_KEY_UP))
            ctx->selected_index = (ctx->selected_index - 1 + ctx->num_items) % ctx->num_items;
        if ((changed & RG_KEY_DOWN) && (buttons & RG_KEY_DOWN))
            ctx->selected_index = (ctx->selected_index + 1) % ctx->num_items;

        if ((changed & RG_KEY_A) && (buttons & RG_KEY_A))
        {
            if (ctx->selected_index == 0)
            {
                ctx->scan_count = marauder_wifi_scan(ctx->wifi, ctx->scan_results, 32);
                ctx->scan_cursor = 0;
                ctx->showing_scan = true;
            }
            else if (ctx->selected_index == 1)
            {
                ctx->ble_count = marauder_ble_scan(ctx->ble, ctx->ble_results, 32, 5);
                ctx->showing_ble = true;
            }
            else if (ctx->selected_index == 2)
            {
                if (!ctx->has_selected_ap && !ctx->target_all)
                    rg_gui_alert("Deauth", "Select an AP or enable 'Target: All'");
                else if (ctx->loop_enabled)
                {
                    ctx->running = true;
                    ctx->running_mode = 1;
                    xTaskCreate(attack_task, "deauth_loop", 3072, ctx, RG_TASK_PRIORITY_2, &ctx->task);
                    rg_gui_alert("Deauth", "Running... B: Stop");
                }
                else
                {
                    if (ctx->target_all && ctx->scan_count > 0)
                        for (int i = 0; i < ctx->scan_count; i++) deauth_once(ctx, &ctx->scan_results[i]);
                    else if (ctx->has_selected_ap) deauth_once(ctx, &ctx->selected_ap);
                    rg_gui_alert("Deauth", "Done");
                }
            }
            else if (ctx->selected_index == 3)
            {
                if (!ctx->has_selected_ap && !ctx->hop_enabled)
                    rg_gui_alert("Beacon", "Select an AP or enable hop");
                else if (ctx->loop_enabled)
                {
                    ctx->running = true;
                    ctx->running_mode = 2;
                    xTaskCreate(attack_task, "beacon_loop", 3072, ctx, RG_TASK_PRIORITY_2, &ctx->task);
                    rg_gui_alert("Beacon", "Running... B: Stop");
                }
                else
                {
                    if (ctx->hop_enabled)
                        for (uint8_t ch = 1; ch <= 13; ch++) beacon_once(ctx, ch);
                    else
                        beacon_once(ctx, ctx->has_selected_ap ? ctx->selected_ap.channel : 1);
                    rg_gui_alert("Beacon", "Done");
                }
            }
            else if (ctx->selected_index == 4)
            {
                if (!ctx->has_selected_ap)
                {
                    rg_gui_alert("Clients", "Select an AP first");
                }
                else
                {
                    marauder_set_channel(ctx->selected_ap.channel);
                    ctx->sniffer = marauder_sniff_start(ctx->selected_ap.bssid, ctx->selected_ap.channel);
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    ctx->client_count = marauder_sniff_get_clients(ctx->sniffer, ctx->clients, 32);
                    marauder_sniff_stop(ctx->sniffer);
                    ctx->client_cursor = 0;
                    ctx->showing_clients = true;
                }
            }
            else if (ctx->selected_index == 5)
            {
                ctx->settings_cursor = 0;
                ctx->showing_settings = true;
            }
            else
            {
                rg_gui_alert("Marauder", ctx->items[ctx->selected_index].label);
            }
        }
    }
    else if (ctx->showing_scan)
    {
        int shown = ctx->scan_count < 12 ? ctx->scan_count : 12;
        if ((changed & RG_KEY_UP) && (buttons & RG_KEY_UP))
            ctx->scan_cursor = (ctx->scan_cursor - 1 + shown) % shown;
        if ((changed & RG_KEY_DOWN) && (buttons & RG_KEY_DOWN))
            ctx->scan_cursor = (ctx->scan_cursor + 1) % shown;

        if ((changed & RG_KEY_A) && (buttons & RG_KEY_A))
        {
            ctx->selected_ap = ctx->scan_results[ctx->scan_cursor];
            ctx->has_selected_ap = true;
            ctx->showing_scan = false;
        }

        if ((changed & RG_KEY_B) && (buttons & RG_KEY_B))
            ctx->showing_scan = false;
    }
    else if (ctx->showing_ble)
    {
        if ((changed & RG_KEY_B) && (buttons & RG_KEY_B))
            ctx->showing_ble = false;
    }
    else if (ctx->showing_clients)
    {
        int shown = ctx->client_count < 12 ? ctx->client_count : 12;
        if ((changed & RG_KEY_UP) && (buttons & RG_KEY_UP))
            ctx->client_cursor = (ctx->client_cursor - 1 + shown) % shown;
        if ((changed & RG_KEY_DOWN) && (buttons & RG_KEY_DOWN))
            ctx->client_cursor = (ctx->client_cursor + 1) % shown;

        if ((changed & RG_KEY_A) && (buttons & RG_KEY_A))
        {
            marauder_set_channel(ctx->selected_ap.channel);
            int times = ctx->deauth_times * (ctx->loop_enabled ? 5 : 1);
            marauder_send_deauth_to_client(ctx->selected_ap.bssid, ctx->clients[ctx->client_cursor].mac, times, ctx->deauth_delay_ms);
            rg_gui_alert("Deauth", "Targeted deauth sent");
        }
        if ((changed & RG_KEY_B) && (buttons & RG_KEY_B))
            ctx->showing_clients = false;
    }
    else if (ctx->showing_settings)
    {
        const int num = 8;
        if ((changed & RG_KEY_UP) && (buttons & RG_KEY_UP))
            ctx->settings_cursor = (ctx->settings_cursor - 1 + num) % num;
        if ((changed & RG_KEY_DOWN) && (buttons & RG_KEY_DOWN))
            ctx->settings_cursor = (ctx->settings_cursor + 1) % num;

        if ((changed & RG_KEY_LEFT) && (buttons & RG_KEY_LEFT))
        {
            if (ctx->settings_cursor == 0 && ctx->deauth_times > 1) ctx->deauth_times -= 1;
            if (ctx->settings_cursor == 1 && ctx->deauth_delay_ms > 0) ctx->deauth_delay_ms -= 1;
            if (ctx->settings_cursor == 2 && ctx->beacon_times > 1) ctx->beacon_times -= 1;
            if (ctx->settings_cursor == 3 && ctx->beacon_delay_ms > 0) ctx->beacon_delay_ms -= 1;
        }
        if ((changed & RG_KEY_RIGHT) && (buttons & RG_KEY_RIGHT))
        {
            if (ctx->settings_cursor == 0) ctx->deauth_times += 1;
            if (ctx->settings_cursor == 1) ctx->deauth_delay_ms += 1;
            if (ctx->settings_cursor == 2) ctx->beacon_times += 1;
            if (ctx->settings_cursor == 3) ctx->beacon_delay_ms += 1;
        }
        if ((changed & RG_KEY_A) && (buttons & RG_KEY_A))
        {
            if (ctx->settings_cursor == 4) ctx->loop_enabled = !ctx->loop_enabled;
            if (ctx->settings_cursor == 5) ctx->hop_enabled = !ctx->hop_enabled;
            if (ctx->settings_cursor == 6)
            {
                char *val = rg_gui_input_str("Beacon SSID", "Enter SSID", ctx->beacon_ssid);
                if (val)
                {
                    strncpy(ctx->beacon_ssid, val, sizeof(ctx->beacon_ssid) - 1);
                    ctx->beacon_ssid[sizeof(ctx->beacon_ssid)-1] = '\0';
                    free(val);
                }
            }
            if (ctx->settings_cursor == 7) ctx->target_all = !ctx->target_all;
            save_settings(ctx);
        }
        if ((changed & RG_KEY_B) && (buttons & RG_KEY_B))
        {
            save_settings(ctx);
            ctx->showing_settings = false;
        }
    }
    else if (ctx->showing_scan || ctx->showing_ble || ctx->showing_clients)
    {
        // existing sub-UIs handled above; nothing to change here for persistence
    }

    ctx->prev_buttons = buttons;
}

void marauder_draw(marauder_ctx_t *ctx, void *surface)
{
    rg_surface_t *fb = (rg_surface_t *)surface;
    rg_gui_set_surface(fb);
    // Clear the entire surface to black
    rg_surface_fill(fb, NULL, C_BLACK);
    // Also clear the display to ensure no background artifacts
    rg_display_clear(C_BLACK);

    if (ctx->running)
    {
        const char *mode = ctx->running_mode == 1 ? "Deauth running" : "Beacon running";
        rg_gui_draw_text(RG_GUI_CENTER, RG_GUI_CENTER, 0, mode, C_WHITE, C_BLACK, 0);
        rg_gui_draw_text(RG_GUI_CENTER, RG_GUI_BOTTOM, 0, "B: Stop", C_WHITE, C_BLACK, 0);
        return;
    }

    if (!ctx->showing_scan && !ctx->showing_ble && !ctx->showing_clients && !ctx->showing_settings)
    {
        rg_gui_draw_text(RG_GUI_CENTER, 10, 0, "ESP32 Marauder", C_WHITE, C_BLACK, 0);
        int top = 40;
        for (int i = 0; i < ctx->num_items; i++)
        {
            rg_color_t fg = (i == ctx->selected_index) ? C_CYAN : C_WHITE;
            rg_gui_draw_text(16, top + i * 16, 0, ctx->items[i].label, fg, C_BLACK, 0);
        }
        if (ctx->has_selected_ap)
        {
            char sel[64];
            snprintf(sel, sizeof(sel), "Selected AP: %s ch%u",
                     ctx->selected_ap.ssid, ctx->selected_ap.channel);
            rg_gui_draw_text(16, top + ctx->num_items * 16 + 8, 0, sel, C_WHITE, C_BLACK, 0);
        }
        else
        {
            rg_gui_draw_text(16, top + ctx->num_items * 16 + 8, 0, "A: Select", C_WHITE, C_BLACK, 0);
        }
    }
    else if (ctx->showing_scan)
    {
        rg_gui_draw_text(RG_GUI_CENTER, 10, 0, "WiFi Scan Results (A: Select, B: Back)", C_WHITE, C_BLACK, 0);
        int top = 40;
        int shown = ctx->scan_count < 12 ? ctx->scan_count : 12;
        for (int i = 0; i < shown; i++)
        {
            char line[64];
            snprintf(line, sizeof(line), "%2d) %-24s ch%2d %4d dBm",
                     i + 1, ctx->scan_results[i].ssid, ctx->scan_results[i].channel, ctx->scan_results[i].rssi);
            rg_color_t fg = (i == ctx->scan_cursor) ? C_CYAN : C_WHITE;
            rg_gui_draw_text(8, top + i * 14, 0, line, fg, C_BLACK, 0);
        }
        char footer[48];
        snprintf(footer, sizeof(footer), "%d network(s)", ctx->scan_count);
        rg_gui_draw_text(8, top + shown * 14 + 8, 0, footer, C_WHITE, C_BLACK, 0);
    }
    else if (ctx->showing_ble)
    {
        rg_gui_draw_text(RG_GUI_CENTER, 10, 0, "BLE Scan Results", C_WHITE, C_BLACK, 0);
        int top = 40;
        int shown = ctx->ble_count < 12 ? ctx->ble_count : 12;
        for (int i = 0; i < shown; i++)
        {
            char addr[18];
            snprintf(addr, sizeof(addr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     ctx->ble_results[i].addr[0], ctx->ble_results[i].addr[1], ctx->ble_results[i].addr[2],
                     ctx->ble_results[i].addr[3], ctx->ble_results[i].addr[4], ctx->ble_results[i].addr[5]);
            char line[80];
            snprintf(line, sizeof(line), "%2d) %-20s %s %4d dBm",
                     i + 1,
                     ctx->ble_results[i].name[0] ? ctx->ble_results[i].name : "(no name)",
                     addr,
                     ctx->ble_results[i].rssi);
            rg_gui_draw_text(8, top + i * 14, 0, line, C_WHITE, C_BLACK, 0);
        }
        char footer[48];
        snprintf(footer, sizeof(footer), "%d device(s). B: Back", ctx->ble_count);
        rg_gui_draw_text(8, top + shown * 14 + 8, 0, footer, C_WHITE, C_BLACK, 0);
    }
    else if (ctx->showing_clients)
    {
        rg_gui_draw_text(RG_GUI_CENTER, 10, 0, "Clients (A: Deauth, B: Back)", C_WHITE, C_BLACK, 0);
        int top = 40;
        int shown = ctx->client_count < 12 ? ctx->client_count : 12;
        for (int i = 0; i < shown; i++)
        {
            char mac[18];
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                     ctx->clients[i].mac[0], ctx->clients[i].mac[1], ctx->clients[i].mac[2],
                     ctx->clients[i].mac[3], ctx->clients[i].mac[4], ctx->clients[i].mac[5]);
            char line[64];
            snprintf(line, sizeof(line), "%2d) %s %4d dBm", i + 1, mac, ctx->clients[i].last_rssi);
            rg_color_t fg = (i == ctx->client_cursor) ? C_CYAN : C_WHITE;
            rg_gui_draw_text(8, top + i * 14, 0, line, fg, C_BLACK, 0);
        }
    }
    else if (ctx->showing_settings)
    {
        rg_gui_draw_text(RG_GUI_CENTER, 10, 0, "Settings (A toggle/edit, B back)", C_WHITE, C_BLACK, 0);
        int top = 40;
        const int num = 8;
        for (int i = 0; i < num; i++)
        {
            char line[64];
            switch (i)
            {
                case 0: snprintf(line, sizeof(line), "Deauth count: %d", ctx->deauth_times); break;
                case 1: snprintf(line, sizeof(line), "Deauth delay: %d ms", ctx->deauth_delay_ms); break;
                case 2: snprintf(line, sizeof(line), "Beacon count: %d", ctx->beacon_times); break;
                case 3: snprintf(line, sizeof(line), "Beacon delay: %d ms", ctx->beacon_delay_ms); break;
                case 4: snprintf(line, sizeof(line), "Loop: %s", ctx->loop_enabled ? "on" : "off"); break;
                case 5: snprintf(line, sizeof(line), "Channel hop: %s", ctx->hop_enabled ? "on" : "off"); break;
                case 6: snprintf(line, sizeof(line), "Beacon SSID: %s", ctx->beacon_ssid); break;
                case 7: snprintf(line, sizeof(line), "Target: %s", ctx->target_all ? "All" : "Selected"); break;
                default: line[0] = 0; break;
            }
            rg_color_t fg = (i == ctx->settings_cursor) ? C_CYAN : C_WHITE;
            rg_gui_draw_text(8, top + i * 16, 0, line, fg, C_BLACK, 0);
        }
        rg_gui_draw_text(8, top + num * 16 + 8, 0, "Left/Right to adjust, A toggle/edit", C_WHITE, C_BLACK, 0);
    }
}

void marauder_shutdown(marauder_ctx_t *ctx)
{
    (void)ctx;
}


