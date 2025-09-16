/*
 * Minimal stub app to integrate ESP32Marauder later.
 */

#include <rg_system.h>
#include <rg_display.h>
#include <rg_gui.h>
#include <rg_surface.h>
#include "../components/marauder/marauder_bridge.h"

static rg_surface_t *framebuffer;
static marauder_ctx_t *mctx;

static void event_handler(int event, void *arg)
{
    if (event == RG_EVENT_REDRAW && framebuffer)
        rg_display_submit(framebuffer, 0);
}

void app_main()
{
    const rg_handlers_t handlers = {
        .event = &event_handler,
    };

    rg_app_t *app = rg_system_init(22050, &handlers, NULL);
    (void)app;

    const rg_display_t *display = rg_display_get_info();
    framebuffer = rg_surface_create(display->screen.width, display->screen.height, RG_PIXEL_565_LE, MEM_FAST);

    rg_surface_fill(framebuffer, NULL, 0x0000);
    mctx = marauder_init();

    while (1)
    {
        marauder_tick(mctx, rg_input_read_gamepad());
        marauder_draw(mctx, framebuffer);
        rg_display_submit(framebuffer, 0);
        rg_display_sync(true);
    }
}


