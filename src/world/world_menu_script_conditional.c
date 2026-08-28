#include "fft/world.h"
#include "psx/types.h"

/* Menu script conditional: opcode byte 3 selects a predicate, byte 4 the
 * number of following commands, byte 5 the constant argument. When the
 * predicate holds the following commands are executed via the opcode table;
 * otherwise they are skipped by their length bytes. */
u8* world_menu_script_conditional(u8* script) {
    s32 (*predicate)(s32) = g_world_menu_script_callbacks[script[3]];
    s32 count = script[4];
    s32 arg;

    if (g_world_menu_use_scroll_position == 0) {
        arg = script[5];
    } else {
        arg = g_world_menu_scroll_offset + g_world_menu_scroll_row_offset;
        if (g_world_menu_scroll_pixel_offset < 0) {
            arg -= 1;
        }
    }
    script += script[1];
    if (predicate(arg) == 0) {
        count -= 1;
        while (count != -1) {
            script += script[1];
            count -= 1;
        }
    } else {
        count -= 1;
        while (count != -1) {
            script = g_world_menu_script_handlers[*script](script);
            count -= 1;
        }
    }
    return script;
}
