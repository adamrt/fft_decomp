#include "fft/world.h"
#include "psx/types.h"

/* Scroll a menu list one page up (dir == -1) or down (dir == 1), as on Square + Up/Down.
 *
 * Built at -O1. `next` is assigned inside the comparison so the option count
 * loads first and the `+ 1` is materialised rather than reassociated into
 * `(cnt - 1) - off`. `cur` routes the cursor load through a temp (`lhu v0` /
 * `subu s0,v0,v1`) where a fused subtraction would load straight into s0.
 * `unused[4]` holds the 0x40 frame. */
void world_menu_scroll_list_page(s32 dir, u8* menu) {
    u8* list;
    /* Pin and tied launder: the load lands in $s1 and `top` is copied out of it (`lh s1` / `move v1,s1`). */
    register s32 old __asm__("$17");
    s32 top;
    s32 delta;
    s16 cnt;
    s16 count;
    s32 next;
    s32 cur;
    s32 unused[4];

    old = g_world_menu_scroll_offset;
    __asm__("" : "=r"(old) : "0"(old));
    top = old;
    /* The declaration is s16, which every other caller sees; this site
     * needs the unsigned load the target emits, so the width lives in a cast
     * here rather than in a second declaration. */
    cur = *(u16*)&g_world_menu_cursor_position;
    delta = cur - top;
    list = menu;
    if (dir == -1) {
        g_world_menu_scroll_offset = top - list[6];
        if (g_world_menu_scroll_offset < 0) {
            g_world_menu_scroll_offset = 0;
        }
    } else if (dir == 1) {
        g_world_menu_scroll_offset = (u16)g_world_menu_scroll_offset + list[6];
        cnt = g_world_menu_option_count;
        if (cnt - (next = g_world_menu_scroll_offset + 1) < list[6]) {
            g_world_menu_scroll_offset = cnt - list[6];
        }
    }
    g_world_menu_cursor_position = (u16)g_world_menu_scroll_offset;
    world_menu_load_scrollable_list_layout((world_menu_list_record_t*)list);
    g_world_menu_cursor_position = delta + (u16)g_world_menu_scroll_offset;
    count = g_world_menu_option_count;
    if ((s16)g_world_menu_cursor_position >= count) {
        g_world_menu_cursor_position = count - 1;
    }
    if (old != g_world_menu_scroll_offset) {
        g_world_menu_sound_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
}
