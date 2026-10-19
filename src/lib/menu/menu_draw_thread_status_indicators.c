/* The two fading translucent letterbox panels of the event dialog overlay. */

#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern menu_gradient_panel_prims_t g_menu_thread_indicator_packets[2][2];

static s32 g_menu_indicator_packet_index[2] = { 0 };
static s32 g_menu_indicator_brightness[2] = { 0 };
static s32 g_menu_indicator_fade_work[2] = { 0 };
static s32 g_menu_indicator_state[2] = { 0 };

/* Build and submit the two translucent letterbox panels of the event dialogue
 * overlay.
 *
 * Banner tasks in slots 8 and 7 request a panel when display->position.x is zero;
 * display->position.y selects which of the two opens. Each panel fades its
 * gradient in by 0xc per frame up to 0x78 and back out again, alternating
 * between two primitive buffers so the previous frame stays queued. The
 * EQUIP twin watches threads 13 and 14.
 */
void menu_draw_status_panel_backdrops(void) {
    menu_gradient_panel_prims_t* prims;
    unit_panel_thread_t* display;
    unit_panel_thread_t* descriptor;
    s32 active = 0;
    s32 i;
    s32 j;
    s32 k;
    s32 flip;
    s16 y;

    if (menu_thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) != 0) {
        if (g_threads[THREAD_SCREEN_SLOT_UNIT_STATUS].task_id == THREAD_TASK_UNIT_STATUS_BANNER) {
            display = (unit_panel_thread_t*)g_threads[THREAD_SCREEN_SLOT_UNIT_STATUS].function_parameter_1;
            active = display->position.x == 0 ? 8 : 0;
        }
    }
    if (menu_thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) != 0) {
        if (g_threads[THREAD_SCREEN_SLOT_COMPARISON_STATUS].task_id == THREAD_TASK_UNIT_STATUS_BANNER) {
            descriptor = (unit_panel_thread_t*)g_threads[THREAD_SCREEN_SLOT_COMPARISON_STATUS].function_parameter_1;
            if (descriptor->position.x == 0) {
                active = 7;
                display = descriptor;
            }
        }
    }
    /* Both requests jump past the fade-out updates; an if/else-if/else form adds two instructions. */
    if (active != 0) {
        if ((display->position.y == 0x90) && !(display->flags & MENU_STATUS_PANEL_HIDDEN)) {
            g_menu_indicator_state[0] = MENU_INDICATOR_STATE_FADING_OUT;
            g_menu_indicator_state[1] = MENU_INDICATOR_STATE_FADING_IN;
            goto draw_panels;
        } else if ((display->position.y == 0) && !(display->flags & MENU_STATUS_PANEL_HIDDEN)) {
            g_menu_indicator_state[0] = MENU_INDICATOR_STATE_FADING_IN;
            g_menu_indicator_state[1] = MENU_INDICATOR_STATE_FADING_OUT;
            goto draw_panels;
        }
    }
    if (g_menu_indicator_state[0] == MENU_INDICATOR_STATE_FADING_IN) {
        g_menu_indicator_state[0] = MENU_INDICATOR_STATE_FADING_OUT;
    }
    if (g_menu_indicator_state[1] == MENU_INDICATOR_STATE_FADING_IN) {
        g_menu_indicator_state[1] = MENU_INDICATOR_STATE_FADING_OUT;
    }
draw_panels:
    for (i = 0; i < 2; i++) {
        flip = g_menu_indicator_packet_index[i] ^ 1;
        g_menu_indicator_packet_index[i] = flip;
        prims = &g_menu_thread_indicator_packets[i][flip];
        SetDrawMode(&prims->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_menu_draw_area_template);
        SetTile(&prims->center);
        setRGB0(&prims->center, 0, 0, 0);
        SetSemiTrans(&prims->center, 1);
        y = 0xb2;
        if (i == 0) {
            y = 0x23;
        }
        setXY0(&prims->center, 0x80, y);
        setWH(&prims->center, 0x100, 0x32);
        for (j = 0; j < 9; j++) {
            SetTile(&prims->top[j]);
            setRGB0(&prims->top[j], 0, 0, 0);
            SetSemiTrans(&prims->top[j], 1);
            y = 0xa9;
            setWH(&prims->top[j], 0x100, 1);
            if (i == 0) {
                y = 0x1a;
            }
            setXY0(&prims->top[j], 0x80, y + j);
            SetTile(&prims->bottom[j]);
            setRGB0(&prims->bottom[j], 0, 0, 0);
            SetSemiTrans(&prims->bottom[j], 1);
            y = 0xe4;
            setWH(&prims->bottom[j], 0x100, 1);
            if (i == 0) {
                y = 0x55;
            }
            setXY0(&prims->bottom[j], 0x80, y + j);
        }
        if (g_menu_indicator_state[i] == MENU_INDICATOR_STATE_HIDDEN) {
            setRGB0(&prims->center, 0, 0, 0);
        } else {
            if (g_menu_indicator_state[i] == MENU_INDICATOR_STATE_FADING_IN) {
                if (g_menu_indicator_brightness[i] < 0x78) {
                    g_menu_indicator_brightness[i] += 0xc;
                }
            }
            if (g_menu_indicator_state[i] == MENU_INDICATOR_STATE_FADING_OUT) {
                if (g_menu_indicator_brightness[i] != 0) {
                    g_menu_indicator_brightness[i] -= 0xc;
                } else {
                    g_menu_indicator_state[i] = MENU_INDICATOR_STATE_HIDDEN;
                }
            }
            setRGB0(&prims->center, g_menu_indicator_brightness[i], g_menu_indicator_brightness[i],
                g_menu_indicator_brightness[i]);
            /* fade_work holds the edge brightness that steps down across the gradient. */
            g_menu_indicator_fade_work[i] = g_menu_indicator_brightness[i];
            for (j = 0; j < 9; j++) {
                if (g_menu_indicator_fade_work[i] != 0) {
                    g_menu_indicator_fade_work[i] -= 0xc;
                }
                k = 8 - j;
                (k + prims->top)->r0 = g_menu_indicator_fade_work[i];
                (k + prims->top)->g0 = g_menu_indicator_fade_work[i];
                (k + prims->top)->b0 = g_menu_indicator_fade_work[i];
                setRGB0(&prims->bottom[j], g_menu_indicator_fade_work[i], g_menu_indicator_fade_work[i],
                    g_menu_indicator_fade_work[i]);
                gfx_draw_or_append_gpu_primitive((k + prims->top));
                gfx_draw_or_append_gpu_primitive(&prims->bottom[j]);
            }
            gfx_draw_or_append_gpu_primitive(&prims->center);
            gfx_draw_or_append_gpu_primitive(&prims->draw_mode);
        }
    }
}
