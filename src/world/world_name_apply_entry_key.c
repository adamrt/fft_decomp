#include "fft/world.h"
#include "psx/types.h"

/* Applies a name-entry key to the encoded name at g_world_name_entry_text and re-kerns it.
 *
 * The name is expanded to 14 character slots (0xD0-0xDF lead bytes form
 * two-byte characters, 0xFA is a space). Key -1 is backspace, -2 deletes at
 * the cursor, -3 inserts a space, -4/-5 move the cursor left/right, and any
 * other key overwrites the slot at the cursor and advances it. The slots are
 * re-encoded with trailing spaces trimmed before the terminator. */
void world_name_apply_entry_key(s16 key) {
    u16 chars[14];
    s32 i;
    s32 j;
    s32 slot;

    slot = 0xFA;
    for (i = 13; i >= 0; i--) {
        chars[i] = slot;
    }
    j = 0;
    slot = 0;
    while (g_world_name_entry_text[j] != TEXT_END_WAIT_FOR_CONFIRM) {
        if ((u8)(g_world_name_entry_text[j] + 0x30) >= 0x10) {
            chars[slot] = g_world_name_entry_text[j++];
        } else {
            chars[slot] = (g_world_name_entry_text[j] << 8) + g_world_name_entry_text[j + 1];
            j += 2;
        }
        slot++;
    }
    if (key == -1) {
        if (g_world_name_entry_cursor == 0) {
            return;
        }
        g_world_name_entry_cursor--;
        for (i = g_world_name_entry_cursor; i < 13; i++) {
            chars[i] = chars[i + 1];
        }
        chars[13] = TEXT_SPACE;
    } else if (key == -2) {
        for (i = g_world_name_entry_cursor; i < 13; i++) {
            chars[i] = chars[i + 1];
        }
        chars[13] = TEXT_SPACE;
    } else if (key == -3) {
        for (i = 13; g_world_name_entry_cursor < i; i--) {
            chars[i] = chars[i - 1];
        }
        chars[g_world_name_entry_cursor] = TEXT_SPACE;
    } else if (key == -4) {
        g_world_name_entry_cursor = g_world_name_entry_cursor != 0 ? g_world_name_entry_cursor - 1 : 0;
    } else if (key == -5) {
        g_world_name_entry_cursor = g_world_name_entry_cursor < 13 ? g_world_name_entry_cursor + 1 : 13;
    } else {
        chars[g_world_name_entry_cursor] = key;
        g_world_name_entry_cursor = g_world_name_entry_cursor < 13 ? g_world_name_entry_cursor + 1 : 13;
    }
    for (i = 0, j = 0; i < 14; i++) {
        if (chars[i] <= 0xCFFF) {
            g_world_name_entry_text[j++] = chars[i];
        } else {
            g_world_name_entry_text[j] = chars[i] >> 8;
            g_world_name_entry_text[j + 1] = chars[i];
            j += 2;
        }
    }
    g_world_name_entry_text[j] = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 0; g_world_name_entry_text[i] != TEXT_END_WAIT_FOR_CONFIRM; i++) { }
    for (i--; g_world_name_entry_text[i] == TEXT_SPACE; i--) { }
    g_world_name_entry_text[i + 1] = TEXT_END_WAIT_FOR_CONFIRM;
    world_text_kern_into_command_buffer(
        g_world_name_entry_text, g_world_name_entry_text_row_ids, g_world_name_entry_text_vram_rect, 0);
}
