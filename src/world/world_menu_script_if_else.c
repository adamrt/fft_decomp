#include "fft/world.h"
#include "psx/types.h"

/* Menu script if/else: opcode byte 3 selects a predicate, byte 4 the number
 * of commands in the "then" branch, byte 5 the number in the "else" branch,
 * byte 6 the constant argument. The taken branch is executed via the opcode
 * table; the other is skipped by its length bytes. */
u8* world_menu_script_if_else(u8* script) {
    s32 (*predicate)(s32) = g_world_menu_script_callbacks[script[3]];
    s32 count_true = script[4];
    s32 count_false = script[5];
    s32 arg;

    if (g_world_menu_use_scroll_position == 0) {
        arg = script[6];
    } else {
        arg = g_world_menu_scroll_offset + g_world_menu_scroll_row_offset;
        if (g_world_menu_scroll_pixel_offset < 0) {
            arg -= 1;
        }
    }
    script += script[1];
    if (predicate(arg) != 0) {
        count_true -= 1;
        while (count_true != -1) {
            script = g_world_menu_script_handlers[*script](script);
            count_true -= 1;
        }
        count_false -= 1;
        while (count_false != -1) {
            script += script[1];
            count_false -= 1;
        }
    } else {
        count_true -= 1;
        while (count_true != -1) {
            script += script[1];
            count_true -= 1;
        }
        count_false -= 1;
        while (count_false != -1) {
            script = g_world_menu_script_handlers[*script](script);
            count_false -= 1;
        }
    }
    return script;
}
