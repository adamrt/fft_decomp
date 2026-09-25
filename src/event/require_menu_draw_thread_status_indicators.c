#include "fft/event_require.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Build and submit the two translucent letterbox panels of the event dialogue
 * overlay.
 *
 * Threads 8 and 7 running task 0x39 with a cleared first parameter request a
 * panel; parameter 2 selects which of the two opens. Each panel fades its
 * gradient in by 0xc per frame up to 0x78 and back out again, alternating
 * between two primitive buffers so the previous frame stays queued. The
 * EQUIP twin watches threads 13 and 14.
 */
void require_menu_draw_thread_status_indicators(void) {
    battle_menu_status_panel_indicator_prims_t* prims;
    native_thread_t* thread;
    native_thread_t* descriptor;
    s32 active;
    s32 i;
    s32 j;
    s32 k;
    s32 flip;
    s16 y;

    active = 0;
    if (require_thread_is_running(8) != 0) {
        if (g_battle_threads[8].task_id == NATIVE_THREAD_TASK_UNIT_STATUS_BANNER) {
            thread = (native_thread_t*)g_battle_threads[8].function_parameter_1;
            active = (thread->function_parameter_1 == 0) * 8;
        }
    }
    if (require_thread_is_running(7) != 0) {
        if (g_battle_threads[7].task_id == NATIVE_THREAD_TASK_UNIT_STATUS_BANNER) {
            descriptor = (native_thread_t*)g_battle_threads[7].function_parameter_1;
            if (descriptor->function_parameter_1 == 0) {
                active = 7;
                thread = descriptor;
            }
        }
    }
    if (active != 0) {
        if ((thread->function_parameter_2 == 0x90) && !(thread->function_parameter_3 & 0x80)) {
            g_require_menu_indicator_state[0] = 2;
            g_require_menu_indicator_state[1] = 1;
            goto draw_panels;
        } else if ((thread->function_parameter_2 == 0) && !(thread->function_parameter_3 & 0x80)) {
            g_require_menu_indicator_state[0] = 1;
            g_require_menu_indicator_state[1] = 2;
            goto draw_panels;
        }
    }
    if (g_require_menu_indicator_state[0] == 1) {
        g_require_menu_indicator_state[0] = 2;
    }
    if (g_require_menu_indicator_state[1] == 1) {
        g_require_menu_indicator_state[1] = 2;
    }
draw_panels:
    for (i = 0; i < 2; i++) {
        flip = g_require_menu_indicator_packet_index[i] ^ 1;
        g_require_menu_indicator_packet_index[i] = flip;
        prims = &g_require_thread_indicator_packets[i][flip];
        SetDrawMode(&prims->mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100) & 0xffff, &g_require_gfx_draw_area_template);
        SetTile(&prims->frame);
        prims->frame.r0 = 0;
        prims->frame.g0 = 0;
        prims->frame.b0 = 0;
        SetSemiTrans(&prims->frame, 1);
        y = 0xb2;
        if (i == 0) {
            y = 0x23;
        }
        prims->frame.x0 = 0x80;
        prims->frame.y0 = y;
        prims->frame.w = 0x100;
        prims->frame.h = 0x32;
        for (j = 0; j < 9; j++) {
            SetTile(&prims->top[j]);
            prims->top[j].r0 = 0;
            prims->top[j].g0 = 0;
            prims->top[j].b0 = 0;
            SetSemiTrans(&prims->top[j], 1);
            y = 0xa9;
            prims->top[j].w = 0x100;
            prims->top[j].h = 1;
            if (i == 0) {
                y = 0x1a;
            }
            prims->top[j].x0 = 0x80;
            prims->top[j].y0 = y + j;
            SetTile(&prims->bottom[j]);
            prims->bottom[j].r0 = 0;
            prims->bottom[j].g0 = 0;
            prims->bottom[j].b0 = 0;
            SetSemiTrans(&prims->bottom[j], 1);
            y = 0xe4;
            prims->bottom[j].w = 0x100;
            prims->bottom[j].h = 1;
            if (i == 0) {
                y = 0x55;
            }
            prims->bottom[j].x0 = 0x80;
            prims->bottom[j].y0 = y + j;
        }
        if (g_require_menu_indicator_state[i] == 0) {
            prims->frame.r0 = 0;
            prims->frame.g0 = 0;
            prims->frame.b0 = 0;
        } else {
            if (g_require_menu_indicator_state[i] == 1) {
                if (g_require_menu_indicator_brightness[i] < 0x78) {
                    g_require_menu_indicator_brightness[i] += 0xc;
                }
            }
            if (g_require_menu_indicator_state[i] == 2) {
                if (g_require_menu_indicator_brightness[i] != 0) {
                    g_require_menu_indicator_brightness[i] -= 0xc;
                } else {
                    g_require_menu_indicator_state[i] = 0;
                }
            }
            prims->frame.r0 = (u8)g_require_menu_indicator_brightness[i];
            prims->frame.g0 = (u8)g_require_menu_indicator_brightness[i];
            prims->frame.b0 = (u8)g_require_menu_indicator_brightness[i];
            /* fade_work holds the edge brightness that steps down across the gradient. */
            g_require_menu_indicator_fade_work[i] = g_require_menu_indicator_brightness[i];
            for (j = 0; j < 9; j++) {
                if (g_require_menu_indicator_fade_work[i] != 0) {
                    g_require_menu_indicator_fade_work[i] -= 0xc;
                }
                k = 8 - j;
                (k + prims->top)->r0 = (u8)g_require_menu_indicator_fade_work[i];
                (k + prims->top)->g0 = (u8)g_require_menu_indicator_fade_work[i];
                (k + prims->top)->b0 = (u8)g_require_menu_indicator_fade_work[i];
                prims->bottom[j].r0 = (u8)g_require_menu_indicator_fade_work[i];
                prims->bottom[j].g0 = (u8)g_require_menu_indicator_fade_work[i];
                prims->bottom[j].b0 = (u8)g_require_menu_indicator_fade_work[i];
                battle_gfx_draw_or_append_gpu_primitive((k + prims->top));
                battle_gfx_draw_or_append_gpu_primitive(&prims->bottom[j]);
            }
            battle_gfx_draw_or_append_gpu_primitive(&prims->frame);
            battle_gfx_draw_or_append_gpu_primitive(&prims->mode);
        }
    }
}
