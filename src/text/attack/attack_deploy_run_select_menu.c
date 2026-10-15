#include "fft/battle_script.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void attack_deploy_run_select_menu(void) {
    POLY_FT4 left[2];
    POLY_FT4 right[2];
    s32 i;
    s32 selected;
    u32* input;
    s32 parent_id;
    thread_t* thread;
    thread_t* parent;
    help_navigation_record_t* node;
    help_navigation_record_t* nodes;
    s32 x;
    s32 y;
    s32 text_id;
    u32 buttons;
    s32 bob;
    s32 buffer;
    s32 u;
    s32 u_mid;
    s32 shadow_offset;

    u_mid = 0xb8;
    g_attack_panel_status_animation[0] = 1;
    g_attack_panel_status_animation[1] = 1;
    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    for (i = 0; i < 2; i++) {
        menu_init_default_poly_ft4(&left[i]);
        SetSemiTrans(&left[i], 0);
        left[i].tpage = GetTPage(0, 0, 0x3c0, 0x100);
        left[i].clut = 0x7d7c;
        menu_init_default_poly_ft4(&right[i]);
        SetSemiTrans(&right[i], 1);
        right[i].tpage = GetTPage(0, 2, 0x3c0, 0x100);
        right[i].clut = 0x7dbc;
    }
    if (thread_get_current_parameter_1_variant() != 0) {
        g_attack_deploy_select_menu_nodes[5].destination[1] = 5;
        g_attack_deploy_select_menu_nodes[9].destination[1] = 9;
        g_attack_deploy_select_menu_nodes[10].destination[1] = 10;
    } else {
        g_attack_deploy_select_menu_nodes[5].destination[1] = 12;
        g_attack_deploy_select_menu_nodes[9].destination[1] = 17;
        g_attack_deploy_select_menu_nodes[10].destination[1] = 17;
    }
    g_attack_deploy_select_menu_nodes[8].help_base = g_attack_deploy_selected_roster_index * 11 + 10 - 0x3000;
    for (i = 0; i < 10; i++) {
        g_attack_deploy_select_menu_nodes[i + 18].help_base = g_attack_deploy_selected_roster_index * 11 - 0x3000 + i;
    }
    selected = 0;
    nodes = g_attack_deploy_select_menu_nodes;
    g_text_section_pointers[TEXT_SECTION_DEPLOYMENT_UNIT_HELP] = g_attack_deploy_unit_text_section;
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_attack_deploy_help_text_section;
    parent_id = g_thread_current_id - 1;
    input = script_get_controller_input_pointer(0);
    i = 0;
    thread = &g_threads[g_thread_current_id];
    parent = &g_threads[parent_id];
    while (1) {
        if (parent->is_running == 0) {
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                g_menu_input_lock_mode = 0;
            }
            g_menu_input_lock_mode = 0;
            if (thread->task_words[1] != 0) {
                thread->task_words[1] = 0;
                thread->task_words[0] = 0;
            }
            node = &nodes[selected];
            y = node->y;
            x = node->x;
            if (thread_find_running_by_task(THREAD_TASK_DEBUG_UNIT_EDITOR) != 0) {
                y += 8;
            }
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                y += 8;
            }
            buttons = *input;
            y += 8;
            if (buttons & PSX_PAD_CROSS) {
                g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                *input = 0;
                break;
            }
            if (buttons & PSX_PAD_CIRCLE) {
                if (node->value != 0) {
                    text_id = node->help_base + *node->value;
                    if (node->help_base == -0x6800 && g_menu_selected_unit_fields.state.generic_monster != 0) {
                        text_id = *node->value | 0x7800;
                        if ((*node->value & 0x7ff) == 0x7ff || (*node->value & 0x7ff) == 0) {
                            *input = 0;
                        } else {
                            g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                            thread_start(parent_id, text_run_dialog_thread_variant);
                            thread_set_parameters(parent_id, 0x3b, text_id, 0);
                        }
                    } else if (node->help_base == 0x7800 && (u16)*node->value - 1 >= 0x301U) {
                        *input = 0;
                    } else if (node->help_base == 0x6800 && (u16)*node->value - 1 >= 0x101U) {
                        *input = 0;
                    } else if (node->help_base == -0x6800 && (u16)*node->value - 1 >= 0x101U) {
                        *input = 0;
                    } else {
                        g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                        thread_start(parent_id, text_run_dialog_thread_variant);
                        thread_set_parameters(parent_id, 0x3b, text_id, 0);
                    }
                } else {
                    text_id = node->help_base;
                    g_thread_call_target = (void (*)(void))text_find_entry;
                    if ((*(u8*)thread_call_on_main_stack(text_id) & 0xfe) != 0xfe) {
                        g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                        thread_start(parent_id, text_run_dialog_thread_variant);
                        thread_set_parameters(parent_id, 0x3b, text_id, 0);
                    }
                }
            } else if (buttons & PSX_PAD_DPAD_MASK) {
                text_id = 0;
                if (!(buttons & PSX_PAD_UP)) {
                    if (buttons & PSX_PAD_DOWN) {
                        text_id = 1;
                    } else {
                        text_id = 3;
                        if (buttons & PSX_PAD_RIGHT) {
                            text_id = 2;
                        }
                    }
                }
                if (selected != node->destination[text_id]) {
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
                selected = node->destination[text_id];
            }
        } else {
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                g_menu_input_lock_mode = 1;
            }
            g_menu_input_lock_mode = 1;
            if (thread->task_words[1] == 0) {
                thread->task_words[1] = 2;
                thread->task_words[0] = 0;
            }
        }
        if (thread->task_words[1] != 0) {
            bob = menu_get_cursor_bob_offset(1);
        } else {
            bob = menu_get_cursor_bob_offset(0);
        }
        thread->task_words[0] += g_script_event_speed;
        bob -= 4;
        buffer = i & 1;
        if (node->vertical_cursor != 0) {
            u = 0xa8;
            left[buffer].u0 = u;
            left[buffer].u1 = u;
            left[buffer].u2 = u_mid;
            left[buffer].u3 = u_mid;
            left[buffer].v0 = 0;
            left[buffer].v1 = 0x10;
            left[buffer].v2 = 0;
            left[buffer].v3 = 0x10;
            u = 0xb8;
            right[buffer].u0 = u;
            right[buffer].u1 = u;
            right[buffer].u2 = 0xc8;
            right[buffer].u3 = 0xc8;
            right[buffer].v0 = 0;
            right[buffer].v1 = 0x10;
            right[buffer].v2 = 0;
            right[buffer].v3 = 0x10;
            setXY4(&left[buffer], x, y + bob, x + 0x10, y + bob, x, y + bob + 0x10, x + 0x10, y + bob + 0x10);
            (right + buffer)->x0 = x + 2;
            (right + buffer)->y0 = y + bob + 2;
            (right + buffer)->x1 = x + 0x12;
            (right + buffer)->y1 = y + bob + 2;
            (right + buffer)->x2 = x + 2;
            (right + buffer)->y2 = y + bob + 0x12;
            (right + buffer)->x3 = x + 0x12;
            (right + buffer)->y3 = y + bob + 0x12;
        } else {
            u = 0xa8;
            (left + buffer)->u0 = u;
            (left + buffer)->v0 = 0;
            (left + buffer)->u1 = u_mid;
            (left + buffer)->v1 = 0;
            (left + buffer)->u2 = 0xa8;
            (left + buffer)->v2 = 0x10;
            (left + buffer)->u3 = u_mid;
            (left + buffer)->v3 = 0x10;
            u = 0xb8;
            (right + buffer)->u0 = u;
            (right + buffer)->v0 = 0;
            (right + buffer)->u1 = 0xc8;
            (right + buffer)->v1 = 0;
            (right + buffer)->u2 = u_mid;
            (right + buffer)->v2 = 0x10;
            (right + buffer)->u3 = 0xc8;
            (right + buffer)->v3 = 0x10;
            (left + buffer)->x0 = x + bob;
            (left + buffer)->y0 = y;
            (left + buffer)->x1 = x + bob + 0x10;
            (left + buffer)->y1 = y;
            (left + buffer)->x2 = x + bob;
            (left + buffer)->y2 = y + 0x10;
            (left + buffer)->x3 = x + bob + 0x10;
            (left + buffer)->y3 = y + 0x10;
            shadow_offset = bob + 2;
            (right + buffer)->x0 = x + shadow_offset;
            (right + buffer)->y0 = y + 2;
            (right + buffer)->x1 = x + shadow_offset + 0x10;
            (right + buffer)->y1 = y + 2;
            (right + buffer)->x2 = x + shadow_offset;
            (right + buffer)->y2 = y + 0x12;
            (right + buffer)->x3 = x + shadow_offset + 0x10;
            (right + buffer)->y3 = y + 0x12;
        }
        if (thread->task_words[1] != 0) {
            left[buffer].clut = 0x7dfc;
            right[buffer].clut = 0x7e3c;
        } else {
            left[buffer].clut = 0x7d7c;
            right[buffer].clut = 0x7dbc;
        }
        gfx_draw_or_append_gpu_primitive(&left[buffer]);
        gfx_draw_or_append_gpu_primitive(&right[buffer]);
        thread_wait_frames(1);
        i++;
    }
    g_attack_panel_status_animation[0] = 0;
    g_attack_panel_status_animation[1] = 0;
    thread_exit_current();
}
