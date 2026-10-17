/* Menu script execution and conditional command groups. */

#include "fft/equipment.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/screen.h"
#include "fft/unit_list.h"
#include "psx/types.h"

/* Each interpreter stops at its own dialect's first non-instruction value. */
#if defined(FFT_WORLD)
enum { MENU_SCRIPT_END = SCREEN_SCRIPT_END };
#elif defined(FFT_EVENT_BUNIT)
enum { MENU_SCRIPT_END = UNIT_LIST_SCRIPT_END };
#elif defined(FFT_EVENT_EQUIP)
enum { MENU_SCRIPT_END = EQUIPMENT_SCRIPT_END };
#endif

/* Page setup fills the value, text, and icon callbacks used by menu commands. */
menu_callback_t g_menu_row_callbacks[16] = { 0 };

/* JOBSTTS advances its transition in a separate script runner. */
#ifndef FFT_EVENT_JOBSTTS
/* Run a menu command stream: reset the ordering-table index, semi-transparency and lock, keep
 * `flags` as the stream's input, then dispatch each command through g_menu_opcode_handlers until
 * the program's end opcode (MENU_SCRIPT_END: 0x1C in BUNIT and WORLD, 0x19 in
 * EQUIP). */
void menu_script_run(void* script, s32 flags) {
    menu_instruction_t* instruction = script;

    g_menu_gfx_otag_index = 0;
    g_menu_gfx_semitransparency = 0;
    g_menu_script_input = flags;
    g_gfx_otag_index_locked = 0;
    if (instruction->header.opcode != MENU_SCRIPT_END) {
        do {
            instruction = g_menu_opcode_handlers[instruction->header.opcode](instruction);
        } while (instruction->header.opcode != MENU_SCRIPT_END);
    }
}

/* Run a menu display script with palette mode `mode` (nonzero dims it); a dimmed script also gets
 * no input. */
void menu_run_script_with_palette_mode(void* script, s32 input, s32 mode) {
    s32 value = input;

    if (mode != 0) {
        value = 0;
    }
    menu_set_palette_mode(mode);
    menu_script_run(script, value);
}
#endif

/* WORLD keeps its group commands in its larger opcode unit. */
#ifndef FFT_WORLD
/* Menu script command: run the next count commands when the row predicate holds, otherwise skip
 * them. The predicate gets the command's literal row, or the current list row in list mode. */
menu_instruction_t* menu_opcode_01_if_group(menu_instruction_t* cmd) {
    s32 (*callback)(s32);
    s32 count;
    s32 row;

    callback = g_menu_row_callbacks[cmd->conditional.predicate].value;
    count = cmd->conditional.count;
    if (g_menu_list_row_mode == 0) {
        row = cmd->conditional.value;
    } else {
        row = g_menu_scroll_base_index + g_menu_list_row_index;
        if (g_menu_list_scroll_offset < 0) {
            row -= 1;
        }
    }
    cmd = menu_next_instruction(cmd);
    if (callback(row) == 0) {
        while (--count != -1) {
            cmd = menu_next_instruction(cmd);
        }
    } else {
        while (--count != -1) {
            cmd = g_menu_opcode_handlers[cmd->header.opcode](cmd);
        }
    }
    return cmd;
}

/* Run the selected branch and skip the other, using the literal or current
 * list row as the predicate's argument. */
menu_instruction_t* menu_opcode_02_if_else_group(menu_instruction_t* cmd) {
    s32 then_count;
    s32 else_count;
    s32 (*callback)(s32);
    s32 row;

    then_count = cmd->if_else.count_true;
    else_count = cmd->if_else.count_false;
    callback = g_menu_row_callbacks[cmd->if_else.predicate].value;
    if (g_menu_list_row_mode == 0) {
        row = cmd->if_else.value;
    } else {
        row = g_menu_scroll_base_index + g_menu_list_row_index;
        if (g_menu_list_scroll_offset < 0) {
            row--;
        }
    }
    cmd = menu_next_instruction(cmd);
    if (callback(row) != 0) {
        for (then_count--; then_count != -1; then_count--) {
            cmd = g_menu_opcode_handlers[cmd->header.opcode](cmd);
        }
        for (else_count--; else_count != -1; else_count--) {
            cmd = menu_next_instruction(cmd);
        }
    } else {
        for (then_count--; then_count != -1; then_count--) {
            cmd = menu_next_instruction(cmd);
        }
        for (else_count--; else_count != -1; else_count--) {
            cmd = g_menu_opcode_handlers[cmd->header.opcode](cmd);
        }
    }
    return cmd;
}
#endif
