#include "fft/main_gfx.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/open.h"
#include "fft/text.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef struct open_new_game_state {
    /* 0x00 */ u8 unused_00[0x10];
    /* 0x10 */ s32 step;
    /* 0x14 */ u8 unused_14[0x0c];
    /* 0x20 */ s32 name_text_id;
} open_new_game_state_t;

typedef struct open_party_name_block {
    /* 0x00 */ char name[16];
} open_party_name_block_t;

void open_title_step_new_game_start(open_new_game_state_t* state) {
    RECT rect;
    open_party_name_block_t* name;
    party_data_t* party;

    if (state->step == 5) {
        if (g_open_gfx_overlay_fade.flags & 4) {
            return;
        }
        main_sound_unload_current_scenario_music();
        g_open_current_controller_index -= 1;
        open_menu_start_music_test_controller();
        return;
    }

    if (state->step == 4) {
        open_title_init_new_game_party(0, 0);
        world_menu_load_text_1c18();

        if (g_main_gfx_display_buffer_index != 0) {
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0xF0;
        } else {
            rect.x = 0;
            rect.y = 0xF0;
            rect.w = 0x100;
            rect.h = 0xF0;
        }
        StoreImage(&rect, (u32*)g_open_file_destination);
        DrawSync(0);

        if (g_main_gfx_display_buffer_index == 0) {
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0xF0;
        } else {
            rect.x = 0;
            rect.y = 0xF0;
            rect.w = 0x100;
            rect.h = 0xF0;
        }
        LoadImage(&rect, (u32*)g_open_file_destination);
        DrawSync(0);

        g_open_file_current_openbk_image_id = -1;
        name = (open_party_name_block_t*)world_name_run_entry_screen(
            world_text_find_entry(TEXT_ID_UNIT_NAME_SPECIAL_BASE), (u32*)g_open_file_destination);
        party = main_party_get_data_pointer(0);
        *(open_party_name_block_t*)party->name = *name;
        g_open_current_controller_index -= 1;
        open_birthday_push_date_controller();
        return;
    }

    if (g_open_gfx_overlay_fade.flags & 4) {
        return;
    }
    if (open_gfx_update_transition_sequence() != 0) {
        return;
    }
    open_noop_8006d7ec();
    state->step = 4;
}
