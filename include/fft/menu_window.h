#ifndef MENU_WINDOW_H
#define MENU_WINDOW_H

/* Menu and message windows: their frame images, the window buffers the dialog threads draw into, window
 * scaling, the icon sprites menu windows draw cursors and arrows with, and the window frame panels. */

#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Window images are 4bpp images built from the glyph sheet's eight-tile border and interior patterns; a message
 * window can add a tail that points at the speaker. */

/* Last argument of menu_build_and_upload_window_frame_image. The UPLOAD
 * modes load the image into VRAM and free the buffer; the BUILD modes return
 * the buffer for the caller to draw into and upload. The ALTERNATE modes use
 * the frame pattern with sixteen-row top and bottom borders. */
typedef enum menu_window_frame_mode {
    MENU_WINDOW_FRAME_UPLOAD = 0,
    MENU_WINDOW_FRAME_BUILD = 1,
    MENU_WINDOW_FRAME_ALTERNATE_UPLOAD = 2,
    MENU_WINDOW_FRAME_ALTERNATE_BUILD = 3,
} menu_window_frame_mode_e;

/* Tail glyph source and destination records for menu_build_window_image. */
/* Image the window frame thread composes its border pieces in before uploading them. */
extern u8 g_menu_window_frame_image[];

void menu_build_window_image_row(const u16* tiles, u16* row, s32 count, s32 index);

/* With keep set, returns the image instead of uploading and freeing it. */
u8* menu_build_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset, s32 keep);

void menu_build_and_upload_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset);

/* In BATTLE, MENU_WINDOW_FRAME_ALTERNATE_UPLOAD at scroll list depth 2 draws into the fixed UI buffer past the
 * companion overlay instead of allocating. */
u8* menu_build_and_upload_window_frame_image(s32 width, s32 height, RECT* rect, s32 mode);

/* menu_window_buffer_t::icon_flags: which of the window's three icon sprites
 * are drawn. */
typedef enum menu_window_icon_flags {
    MENU_WINDOW_ICON_SCROLL_UP = 0x01,     /* icon_sprites[0]: lines above the first drawn line */
    MENU_WINDOW_ICON_SCROLL_DOWN = 0x02,   /* icon_sprites[1]: lines below the last drawn line */
    MENU_WINDOW_ICON_SCROLL_MARKER = 0x04, /* icon_sprites[2]: the scroll position */
} menu_window_icon_flags_e;

/* The six message window buffers, allocated and released as three adjacent pairs by
 * menu_alloc_window_buffer_pair, which fixes the 0x118 stride. The event instruction ChangeDialog scans
 * records 0, 2 and 4, compares dialogue_selector with its operand 0 and checks the thread for task 0x33. For
 * DisplayMessage the text threads store the dialog type's box nibble (& 0x70), the portrait unit and the scroll
 * state, which menu_draw_active_window_frames reads. */
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
    s32 icon_flags;         /* 0x20: menu_window_icon_flags_e */
    s32 scroll_range;       /* 0x24: scrollable lines (line - lines); 1 when freshly allocated */
    s32 scroll_position;    /* 0x28: first drawn line; the scroll marker sits at
                               scroll_position / scroll_range of its span; 1 when freshly allocated */
    /* Primitive payload set up by menu_alloc_window_buffer_pair:
     * draw mode, four window sprites, three icon sprites, two quads and a
     * trailing color. */
    DR_MODE draw_mode;    /* 0x2c */
    SPRT sprites[4];      /* 0x38 */
    SPRT icon_sprites[3]; /* 0x88 */
    POLY_FT4 quads[2];    /* 0xc4 */
    rgb24_t color;        /* 0x114; window tint */
    u8 is_message_box;    /* 0x117: set by the message-box thread; alternate frame clut and tint */
} menu_window_buffer_t;

typedef char menu_window_buffer_size_must_be_280[(sizeof(menu_window_buffer_t) == 280) ? 1 : -1];
extern menu_window_buffer_t* g_menu_window_buffer_slots[6];
extern menu_window_buffer_t g_menu_window_buffers[6];
menu_window_buffer_t* menu_alloc_window_buffer_pair(void);
void menu_reset_window_buffer_slots(void);
void menu_release_window_buffer_pair(menu_window_buffer_t* buffer);

/* BATTLE's and WORLD's bodies differ. */
void menu_draw_active_window_frames(void);

