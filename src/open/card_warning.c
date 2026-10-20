/* Starting a new game without a memory card saves the title background while
 * asking whether to continue, then restores the menu or starts the prologue. */

#include "fft/controller_stack.h"
#include "fft/display_object.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/prologue.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/title.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* A 256x240 16-bit frame image in 32-bit words. */
#define PROLOGUE_FRAME_IMAGE_WORDS (256 * 240 / 2)

/* New Game on the title menu. With a memory card in either slot the new-game
 * prologue starts at once. With none the frame image is saved to
 * g_system_work_buffer_0 and blanked, animation 0x12 is shown on the black screen
 * and the title-exit controller (handler 12) is pushed: Cross returns to the
 * title menu, Circle or Start starts the prologue anyway. */
void title_select_new_game(void) {
    s32* source;
    s32* destination;
    s32 index;
    s32 record;
    point32_t* position;
    s32 controller;
    s32 card_present;
    s32 frame_padding[2];

    card_present = title_check_card_slots();
    index = 0;
    if (card_present != 0) {
        prologue_start();
        return;
    }

    source = g_file_destination;
    destination = (s32*)g_system_work_buffer_0;
    for (; index < PROLOGUE_FRAME_IMAGE_WORDS; index++) {
        *destination = *source;
        *source = 0;
        source++;
        destination++;
    }

    gfx_start_overlay_fade_out(0x10);
    record = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_display_object_animated_pool[record].sequence = 0x12;
    g_display_object_animated_pool[record].anim_counter = 0;
    g_display_object_animated_pool[record].frame_index = 0;
    position = &g_display_object_animated_pool[record].position;
    controller = g_controller_depth;
    position->x = -4;
    position->y = -16;
    g_controller_records[controller].card_warning.state = 0;
    g_controller_handler_indices[controller] = CONTROLLER_TITLE_CARD_WARNING;
    g_controller_depth = controller + 1;
}

/* Per-frame handler of the title-exit controller (handler 12), pushed when New
 * Game is chosen with no memory card inserted.
 *
 * Cross, or Circle or Start, records the choice and fades the screen to black.
 * When the fade has ended the notice record and the controller are released: on
 * Cross the frame image saved in g_system_work_buffer_0 is copied back and the
 * title menu is pushed again; on Circle or Start the new-game prologue starts. */
void title_update_card_warning(controller_record_t* record) {
    s32 state;
    u32 buttons;

    if ((g_gfx_overlay_fade.flags & GFX_OVERLAY_FADE_ACTIVE) != 0) {
        return;
    }

    if (record->card_warning.state != TITLE_CARD_WARNING_INPUT) {
        g_display_object_animated_count -= 1;
        g_display_object_queue_count -= 1;
        g_controller_depth -= 1;
        state = record->card_warning.state;
        if (state == TITLE_CARD_WARNING_CANCEL) {
            s32* source;
            s32* destination;
            s32 index;

            SetDispMask(0);
            source = (s32*)g_system_work_buffer_0;
            g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
            destination = g_file_destination;
            for (index = 0; index < PROLOGUE_FRAME_IMAGE_WORDS; index++) {
                destination[index] = source[index];
            }
            title_menu_push(0);
        } else if (state == TITLE_CARD_WARNING_CONFIRM) {
            prologue_start();
        }
        return;
    }

    buttons = g_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        sound_sfx_play(SOUND_SFX_CANCEL);
        gfx_start_overlay_fade_in(0x20);
        record->card_warning.state = TITLE_CARD_WARNING_CANCEL;
    } else if ((buttons & (PSX_PAD_CIRCLE | PSX_PAD_START)) != 0) {
        sound_sfx_play(SOUND_SFX_CONFIRM);
        gfx_start_overlay_fade_in(0x20);
        record->card_warning.state = TITLE_CARD_WARNING_CONFIRM;
    }
}
