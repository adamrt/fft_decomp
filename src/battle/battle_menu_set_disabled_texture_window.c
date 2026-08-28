#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Cursor record view: a DR_MODE and SPRT followed by the texture window RECT
 * (option_panel_frame_t cursor_mode..cursor_texture_window). Callers pass
 * several record types that share this layout, hence the byte pointer. */
typedef struct battle_menu_cursor_record {
    DR_MODE mode;        /* 0x00 */
    SPRT sprite;         /* 0x0c */
    RECT texture_window; /* 0x20 */
} battle_menu_cursor_record_t;

/* Clear the cursor sprite's width and install the disabled texture window. */
void battle_menu_set_disabled_texture_window(u8* menu) {
    battle_menu_cursor_record_t* record = (battle_menu_cursor_record_t*)menu;

    record->sprite.w = 0;
    record->texture_window.h = 0;
    battle_copy_bytes(&record->texture_window, &g_battle_menu_disabled_texture_window, 8);
}
