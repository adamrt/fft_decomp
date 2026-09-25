#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct world_menu_confirm_entry world_menu_confirm_entry_t;
extern void world_menu_handle_entry_confirm(world_menu_confirm_entry_t* param, s32 value);
typedef struct world_menu_icon_prims world_menu_icon_prims_t;
extern void world_menu_submit_icon_primitives(world_menu_icon_prims_t* menu);

/* Timed menu icon thread: shows one text entry with alternating sprite
 * records until input arrives or the g_world_text_message_duration_frames frame budget (scaled by the
 * event speed) runs out. Bit 8 of the budget suppresses cancel handling
 * until the budget expires. The thread-id check reads an unset local in the
 * original. */
void world_menu_icon_timed_thread(void) {
    RECT rect;
    world_menu_icon_record_t records[2];
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    s32 hold;
    world_menu_icon_record_t* record;
    s16 width;
    s16 height;
    s32 pad;
    s32 unset;

    hold = 0;
    if (g_world_text_message_duration_frames >= 0x100) {
        hold = 1;
        g_world_text_message_duration_frames &= 0xFF;
    }
    i = 0;
    param = world_thread_get_current_parameter_1();
    world_menu_size_entry_to_text((world_menu_entry_t*)param, &width, &height, &pad, 1);
    world_menu_build_icon_record(&rect, param, &records[0]);
    world_script_copy_bytes(&records[1], &records[0], 0x7C);
    buffer = world_menu_build_and_upload_window_frame_image(width, height, &rect, 1);
    g_world_menu_text_state.stride = width;
    /* The target passes both coordinates without the s16 sign extension. */
    ((void (*)(s32, s32))world_menu_set_text_origin)(pad + 8, 9);
    world_menu_display_text_entry(param->text_id, buffer, &g_world_menu_text_state.origin_x);
    LoadImage(&rect, buffer);
    for (;; i++) {
        world_thread_yield();
        if (i == 0) {
            world_menu_free_memory(buffer);
        }
        if (g_world_text_message_duration_frames < i * g_world_event_speed) {
            hold = 0;
            g_world_menu_new_button_input = PSX_PAD_CIRCLE;
            g_world_menu_sound_muted = 1;
        }
        if (world_menu_check_thread_completion(&g_world_menu_new_button_input) != 0) {
            break;
        }
        if (unset >= 0) {
            g_world_thread_current_id--;
            world_menu_select_icon_cluts(&records[i & 1].base);
            g_world_thread_current_id++;
        } else {
            world_menu_select_icon_cluts(&records[i & 1].base);
        }
        record = &records[i & 1];
        world_menu_update_icon_cursor_sprites(param, &record->base, i, -1);
        if (hold == 0) {
            world_menu_handle_entry_confirm((world_menu_confirm_entry_t*)param, 0);
            world_menu_cancel_thread_group((world_menu_cancel_context_t*)param);
        }
        world_menu_submit_icon_primitives((world_menu_icon_prims_t*)record);
    }
    g_world_menu_panel_fade_intensity = 0;
    g_world_menu_sound_muted = 0;
    world_thread_yield();
    if (world_thread_get_current_parameter_2() == 0) {
        world_thread_exit_current();
    }
}
