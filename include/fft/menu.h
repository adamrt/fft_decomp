#ifndef FFT_MENU_H
#define FFT_MENU_H

#include "fft/menu_types.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Six menu buffers, allocated/released as three adjacent pairs. The BATTLE
 * allocator at 0x8012e348 and WORLD counterpart establish the 0x118 stride.
 * Event ChangeDialog scans records 0, 2 and 4, compares +0x14 with operand 0,
 * and checks the thread at +0x04 for task 0x33. For DisplayMessage, the text
 * threads store the dialog type's box nibble (& 0x70), the portrait unit and
 * the scroll state, and battle/world_menu_draw_active_window_frames read
 * them. */
typedef struct menu_window_buffer {
    s32 active;             /* 0x00: 1 once the text thread has built the window; the frame
                               drawers skip the pair otherwise; cleared on allocation and close */
    s32 thread_id;          /* 0x04 */
    s32 message_id;         /* 0x08: active/change-dialog text entry ID */
    s32 portrait_code;      /* 0x0c: portrait texture/CLUT descriptor, -1 when unused */
    s32 portrait_battle_id; /* 0x10: battle id of the DisplayMessage unit (box type 0x10),
                               drawn as the portrait when portrait_code == -1 */
    s32 dialogue_selector;  /* 0x14 */
    s32 box_type;           /* 0x18: dialog type & 0x70; 0x10 is the portrait box */
    s32 portrait_flipped;   /* 0x1c: 1 when the tail offset is negative; event
                               instruction 0x37 toggles it */
    s32 icon_flags;         /* 0x20: visibility bits for the three window icons */
    s32 scroll_range;       /* 0x24: scrollable lines (line - lines); 1 when freshly allocated */
    s32 scroll_position;    /* 0x28: first drawn line; the scroll marker sits at
                               scroll_position / scroll_range of its span; 1 when freshly allocated */
    /* Primitive payload established by the WORLD allocator at 0x800e2548:
     * draw mode, four window sprites, three icon sprites, two quads and a
     * trailing colour. */
    DR_MODE draw_mode;    /* 0x2c */
    SPRT sprites[4];      /* 0x38 */
    SPRT icon_sprites[3]; /* 0x88 */
    POLY_FT4 quads[2];    /* 0xc4 */
    u8 r;                 /* 0x114 */
    u8 g;                 /* 0x115 */
    u8 b;                 /* 0x116 */
    u8 is_message_box;    /* 0x117: set by the message-box thread; alternate frame clut and tint */
} menu_window_buffer_t;

typedef char menu_window_buffer_size_must_be_0x118[(sizeof(menu_window_buffer_t) == 0x118) ? 1 : -1];

typedef char menu_window_buffer_selector_must_be_0x14
    [((unsigned long)&((menu_window_buffer_t*)0)->dialogue_selector == 0x14) ? 1 : -1];

/* Text origin record passed to world_menu_display_text_entry; the same
 * layout as the origin_x..stride tail of world_menu_text_state_t, which most
 * WORLD callers pass directly. */
typedef struct menu_text_origin {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s32 unknown_04;
    /* 0x08 */ s32 stride; /* OPEN/WLDCORE pass their window record's width */
} menu_text_origin_t;

/* Full record bases, not the legacy aliases biased to thread_id. */
extern menu_window_buffer_t g_battle_menu_window_buffers[6]; /* 0x8016e43c */
extern s32 g_menu_inner_window_width;
extern s32 g_menu_text_palette_offset;
extern menu_window_buffer_t g_world_menu_window_buffers[6]; /* 0x8019aca0 */

/* menu */
void battle_menu_display_text_entry(s32 text_id, void* image, void* origin);
void battle_menu_handle_action(void* menu, s32 selected_option);
void battle_menu_set_text_origin(s16 x, s16 y);
s32 battle_menu_should_close_thread(s32* out_flag);

/* menu */
void world_menu_display_text_entry(s32 text_id, void* image, void* origin);
void world_menu_init_sprite(SPRT* sprite);
void world_menu_set_text_origin(s16 x, s16 y);
#endif