/* Transition step of the windows that zoom open: menu script command 0x16 advances it up to 4, and the zoomed
 * draw-area command crops each window by it. */
extern s8 g_gfx_transition_frame;

extern u16* g_menu_window_tween_easing_tables[];
void menu_set_window_scale_step(s32 value);
s32 menu_get_window_scale_step(void);
void menu_animate_window_quad_crop(menu_window_buffer_t* buffers, RECT* rect, RECT* unused_to);
void menu_tween_window_quads(menu_window_buffer_t* buffers, RECT* to, RECT* to2, s32 table);

/* Icon packet head: two DR_MODE packets at +0x00/+0x0c and four SPRT packets
 * at +0x18 (CLUT ids at +0x26/+0x3a/+0x4e/+0x62). The submitter draws all
 * six packets. */
typedef struct menu_icon_sprites {
    DR_MODE first_draw_mode;
    DR_MODE draw_mode;
    SPRT sprites[4];
} menu_icon_sprites_t;

typedef char menu_icon_sprites_size_must_be_104[(sizeof(menu_icon_sprites_t) == 104) ? 1 : -1];

/* The 0x7c-byte icon record menu_build_window_sprites fills: menu_icon_sprites_t followed by three borrowed
 * icon sprites and two state words. Provisional: the state words are unnamed. */
typedef struct menu_icon_record {
    menu_icon_sprites_t base; /* 0x00 */
    SPRT* icons[3];           /* 0x68 */
    s32 _unknown_74;          /* 0x74 */
    s32 _unknown_78;          /* 0x78 */
} menu_icon_record_t;

void menu_select_icon_cluts(menu_icon_sprites_t* frame);
void menu_submit_icon_primitives(menu_icon_sprites_t* frame);
void menu_update_icon_cursor_sprites(menu_entry_t* param, menu_icon_sprites_t* record, s32 frame, s32 cursor);

/* BATTLE's and WORLD's bodies differ. */
void menu_build_window_sprites(RECT* rect, menu_entry_t* param, menu_icon_record_t* record);

/* Shared window record whose signed rectangle the window-frame and tiled
 * rectangle commands use for source 2 (g_menu_window_rect_source). */
typedef struct menu_window_rect_source {
    u8 _unused_00[6];
    RECT rect; /* 0x06 */
} menu_window_rect_source_t;

/* Layout of the window frame primitive block in g_menu_scratch_buffer (WORLD and OPTION), which holds two
 * identical frame records. The 0x158 record size is fixed by the second frame starting there and by the byte
 * copy between them. Members come from the panels' own accesses plus two helpers: menu_init_sprite_array is
 * handed the nine SPRT packets, and menu_init_scaled_draw_area_packets clears rects[0].w and installs rects[1]
 * of the draw-area pair. The block is zero-filled in the overlay image, so none of it is initialized data.
 * Provisional. */
typedef struct menu_panel_frame {
    DR_MODE modes[7];                 /* 0x000: one per texture window */
    SPRT sprites[9];                  /* 0x054: 5 border spans then 4 corners */
    texture_primitive_t texture_prim; /* 0x108: tpage-7 VRAM allocation record */
    /* 0x130; menu_init_scaled_draw_area_packets */
    scaled_draw_area_pair_t draw_areas;
} menu_panel_frame_t;

typedef char menu_panel_frame_size_must_be_344[sizeof(menu_panel_frame_t) == 344 ? 1 : -1];

/* Scratch in the event-overlay area past the scene program: the unit stats panels lay it out as two sprite
 * pages, the window frame panels as two panel frames, and BATTLE's scrolling ability list saves its five
 * 16-color CLUTs there. */
typedef union menu_scratch_buffer {
    menu_sprite_page_t sprite_pages[2];
    menu_panel_frame_t panel_frames[2];
    u16 clut_save[5 * 16];
} menu_scratch_buffer_t;

extern menu_window_rect_source_t* g_menu_window_rect_source;

/* Texture windows of the frame pieces the frame threads upload. */
extern RECT g_menu_thin_panel_texture_windows[7];

extern RECT g_menu_panel_texture_windows[7];
extern menu_scratch_buffer_t g_menu_scratch_buffer;
void menu_window_frame_thread(void);
void menu_thin_window_frame_thread(void);

#endif
