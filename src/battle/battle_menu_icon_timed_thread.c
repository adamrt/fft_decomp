#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern void battle_update_menu_cursor_primitives(
    world_menu_icon_thread_param_t* param, world_menu_icon_sprites_t* record, s32 frame, s32 cursor);
struct battle_menu_frame_primitives;
extern void battle_menu_submit_frame_primitives(struct battle_menu_frame_primitives* menu);

struct battle_message_window_layout;
extern void battle_text_layout_message_window(
    struct battle_message_window_layout* window, s16* width, s16* height, s32* extra_frames, s32 pad_short);
struct menu_frame_sprites;
extern void battle_menu_configure_frame_cluts(struct menu_frame_sprites* icons);

/* Timed menu icon thread: shows one text entry with alternating sprite
 * records until input arrives or the g_battle_text_message_duration_frames frame budget (scaled by the
 * event speed) runs out. Bit 8 of the budget suppresses cancel handling
 * until the budget expires. The thread-id check reads an unset local in the
 * original.
 *
 * BATTLE twin of world_menu_icon_timed_thread. */
void battle_menu_icon_timed_thread(void) {
    RECT rect;
    battle_menu_window_record_t records[2];
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    s32 hold;
    battle_menu_window_record_t* record;
    s16 width;
    s16 height;
    s32 pad;
    s32 unset;

    hold = 0;
    if (g_battle_text_message_duration_frames >= 0x100) {
        hold = 1;
        g_battle_text_message_duration_frames &= 0xFF;
    }
    i = 0;
    param = (world_menu_icon_thread_param_t*)battle_thread_get_current_parameter_1();
    battle_text_layout_message_window((struct battle_message_window_layout*)param, &width, &height, &pad, 1);
    battle_menu_build_window_sprites(
        (battle_menu_window_header_t*)&rect, (battle_menu_window_spec_t*)param, &records[0]);
    battle_copy_bytes(&records[1], &records[0], 0x7C);
    buffer = battle_menu_build_and_upload_window_frame_image(width, height, &rect, 1);
    g_menu_text_state.stride = width;
    /* The target passes x and y as full words; the s16 prototype narrows them. */
    ((void (*)(s32, s32))battle_menu_set_text_origin)(pad + 8, 9);
    battle_menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.origin_x);
    LoadImage(&rect, buffer);
    for (;; i++) {
        battle_thread_yield();
        if (i == 0) {
            battle_menu_free_memory(buffer);
        }
        if (g_battle_text_message_duration_frames < i * g_battle_event_speed) {
            hold = 0;
            g_battle_script_event_input = PSX_PAD_CIRCLE;
            g_battle_sound_suppressed = 1;
        }
        if (battle_menu_should_close_thread(&g_battle_script_event_input) != 0) {
            break;
        }
        if (unset >= 0) {
            g_battle_current_thread_id--;
            battle_menu_configure_frame_cluts((struct menu_frame_sprites*)&records[i & 1]);
            g_battle_current_thread_id++;
        } else {
            battle_menu_configure_frame_cluts((struct menu_frame_sprites*)&records[i & 1]);
        }
        record = &records[i & 1];
        battle_update_menu_cursor_primitives(param, (world_menu_icon_sprites_t*)record, i, -1);
        if (hold == 0) {
            battle_menu_handle_action(param, 0);
            battle_handle_menu_cancel_input(param);
        }
        battle_menu_submit_frame_primitives((struct battle_menu_frame_primitives*)record);
    }
    g_battle_menu_panel_fade_intensity = 0;
    g_battle_sound_suppressed = 0;
    battle_thread_yield();
    if (battle_thread_get_current_parameter_2() == 0) {
        battle_thread_exit_current();
    }
}
