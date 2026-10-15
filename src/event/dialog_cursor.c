/* The dialog text cursor: advancing it with separate backreference state and printing FNT values. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/open.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

u8* text_advance_cursor_with_separate_backreference_state(s32* remaining_bytes, u8* cursor, u8** return_cursor) {
    text_backref_t* backref;
    s32 remaining;
    s32 scratch;
    u8 command;

    remaining = *remaining_bytes;
    cursor += 1;
    if (remaining > 0) {
        remaining -= 1;
        *remaining_bytes = remaining;
        if (remaining == 0) {
            cursor = *return_cursor;
            *return_cursor = (u8*)-1;
        }
    }
    command = cursor[0];
    if (((command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST) && ((command & 0xF) < 4)) {
        *return_cursor = cursor + 3;
        backref = (text_backref_t*)cursor;
        scratch = backref->length_high << 3;
        *remaining_bytes = scratch + backref->length_low + 4;
        scratch = backref->offset_high;
        scratch = scratch * 0xFE + backref->offset_low;
        cursor -= scratch;
    }
    return cursor;
}

/* Append value and value + 1 to the active Psy-Q font stream. */
void text_print_fnt_value_and_next(s32 value) {
    FntPrint(g_text_decimal_format, value);
    value++;
    FntPrint(g_text_decimal_format, value);
}
