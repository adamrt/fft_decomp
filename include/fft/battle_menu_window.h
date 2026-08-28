#ifndef FFT_BATTLE_MENU_WINDOW_H
#define FFT_BATTLE_MENU_WINDOW_H

#include "fft/battle_gfx.h"
#include "fft/menu_types.h"

/* Provisional 0x7c-byte menu window record built by this routine: two draw
 * modes, four sprites (background, left cap, right cap and a style icon) and
 * three optional extra sprites for the compound styles. */
typedef struct battle_menu_window_record {
    DR_MODE mode0;   /* 0x00 */
    DR_MODE mode1;   /* 0x0c */
    SPRT sprites[4]; /* 0x18 */
    SPRT* extra[3];  /* 0x68 */
    s32 unknown_74;  /* 0x74 */
    s32 unknown_78;  /* 0x78 */
} battle_menu_window_record_t;

/* Provisional window request: VRAM source point, size, screen point and the
 * load parameters for the window background quad. */
typedef struct battle_menu_window_spec {
    battle_image_location_t load;             /* 0x00 */
    u16 width;                                /* 0x04 */
    u16 height;                               /* 0x06 */
    battle_image_location_t screen;           /* 0x08 */
    u8 unknown_0c[4];                         /* 0x0c */
    world_gfx_image_load_parameters_t params; /* 0x10 */
    u8 unknown_1c[0x10];                      /* 0x1c */
    s16 style;                                /* 0x2c */
} battle_menu_window_spec_t;

/* Provisional menu text image record rendered by
 * battle_menu_render_text_image_at_record_origin: a text image buffer is
 * built (the wide builder when box_type is 0x10), the text at +0x10 is
 * drawn from the origin at +0x40, and the RECT at +0x08 is uploaded. Same
 * layout as the WORLD world_menu_text_image_t and the text threads' dialog
 * records. */
typedef struct battle_menu_text_image {
    void* buffer; /* 0x00 */
    u16 width;    /* 0x04 */
    u16 height;   /* 0x06 */
    RECT rect;    /* 0x08 */
    void* text;   /* 0x10 */
    u8 unknown_14[2];
    u16 dialog_type; /* 0x16; window-image mode */
    u16 box_type;    /* 0x18: 0 skips rendering, 0x10 selects the wide builder */
    u8 unknown_1a[8];
    u16 first_line; /* 0x22 */
    u16 last_line;  /* 0x24 */
    u8 unknown_26[0x30 - 0x26];
    s16 tail_offset; /* 0x30; arrow position */
    u8 unknown_32[0x40 - 0x32];
    u16 origin_x; /* 0x40 */
    u16 origin_y; /* 0x42 */
} battle_menu_text_image_t;

typedef char battle_menu_text_image_size_must_be_0x44[(sizeof(battle_menu_text_image_t) == 0x44) ? 1 : -1];

void battle_menu_render_text_image_at_record_origin(battle_menu_text_image_t* record);

/* Provisional 8-byte header copied out of the request for the caller. */
typedef struct battle_menu_window_header {
    u16 texture_x;  /* 0x00 */
    u16 texture_y;  /* 0x02 */
    s16 half_width; /* 0x04 */
    u16 height;     /* 0x06 */
} battle_menu_window_header_t;

void battle_menu_build_window_sprites(
    battle_menu_window_header_t* header, battle_menu_window_spec_t* spec, battle_menu_window_record_t* record);
s32 battle_menu_display_projected_action_effect(POLY_FT4* prim, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, world_gfx_image_load_parameters_t* params);

#endif
