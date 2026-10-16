/* Shop background TIM loading, text section binding, resident data and tutorial file loading. */

#include "fft/main_file.h"
#include "fft/menu.h"
#include "fft/menu_text.h"
#include "fft/world_script.h"
#include "fft/world_shop.h"
#include "fft/world_text.h"
#include "psx/types.h"

/* Sector and size pairs; flat 32-bit value access preserves the original addressing. */
static s32 g_world_script_tutorial_message_files[7 * 2]
    = { 72298, 4096, 72301, 6144, 72305, 6144, 72309, 4096, 72312, 6144, 72316, 2048, 72318, 2048 };

static s32 g_world_script_tutorial_script_files[7 * 2]
    = { 72297, 2048, 72300, 2048, 72304, 2048, 72308, 2048, 72311, 2048, 72315, 2048, 72317, 2048 };

/* Load shop background image `index` to the heap; the table holds a sector
 * and a size per image. */
u32* world_shop_load_background_tim(s32 index) {
    return main_file_load_checked_to_heap(
        g_world_shop_background_tim_files[index * 2], g_world_shop_background_tim_files[(index * 2) + 1]);
}

/* Points one of the text section pointers at its resident text data.
 *
 * No path returns a value; world_card_build_save_slot_description uses the
 * bound address the store leaves in $v0. */
u8* world_text_bind_section_data(u32 index) {
    switch (index) {
    case WORLD_TEXT_DATA_SKILLSET_NAMES:
        g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES] = g_world_text_skillset_names_data;
        break;
    case WORLD_TEXT_DATA_JOB_NAMES:
        g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = g_world_text_job_names_data;
        break;
    case WORLD_TEXT_DATA_ABILITY_NAMES:
        g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES] = g_world_text_ability_names_data;
        break;
    case WORLD_TEXT_DATA_FORMATION_MESSAGES:
        g_text_section_pointers[TEXT_SECTION_FORMATION_MESSAGES] = g_world_text_formation_messages_data;
        break;
    case WORLD_TEXT_DATA_DISMISS_UNIT_REPLIES:
        g_text_section_pointers[WORLD_TEXT_SECTION_FORMATION_TEXT] = g_world_text_dismiss_unit_replies_data;
        break;
    case WORLD_TEXT_DATA_JOB_DESCRIPTIONS:
        g_text_section_pointers[WORLD_TEXT_SECTION_FORMATION_TEXT] = g_world_text_job_descriptions_data;
        break;
    case WORLD_TEXT_DATA_NAME_ENTRY_KEYBOARD:
        g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_world_text_name_entry_keyboard_data;
        break;
    case WORLD_TEXT_DATA_SHOP_DIALOGUE:
        g_text_section_pointers[WORLD_TEXT_SECTION_SHOP_DIALOGUE] = g_world_text_shop_dialogue_data;
        break;
    }
}

/* Return the resident text resource; index 6 has no data. */
u8* world_text_get_resident_data(world_text_data_e index) {
    return g_world_text_resident_data[index];
}

/* Load tutorial `index`'s (1-based) message text and command stream to the
 * heap and store their addresses in `record`. */
void world_script_load_tutorial_files(s32 index, world_tutorial_state_t* record) {
    s32 slot = index - 1;

    record->messages = main_file_load_checked_to_heap(
        g_world_script_tutorial_message_files[slot * 2], g_world_script_tutorial_message_files[(slot * 2) + 1]);
    record->script = main_file_load_checked_to_heap(
        g_world_script_tutorial_script_files[slot * 2], g_world_script_tutorial_script_files[(slot * 2) + 1]);
}
