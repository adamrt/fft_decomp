/* The two fading translucent letterbox panels behind the unit status banner. */

#include "fft/equipment.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Double-buffer index, current level, fade level and state, one pair each for
 * the two status panels. */
extern s32 g_equipment_indicator_buffer_index[2];
extern s32 g_equipment_indicator_brightness[2];
extern s32 g_equipment_indicator_edge_brightness[2];
extern s32 g_equipment_indicator_state[2];
extern menu_gradient_panel_prims_t g_equipment_thread_indicator_packets[2][2];

/*
 * Builds and submits the two translucent letterbox panels behind the unit status
 * banner: the top band (y 0x1a to 0x5d) and the bottom one (y 0xa9 to 0xec).
 *
 * A status banner thread (EQUIPMENT_THREAD_SLOT_UNIT_STATUS, or the comparison slot,
 * which wins) requests a panel while its display x is zero: at y 0 the top band
 * fades in, at y 0x90 the bottom one, and the other fades out. Each panel's
 * center fades by 0xc per frame up to 0x78 and back, its nine edge lines fading
 * further toward the outside, alternating between two primitive buffers so the
 * previous frame stays queued. Nothing in EQUIP calls it.
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

    if (menu_thread_is_running(EQUIPMENT_THREAD_SLOT_UNIT_STATUS) != 0) {
        if (g_threads[EQUIPMENT_THREAD_SLOT_UNIT_STATUS].task_id == THREAD_TASK_UNIT_STATUS_BANNER) {
            display = (unit_panel_thread_t*)g_threads[EQUIPMENT_THREAD_SLOT_UNIT_STATUS].function_parameter_1;
            active = display->position.x == 0 ? 13 : 0;
        }
    }
    if (menu_thread_is_running(EQUIPMENT_THREAD_SLOT_COMPARISON_STATUS) != 0) {
        if (g_threads[EQUIPMENT_THREAD_SLOT_COMPARISON_STATUS].task_id == THREAD_TASK_UNIT_STATUS_BANNER) {
            descriptor = (unit_panel_thread_t*)g_threads[EQUIPMENT_THREAD_SLOT_COMPARISON_STATUS].function_parameter_1;
            if (descriptor->position.x == 0) {
                active = 14;
                display = descriptor;
            }
        }
    }
    /* Both requests jump past the fade-out updates; the target shares one
     * state[1] store between them and the fall-through path. */
    if (active != 0) {
        if ((display->position.y == 0x90) && !(display->flags & MENU_STATUS_PANEL_HIDDEN)) {
            g_equipment_indicator_state[0] = MENU_INDICATOR_STATE_FADING_OUT;
            g_equipment_indicator_state[1] = MENU_INDICATOR_STATE_FADING_IN;
            goto draw_panels;
        } else if ((display->position.y == 0) && !(display->flags & MENU_STATUS_PANEL_HIDDEN)) {
            g_equipment_indicator_state[0] = MENU_INDICATOR_STATE_FADING_IN;
            g_equipment_indicator_state[1] = MENU_INDICATOR_STATE_FADING_OUT;
            goto draw_panels;
        }
    }
    if (g_equipment_indicator_state[0] == MENU_INDICATOR_STATE_FADING_IN) {
        g_equipment_indicator_state[0] = MENU_INDICATOR_STATE_FADING_OUT;
    }
    if (g_equipment_indicator_state[1] == MENU_INDICATOR_STATE_FADING_IN) {
        g_equipment_indicator_state[1] = MENU_INDICATOR_STATE_FADING_OUT;
    }
draw_panels:
    for (i = 0; i < 2; i++) {
        flip = g_equipment_indicator_buffer_index[i] ^ 1;
        g_equipment_indicator_buffer_index[i] = flip;
        prims = &g_equipment_thread_indicator_packets[i][flip];
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
        if (g_equipment_indicator_state[i] == MENU_INDICATOR_STATE_HIDDEN) {
            setRGB0(&prims->center, 0, 0, 0);
        } else {
            if (g_equipment_indicator_state[i] == MENU_INDICATOR_STATE_FADING_IN) {
                if (g_equipment_indicator_brightness[i] < 0x78) {
                    g_equipment_indicator_brightness[i] += 0xc;
                }
            }
            if (g_equipment_indicator_state[i] == MENU_INDICATOR_STATE_FADING_OUT) {
                if (g_equipment_indicator_brightness[i] != 0) {
                    g_equipment_indicator_brightness[i] -= 0xc;
                } else {
                    g_equipment_indicator_state[i] = MENU_INDICATOR_STATE_HIDDEN;
                }
            }
            setRGB0(&prims->center, g_equipment_indicator_brightness[i], g_equipment_indicator_brightness[i],
                g_equipment_indicator_brightness[i]);
            g_equipment_indicator_edge_brightness[i] = g_equipment_indicator_brightness[i];
            for (j = 0; j < 9; j++) {
                if (g_equipment_indicator_edge_brightness[i] != 0) {
                    g_equipment_indicator_edge_brightness[i] -= 0xc;
                }
                k = 8 - j;
                (k + prims->top)->r0 = g_equipment_indicator_edge_brightness[i];
                (k + prims->top)->g0 = g_equipment_indicator_edge_brightness[i];
                (k + prims->top)->b0 = g_equipment_indicator_edge_brightness[i];
                setRGB0(&prims->bottom[j], g_equipment_indicator_edge_brightness[i],
                    g_equipment_indicator_edge_brightness[i], g_equipment_indicator_edge_brightness[i]);
                gfx_draw_or_append_gpu_primitive(&prims->top[k]);
                gfx_draw_or_append_gpu_primitive(&prims->bottom[j]);
            }
            gfx_draw_or_append_gpu_primitive(&prims->center);
            gfx_draw_or_append_gpu_primitive(&prims->draw_mode);
        }
    }
}
