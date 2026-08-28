#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Initialises both battle render buffers: every primitive in each pool gets its
 * GPU command code, the full-screen Gouraud overlay gets its corners and two
 * semi-transparent draw modes, and the ordering table pointers are bound.
 *
 * The function-scope counters are intentionally reused by each pool loop:
 * their lifetime reproduces the target register allocation. */
void battle_gfx_init_render_buffers(void) {
    POLY_F3 prototype;
    s32 primitive_index;
    s32 buffer_index;

    g_battle_data = &g_battle_render_buffers[0];
    g_battle_render_buffers[0].otag = g_battle_otags[0];
    g_battle_render_buffers[1].otag = g_battle_otags[1];
    {
        for (buffer_index = 0; buffer_index < 2; buffer_index++) {
            for (primitive_index = 0; primitive_index < 64; primitive_index++) {
                SetPolyFT3(&g_battle_render_buffers[buffer_index].ft3[primitive_index]);
            }
        }
    }
    SetPolyF3(&prototype);
    g_battle_map_light_direction[0] = 0;
    g_battle_map_light_direction[1] = 0;
    g_battle_map_light_direction[2] = 1;
    g_battle_map_light_direction[3] = prototype.code;
    SetPolyG4(&g_battle_render_buffers[0].overlay);
    SetPolyG4(&g_battle_render_buffers[1].overlay);
    g_battle_render_buffers[0].overlay.x0 = 0;
    g_battle_render_buffers[0].overlay.y0 = 0;
    g_battle_render_buffers[0].overlay.x1 = 0x180;
    g_battle_render_buffers[0].overlay.y1 = 0;
    g_battle_render_buffers[0].overlay.x2 = 0;
    g_battle_render_buffers[0].overlay.y2 = 0xF0;
    g_battle_render_buffers[0].overlay.x3 = 0x180;
    g_battle_render_buffers[0].overlay.y3 = 0xF0;
    g_battle_render_buffers[1].overlay.x0 = 0;
    g_battle_render_buffers[1].overlay.y0 = 0;
    g_battle_render_buffers[1].overlay.x1 = 0x180;
    g_battle_render_buffers[1].overlay.y1 = 0;
    g_battle_render_buffers[1].overlay.x2 = 0;
    g_battle_render_buffers[1].overlay.y2 = 0xF0;
    g_battle_render_buffers[1].overlay.x3 = 0x180;
    g_battle_render_buffers[1].overlay.y3 = 0xF0;
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    {
        for (primitive_index = 0; primitive_index < 360; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyGT3(&g_battle_render_buffers[buffer_index].gt3[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 710; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyGT4(&g_battle_render_buffers[buffer_index].gt4[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 64; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyF3(&g_battle_render_buffers[buffer_index].f3[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 256; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyF4(&g_battle_render_buffers[buffer_index].f4[primitive_index]);
            }
        }
    }
    g_battle_sound_primary_weather_sfx_id = 0;
    g_battle_sound_weather_sfx_enabled = 0;
    g_current_effect_work = 0;
}
