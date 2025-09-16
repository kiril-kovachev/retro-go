#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct marauder_ctx marauder_ctx_t;

marauder_ctx_t *marauder_init(void);
void marauder_tick(marauder_ctx_t *ctx, uint32_t buttons);
void marauder_draw(marauder_ctx_t *ctx, void *surface);
void marauder_shutdown(marauder_ctx_t *ctx);


