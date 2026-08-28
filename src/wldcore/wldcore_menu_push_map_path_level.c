#include "fft/main_runtime.h"
#include "fft/wldcore.h"

void wldcore_map_prepare_path_animation(s32 from, s32 to);

/* Pushes menu level type 0x35, which animates the world-map path the script
 * asked to draw or erase: wldcore_map_prepare_path_animation prepares the path, then the step
 * handler at 0x8006dbb8 walks its nodes forward, or backward when `erase` is
 * set, playing the pending sound once. The push raises system flag 0x2000 and
 * hides the parent level's window record (flag 0x10); the step handler clears
 * both on the way out.
 *
 * Structural twin of wldcore_push_map_location_visibility_level (0x8006de50,
 * type 0x36), whose spelling this follows statement for statement, with the
 * leading call and a third argument column added.
 *
 * Window: the only `jr ra` is at 0x8006dbb0 with its delay-slot nop at
 * 0x8006dbb4, and the type-0x35 step handler begins at 0x8006dbb8 -- 304
 * bytes, 76 instructions, one call and no branches.
 *
 * Profile is the module canonical gcc-2.6.3_O2_aspsx-2.21. Every menu-stack
 * and window-record store below must stay a bare-symbol reference so it keeps
 * the 2.21 four-instruction lui/addiu/addu/sw $at expansion, which the target
 * uses at all six indexed-global sites. */
void wldcore_menu_push_map_path_level(s32 path_id, s32 path_id_hi, s32 erase) {
    s32 depth;
    s32 window_index;

    wldcore_map_prepare_path_animation(path_id, path_id_hi);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].path.path_id = path_id;
    g_wldcore_menu_stack_records_next[depth].path.path_id_hi = path_id_hi;
    g_wldcore_menu_stack_records_next[depth].path.erase = erase;
    g_wldcore_menu_stack_records_next[depth].path.pending_sound = 1;
    g_main_system_flags |= 0x2000;
    window_index = g_wldcore_menu_stack_records_next[depth - 1].window_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_MAP_PATH;
    g_wldcore_window_records[window_index].flags |= 0x10;
    g_wldcore_menu_stack_depth = depth + 1;
}
