#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

u32 wldcore_input_check_repeating_directional(u32 mask);
void wldcore_menu_push_proposition_report_level(s32 argument);

/* Job-report confirmation idle handler (one of the g_wldcore_menu_idle_handlers
 * entries, dispatched every frame by wldcore_menu_dispatch_idle_handler): the
 * up/down repeats (0x1000/0x4000) toggle the selected row, moving the cursor
 * window to that row of the render record; cancel (0x40) and a lower-row
 * confirm release the level's records and resume the parent, while an
 * upper-row confirm (0x20) also starts WORLD thread 12 and pushes the
 * type-0x10 level for the stored argument.
 *
 * The `- -(...)` in the cursor-row expression is load bearing, not a typo.
 * The target's final add is `addu v0,v0,v1` (base_y first, result tied to
 * base_y's register); every plain `+` spelling of the same sum compiles to
 * `addu v1,v1,v0`. Written with a `+`, expand hands combine a plus tree with a
 * constant in it, combine reassociates the 0xE onto the scaled selection and
 * rebuilds the outer add through gen_binary, which canonicalises the complex
 * operand into position 0 -- so the sum is tied to the delta register and the
 * following `sw` reads it. Negating the addend makes the outer operation a
 * MINUS of a NEG, which combine turns back into a plus *after* that
 * canonicalisation, leaving base_y as operand 0; the rest of the block
 * (`sll` at +0x120, address materialisation, `lw` at +0x130, `addiu` filling
 * the load shadow at +0x134) is unchanged. `base_y - (0 - (selection * 0x10 +
 * 0xE))` and `base_y - (0 - (0xE + selection * 0x10))` are byte-identical
 * spellings; `base_y - (-0xE - selection * 0x10)` is not (fold collapses it
 * back to a plus tree and the tie returns). */
void wldcore_menu_idle_job_report_confirmation(wldcore_job_report_confirm_level_t* level) {
    s32 selection;

    if (g_wldcore_window_render_records[level->render_index].flags & 0x100) {
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
    block_tail:
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 3;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0
        || wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        selection = level->selection ^ 1;
        level->selection = selection;
        g_wldcore_window_records[level->cursor_window].y
            = g_wldcore_window_render_records[level->render_index].base_y - -(selection * 0x10 + 0xE);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        if (level->selection != 0) {
            /* Shared tail: a duplicated release path is cross-jumped into
             * this arm instead of the cancel arm the target keeps. */
            goto block_tail;
        } else {
            world_thread_set_parameters(0xC, 0, 0, 1);
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count -= 1;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth -= 2;
            wldcore_menu_push_proposition_report_level(level->argument);
        }
    }
}
