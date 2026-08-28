#include "fft/battle.h"
#include "fft/battle_text.h"

/* Message window layout; only the fields written here are named. */
typedef struct battle_message_window_layout {
    u8 _unknown_00[4];
    s16 width_04;
    s16 height_06;
    s16 x; /* 0x08; centred on 0x102 */
    u8 _unknown_0a[2];
    s16 width_0c;
    s16 height_0e;
    u8 _unknown_10[4];
    s16 width_14;
    s16 height_16;
    u8 _unknown_18[4];
    s16 text_id; /* 0x1c */
} battle_message_window_layout_t;

/*
 * Size a message window to its text entry.
 *
 * Measures the text, optionally widens single-line messages to eight columns
 * (reporting (8 - width) * 5 extra display frames), pads the width to a
 * multiple of 4 plus 0x18 border, and centres the window on x = 0x102.
 */
void battle_text_layout_message_window(
    battle_message_window_layout_t* window, s16* width, s16* height, s32* extra_frames, s32 pad_short) {
    u8* text;

    text = battle_text_init_entry(window->text_id);
    *extra_frames = 0;
    battle_text_measure_pixels(width, height, text);
    if (pad_short != 0 && *width < 8 && *height == 1) {
        *extra_frames = (8 - *width) * 5;
        *width = 8;
    }
    *width += 0x18;
    *width += *width & 3;
    *height = *height * 16 + 16;
    window->x = 0x102 - (*width >> 1);
    window->width_0c = window->width_04 = window->width_14 = *width;
    window->height_0e = window->height_06 = window->height_16 = *height;
}
