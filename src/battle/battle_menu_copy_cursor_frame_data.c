#include "psx/types.h"

typedef struct battle_menu_cursor_source {
    u16 texture_flags;
    u8 texture_v;
    u8 _pad03[5];
    u16 x;
    u16 y;
    u8 _pad0c[8];
    u16 width;
    u16 height;
} battle_menu_cursor_source_t;

typedef struct battle_menu_cursor_frame {
    u8 _pad00[8];
    u16 x;
    u16 y;
    u8 texture_u;
    u8 texture_v;
    u8 _pad0e[2];
    u16 width;
    u16 height;
} battle_menu_cursor_frame_t;

/* Copy the position and texture rectangle used by a menu cursor frame. */
void battle_menu_copy_cursor_frame_data(
    const battle_menu_cursor_source_t* source, battle_menu_cursor_frame_t* destination, s32 frame_index) {
    destination->x = source->x;
    destination->y = source->y;
    destination->texture_u = (*(const volatile u16*)&source->texture_flags & 0x3f) << 2;
    destination->texture_v = source->texture_v;
    destination->width = source->width;
    destination->height = source->height;
}
