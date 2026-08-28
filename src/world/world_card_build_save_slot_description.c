#include "fft/world.h"
#include "psx/types.h"

#define WORLD_CARD_SLOT_TEXT_TABLE g_world_card_slot_text_table

/* Build the encoded save-slot description text for WORLD's save screen.
 *
 * mode 1 copies the empty-slot caption; mode 0 formats leader name, job,
 * level, date and location out of the loaded save image. Other nonnegative
 * modes write only the terminator; negative modes leave the buffer untouched.
 *
 * Only the negative-mode exit explicitly returns a value. Other paths fall
 * through after writing the terminator, and the caller ignores the result.
 * An explicit return at the tail introduces an extra constant load.
 *
 * `world_gfx_bind_data_pointer` is declared void but leaves the bound table in
 * $v0, which the target passes straight on as the text table; the
 * function-pointer cast reproduces that without lying about the callee. */
s32 world_card_build_save_slot_description(s32 mode, u8* slot_graphic) {
    s32 index;
    u8* source;

    if (mode == 1) {
        source = world_text_find_entry_by_index(WORLD_CARD_SLOT_TEXT_TABLE, 14, 1);
        while ((*slot_graphic++ = *source++) != 0xFE) { }
    } else if (mode < 0) {
        return 0;
    } else if (mode == 0) {
        index = 0;
        while (g_world_load_work_buffer->name[index] != 0xFE) {
            *slot_graphic++ = g_world_load_work_buffer->name[index];
            index++;
        }
        *slot_graphic++ = 0xE7;
        *slot_graphic++ = 0x58;
        source = world_text_find_entry_by_index(
            ((u8 * (*)(u32)) world_gfx_bind_data_pointer)(1), g_world_load_work_buffer->job_id, 1);
        while (*source != 0xFE) {
            *slot_graphic++ = *source++;
        }
        *slot_graphic++ = 0xE7;
        *slot_graphic++ = 0xAC;
        source = world_text_find_entry_by_index(WORLD_CARD_SLOT_TEXT_TABLE, 13, 1);
        while (*source != 0xFE) {
            *slot_graphic++ = *source++;
        }
        *slot_graphic++ = g_world_load_work_buffer->level / 10;
        *slot_graphic++ = g_world_load_work_buffer->level % 10;
        *slot_graphic++ = 0xFE;
        source = world_text_find_entry_by_index(WORLD_CARD_SLOT_TEXT_TABLE, g_world_load_work_buffer->month, 1);
        while (*source != 0xFE) {
            *slot_graphic++ = *source++;
        }
        *slot_graphic++ = 0xFA;
        *slot_graphic++ = g_world_load_work_buffer->day / 10;
        *slot_graphic++ = g_world_load_work_buffer->day % 10;
        *slot_graphic++ = 0xE7;
        *slot_graphic++ = 0x58;
        source
            = world_text_find_entry_by_index((u8*)g_world_text_location_names, g_world_load_work_buffer->location, 1);
        while ((*slot_graphic++ = *source++) != 0xFE) { }
    }
    *slot_graphic = 0xFE;
}
