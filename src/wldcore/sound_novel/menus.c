/* The sound-novel reader opens from a treasure and manages its quit prompt and reading history. */

#include "fft/display_object.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/music.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sound_novel.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_sound_novel_quit_help_text_ids[];
extern point32_t g_hud_saved_value_position;

/* Scalar history views retained for the Quit shuffle: direct member accesses
 * change its original loads and store scheduling. */
extern s16 g_sound_novel_history_0;
extern u16 g_sound_novel_history_2;

static void quit_menu_open(void);
static void hud_restore(void);
static void history_open(void);
static void history_entry_draw(sound_novel_history_level_t* level, s32 index);

/*
 * Pushes the sound-novel menu level (MENU_STACK_LEVEL_SOUND_NOVEL).
 * `argument` is kept in the level and names the treasure detail reopened
 * when the novel ends; the novel read is the one in
 * g_sound_novel_selected.
 *
 * Hides the context value display, clears VRAM and the scratch buffer, and
 * appends the level's display objects, all hidden: the two button prompts,
 * the VRAM strip record and the overlay. It then appends the novel's sound
 * effect data, restores any saved reading state into the first render record
 * (sound_novel_reading_restore), blocks Cross, Square and Select
 * for the menu and starts the screen fade.
 */
void sound_novel_reader_open(s32 argument) {
    RECT bounds;
    point32_t origin;
    display_object_animated_t* window1;
    display_object_animated_t* window2;
    display_object_animated_t* window4;
    s32 bar_render;
    s32 index;
    point32_t* pos1;
    point32_t* pos2;
    point32_t* pos3;
    point32_t* pos4;
    point32_t* render_pos;

    /* Held from entry: with every saved register taken, reload rematerializes
     * the constant beside its one use, which is the target's la placement. */
    render_pos = &g_display_object_gs_image_pool[0].position;
    g_hud_saved_value_position = g_hud_value_position;
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
    world_map_image_buffers_clear(1);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.treasure_id = argument;

    bar_render = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.quit_prompt_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_START_BUTTON;
    window1 = &g_display_object_animated_pool[index];
    window1->priority = 0xA;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    pos1 = &g_display_object_animated_pool[index].position;
    pos1->x = -0x7A;
    pos1->y = 0x60;
    g_display_object_animated_pool[index].palette = 0xA;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.history_prompt_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_SQUARE_BUTTON;
    g_display_object_animated_pool[index].priority = 0xA;
    g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    window2 = &g_display_object_animated_pool[index];
    window2->anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    pos2 = &g_display_object_animated_pool[index].position;
    pos2->x = 0x42;
    pos2->y = 0x60;
    g_display_object_animated_pool[index].palette = 0xA;

    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.brightness_image = index;
    bounds.x = 0x1E4;
    bounds.y = 0x1E8;
    bounds.w = 0x20;
    bounds.h = 8;
    origin.x = 0x20;
    origin.y = 0x1E8;
    display_object_gs_image_init(index, bounds, origin, 0);
    pos3 = (point32_t*)((u8*)render_pos + index * sizeof(display_object_gs_image_t));
    pos3->x = 0x34;
    pos3->y = 0x55;
    g_display_object_gs_image_pool[index].priority = 8;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_UNTINTED;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.brightness_overlay_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_OVERLAY;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_UNTINTED;
    window4 = &g_display_object_animated_pool[index];
    window4->anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    pos4 = &g_display_object_animated_pool[index].position;
    pos4->x = 0x54;
    pos4->y = 0x58;
    g_display_object_animated_pool[index].palette = 0;

    suzuki_sound_append_resource(g_sound_novel_sound_resource);
    sound_novel_reading_restore(bar_render);
    g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    g_world_map_fade.priorities[0] = 8;
    g_world_map_fade.flags[1] |= WORLD_MAP_FADE_BOX_VISIBLE;
    g_menu_stack_records[g_menu_stack_depth].sound_novel._unknown_20 = 1;
    g_menu_stack_records[g_menu_stack_depth].sound_novel.countdown = 9999;
    g_menu_stack_records[g_menu_stack_depth].sound_novel.fade_timer = 0;
    g_menu_stack_records[g_menu_stack_depth].sound_novel.phase = SOUND_NOVEL_PHASE_RUN;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_SOUND_NOVEL;
    g_menu_stack_depth++;
    world_map_fade_start(0, 0x10);
}

/* Resume handler of the sound novel level: reload the saved background set, show the picture again
 * (and the side windows when the script left them visible) and resume the message thread. */
void sound_novel_reader_resume(menu_stack_record_t* level) {
    s16 set = g_sound_novel_reading.background_set;

    if (set != -1) {
        picture_load_background_set(set);
        file_pending_wait();
    }
    g_display_object_gs_image_pool[g_sound_novel_reading.picture_render_index].flags &= ~DISPLAY_OBJECT_BLANK;
    if (g_sound_novel_reading.state_flags & SOUND_NOVEL_STATE_PARENT_WINDOWS_VISIBLE) {
        g_display_object_animated_pool[level->sound_novel.quit_prompt_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->sound_novel.history_prompt_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
    }
    thread_resume(WORLD_MAP_THREAD_SLOT_MESSAGE);
}

/* The g_level_suspend_handlers entry of MENU_STACK_LEVEL_SOUND_NOVEL: nothing to suspend. */
void sound_novel_reader_suspend(menu_stack_record_t* unused_level) {
}

/*
 * Per-frame step of the sound novel (MENU_STACK_LEVEL_SOUND_NOVEL).
 *
 * It runs the novel's script until it yields (sound_novel_script_run).
 * L1 and R1 raise and lower the screen brightness, showing the brightness bar
 * for 60 frames. While the message box waits for a button: Select opens help
 * message 0x1036, Start (with the side windows up) opens the Quit/Continue
 * menu and holding Square opens the text history after 8 frames. When the
 * script ends the message box is stopped and the music and screen fade out;
 * then (END) the level's records are released and a screen transition reopens
 * the treasure detail the novel was started from.
 *
 * The unused array reproduces the target's 0x40-byte frame.
 */
void sound_novel_reader_update(menu_stack_record_t* level) {
    s32 unused[2];
    u16* state_flags;
    world_map_fade_t* fade;
    s16* brightness;
    u32 buttons;
    s32 width;
    s32 window;
    u16 shade;

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && !(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
        switch (level->sound_novel.phase) {
        case SOUND_NOVEL_PHASE_OPEN_HISTORY:
            if (level->sound_novel.countdown == 0) {
                g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
                history_open();
                return;
            }
            level->sound_novel.countdown--;
            return;
        case SOUND_NOVEL_PHASE_END:
            if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
                return;
            }
            g_menu_blocked_buttons = 0;
            g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
            hud_restore();
            fade = &g_world_map_fade;
            g_world_map_fade.priorities[0] = 0xF;
            g_world_map_fade.flags[1] = 0;
            g_display_object_animated_count -= 3;
            g_display_object_image_count -= 2;
            g_display_object_queue_count -= 5;
            g_menu_stack_depth--;
            fade->flags[0] = (fade->flags[0] & ~WORLD_MAP_FADE_BOX_VISIBLE) | WORLD_MAP_FADE_OUT;
            sound_all_sfx_stop();
            suzuki_sound_remove_resource(g_sound_novel_sound_resource);
            sound_novel_selected_reading_reset(g_sound_novel_selected);
            g_sound_novel_countdown_frames = 0;
            screen_transition_open(MENU_STACK_LEVEL_BRAVE_STORY_TREASURE, level->sound_novel.treasure_id, 0);
            return;
        }
    }

    state_flags = &g_sound_novel_reading.state_flags;
    if (*state_flags & SOUND_NOVEL_STATE_END) {
        return;
    }

    if (sound_novel_script_run() != 0) {
        if (g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1)) {
            if (g_input_current_buttons & PSX_PAD_L1) {
                if (g_sound_novel_reading.brightness < 0xFF) {
                    g_sound_novel_reading.brightness++;
                }
            }
            if (g_input_current_buttons & PSX_PAD_R1) {
                brightness = &g_sound_novel_reading.brightness;
                if (*brightness != 0) {
                    (*brightness)--;
                }
            }
            shade = g_sound_novel_reading.brightness;
            g_world_map_fade.boxes[1].r = shade;
            g_world_map_fade.boxes[1].g = shade;
            g_world_map_fade.boxes[1].b = shade;
            level->sound_novel.fade_timer = 0x3C;
        }

        if (level->sound_novel.fade_timer != 0) {
            g_display_object_gs_image_pool[level->sound_novel.brightness_image].flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[level->sound_novel.brightness_overlay_window].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            width = 0x100 - g_sound_novel_reading.brightness;
            g_display_object_gs_image_pool[level->sound_novel.brightness_image].bounds.w = width / 4;
            level->sound_novel.fade_timer--;
        } else {
            g_display_object_gs_image_pool[level->sound_novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[level->sound_novel.brightness_overlay_window].flags |= DISPLAY_OBJECT_HIDDEN;
        }

        if (screen_is_dialog_waiting() != 0) {
            buttons = g_world_map_input_new_button_presses;
            if (buttons & PSX_PAD_CIRCLE) {
                return;
            }
            g_display_object_animated_pool[level->sound_novel.quit_prompt_window].palette
                = g_display_object_animated_pool[level->sound_novel.history_prompt_window].palette = 0;
            if (buttons & PSX_PAD_SELECT) {
                menu_stack_message_open(0x1036, 1);
                return;
            }
            if ((buttons & PSX_PAD_START)
                && (g_sound_novel_reading.state_flags & SOUND_NOVEL_STATE_PARENT_WINDOWS_VISIBLE)
                && !(g_sound_novel_reading.state_flags & SOUND_NOVEL_STATE_WHITE_OVERLAY)) {
                g_display_object_animated_pool[level->sound_novel.quit_prompt_window].sequence
                    = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_START_BUTTON_PRESS;
                g_display_object_animated_pool[level->sound_novel.quit_prompt_window].frame_index
                    = g_display_object_animated_pool[level->sound_novel.quit_prompt_window].anim_counter = 0;
                level->sound_novel.fade_timer = 0;
                g_display_object_gs_image_pool[level->sound_novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
                g_display_object_animated_pool[level->sound_novel.brightness_overlay_window].flags
                    |= DISPLAY_OBJECT_HIDDEN;
                quit_menu_open();
                return;
            }
            if ((g_input_current_buttons & PSX_PAD_SQUARE)
                && (g_sound_novel_reading.state_flags & SOUND_NOVEL_STATE_PARENT_WINDOWS_VISIBLE)
                && g_sound_novel_reading.text_history[3] != -1) {
                g_display_object_animated_pool[level->sound_novel.history_prompt_window].sequence
                    = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_SQUARE_BUTTON_PRESS;
                window = level->sound_novel.history_prompt_window;
                g_display_object_animated_pool[window].anim_counter = 0;
                g_display_object_animated_pool[window].frame_index = 0;
                level->sound_novel.fade_timer = 0;
                g_display_object_gs_image_pool[level->sound_novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
                g_display_object_animated_pool[level->sound_novel.brightness_overlay_window].flags
                    |= DISPLAY_OBJECT_HIDDEN;
                g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
                level->sound_novel.phase = SOUND_NOVEL_PHASE_OPEN_HISTORY;
                level->sound_novel.countdown = 8;
            }
        } else {
            g_display_object_animated_pool[level->sound_novel.quit_prompt_window].palette
                = g_display_object_animated_pool[level->sound_novel.history_prompt_window].palette = 0xA;
        }
    } else {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            screen_thread_request_stop(WORLD_MAP_THREAD_SLOT_MESSAGE);
        }
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x20);
        sound_all_sfx_stop();
        level->sound_novel.phase = SOUND_NOVEL_PHASE_END;
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        if (!(*state_flags & SOUND_NOVEL_STATE_WHITE_OVERLAY)) {
            world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        }
    }
}

/* Pushes the sound novel's Quit/Continue menu (MENU_STACK_LEVEL_SOUND_NOVEL_QUIT):
 * the cursor and side label windows and a 0x3c x 0x30 TEXT_ID_QUIT_CONTINUE
 * record at (-0x1e, -0x18), with the windows placed on it. Blocks every
 * button, shows the fade box black (the step dims the screen with it) and
 * starts on Quit with no fade.
 *
 * The level is written through the sound_novel_quit member of the stack-record
 * union; union field access keeps the target's per-field `$at` bases. The
 * fade state is written through a pointer, which keeps the
 * target's register base for the offset-0 32-bit flags. The unused array
 * preserves the target's 0x68-byte frame. */
static void quit_menu_open(void) {
    RECT bounds;
    u8 unused[0x28];
    point32_t* base;
    s32 index;
    s32 depth;
    world_map_fade_t* fade;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_quit.cursor_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_quit.second_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_LIST_SIDE_LABEL;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_quit.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x3C;
    bounds.h = 0x30;
    bounds.y = 0;
    window_text_record_init(index, bounds, 0, TEXT_ID_QUIT_CONTINUE, g_window_image_buffer);
    base = &g_display_object_gs_image_pool[index].position;
    g_display_object_gs_image_pool[index].priority = 8;
    depth = g_menu_stack_depth;
    base->x = -(bounds.w / 2);
    base->y = -0x18;
    g_display_object_animated_pool[g_menu_stack_records[depth].sound_novel_quit.cursor_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 6;
    g_display_object_animated_pool[g_menu_stack_records[depth].sound_novel_quit.cursor_window].position.y
        = g_display_object_gs_image_pool[index].position.y + 0xE;
    g_display_object_animated_pool[g_menu_stack_records[depth].sound_novel_quit.second_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 4;
    g_menu_blocked_buttons = -1;
    g_display_object_animated_pool[g_menu_stack_records[depth].sound_novel_quit.second_window].position.y
        = g_display_object_gs_image_pool[index].position.y - 2;
    g_menu_stack_depth = depth + 1;
    fade = &g_world_map_fade;
    fade->boxes[0].r = 0;
    fade->boxes[0].g = 0;
    fade->boxes[0].b = 0;
    fade->flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;
    g_menu_stack_records[depth].sound_novel_quit.choice = 0;
    g_menu_stack_records[depth].sound_novel_quit._unknown_18 = 0;
    g_menu_stack_records[depth].sound_novel_quit.fade_in = 0;
    g_menu_stack_records[depth].sound_novel_quit.fade_out = 0;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_SOUND_NOVEL_QUIT;
}

/* Resume handler of the Quit/Continue menu: default palettes and the idle
 * cursor again. */
void sound_novel_quit_resume(menu_stack_record_t* level) {
    s32 first = level->sound_novel_quit.cursor_window;
    s32 second = level->sound_novel_quit.second_window;
    s32 index;
    /* The original frame reserves eight unused bytes. */
    s32 scratch[2];

    g_display_object_gs_image_pool[level->sound_novel_quit.render_index].palette = 0;
    g_display_object_animated_pool[second].palette = 0;
    g_display_object_animated_pool[first].palette = 0;
    g_display_object_animated_pool[level->sound_novel_quit.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    index = level->sound_novel_quit.cursor_window;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
}

/* Gray out the Quit/Continue menu (palettes 10, 6 and 2) and still its cursor
 * while a help message opens over it. */
void sound_novel_quit_suspend(menu_stack_record_t* level) {
    s32 index;

    g_display_object_animated_pool[level->sound_novel_quit.cursor_window].palette = 10;
    g_display_object_animated_pool[level->sound_novel_quit.second_window].palette = 6;
    g_display_object_gs_image_pool[level->sound_novel_quit.render_index].palette = 2;
    g_display_object_animated_pool[level->sound_novel_quit.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_STILL;
    index = level->sound_novel_quit.cursor_window;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
}

/* Step of the sound novel's Quit/Continue menu.
 *
 * The screen dims behind it: fade_in raises the black box by 8 a frame to
 * 0x60, and fade_out (set by Cross or Continue) lowers it again and pops the
 * level at 0. While no fade runs: Select opens the highlighted row's help
 * (g_sound_novel_quit_help_text_ids), Up and Down toggle the rows,
 * and Circle on Quit stops the novel: input blocks, the menu's records go, the
 * message box stops and the music fades. The box then darkens to black; once
 * the message box has gone, the text history shifts down a slot (the cleared
 * first slot becomes -1), the play time display comes back and a screen
 * transition reopens the treasure detail.
 *
 * Every color store rereads the 32-bit fade value through a local: a direct
 * byte assignment directly from the fade field narrows the reload to `lbu`, while the
 * target keeps `lw`. The target stores the -1 history marker with a signed
 * `addiu`, so the first history slot is declared s16. The cursor
 * update needs the window pointer, the base_y local and a separate offset
 * local: that keeps base_y as the first `addu` operand and stops fold from
 * reassociating the +0xe. */
void sound_novel_quit_update(menu_stack_record_t* level) {
    u32 flags;
    u32 buttons;
    s32 value;
    s32 fade;
    s32 base;
    s32 offset;
    display_object_animated_t* record;
    s32 window;
    u16 first;

    if (level->sound_novel_quit.fade_in < 0x60) {
        level->sound_novel_quit.fade_in += 8;
        fade = level->sound_novel_quit.fade_in;
        g_world_map_fade.boxes[0].r = fade;
        fade = level->sound_novel_quit.fade_in;
        g_world_map_fade.boxes[0].g = fade;
        fade = level->sound_novel_quit.fade_in;
        g_world_map_fade.boxes[0].b = fade;
    }

    if (level->sound_novel_quit.fade_out != 0) {
        level->sound_novel_quit.fade_out -= 8;
        fade = level->sound_novel_quit.fade_out;
        g_world_map_fade.boxes[0].r = fade;
        fade = level->sound_novel_quit.fade_out;
        g_world_map_fade.boxes[0].g = fade;
        fade = level->sound_novel_quit.fade_out;
        g_world_map_fade.boxes[0].b = fade;
        if (level->sound_novel_quit.fade_out <= 0) {
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_world_map_fade.flags[0] ^= WORLD_MAP_FADE_BOX_VISIBLE;
            g_display_object_animated_count -= 2;
            g_display_object_image_count--;
            g_display_object_queue_count -= 3;
            g_menu_stack_depth--;
            menu_stack_dispatch_resume();
        }
        return;
    }

    if (g_display_object_gs_image_pool[level->sound_novel_quit.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    flags = g_world_map_flags;
    if (flags & WORLD_MAP_FLAG_FADING) {
        return;
    }

    if (flags & WORLD_MAP_FLAG_INPUT_BLOCKED) {
        if (level->sound_novel_quit.fade_in < 0xFF) {
            level->sound_novel_quit.fade_in += 8;
            if (level->sound_novel_quit.fade_in >= 0x100) {
                level->sound_novel_quit.fade_in = 0xFF;
            }
            fade = level->sound_novel_quit.fade_in;
            g_world_map_fade.boxes[0].r = fade;
            fade = level->sound_novel_quit.fade_in;
            g_world_map_fade.boxes[0].g = fade;
            fade = level->sound_novel_quit.fade_in;
            g_world_map_fade.boxes[0].b = fade;
            return;
        }
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        g_world_map_brightness_rgb.r = 0;
        g_world_map_brightness_rgb.g = 0;
        g_world_map_brightness_rgb.b = 0;
        g_world_map_fade.flags[0] |= WORLD_MAP_FADE_OUT;
        first = g_sound_novel_history_0;
        g_sound_novel_history_0 = -1;
        g_menu_blocked_buttons = 0;
        g_sound_novel_countdown_frames = 0;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_sound_novel_reading.text_history[3] = g_sound_novel_history_2;
        g_sound_novel_history_2 = g_sound_novel_history_1;
        g_sound_novel_history_1 = first;
        hud_restore();
        g_world_map_fade.priorities[0] = 0xF;
        g_world_map_fade.flags[1] = 0;
        g_display_object_animated_count -= 3;
        g_display_object_image_count -= 2;
        g_display_object_queue_count -= 5;
        g_menu_stack_depth -= 2;
        suzuki_sound_remove_resource(g_sound_novel_sound_resource);
        screen_transition_open(
            MENU_STACK_LEVEL_BRAVE_STORY_TREASURE, g_menu_stack_records[g_menu_stack_depth].sound_novel.treasure_id, 0);
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->sound_novel_quit.fade_out = level->sound_novel_quit.fade_in;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        sound_novel_quit_suspend(level);
        menu_stack_text_window_open(g_sound_novel_quit_help_text_ids[level->sound_novel_quit.choice] | 0x1000, 1);
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0 || world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        value = level->sound_novel_quit.choice ^ 1;
        level->sound_novel_quit.choice = value;
        record = &g_display_object_animated_pool[level->sound_novel_quit.cursor_window];
        base = g_display_object_gs_image_pool[level->sound_novel_quit.render_index].position.y;
        offset = value * 0x10 + 0xE;
        record->position.y = base + offset;
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_display_object_animated_pool[level->sound_novel_quit.cursor_window].sequence
            = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
        window = level->sound_novel_quit.cursor_window;
        g_display_object_animated_pool[window].anim_counter = 0;
        g_display_object_animated_pool[window].frame_index = 0;
        if (level->sound_novel_quit.choice == 0) {
            g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
            g_display_object_animated_count -= 2;
            g_display_object_image_count--;
            g_display_object_queue_count -= 3;
            if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
                screen_thread_request_stop(WORLD_MAP_THREAD_SLOT_MESSAGE);
            }
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x20);
            sound_all_sfx_stop();
            sound_play_effect(SOUND_SFX_CONFIRM);
            return;
        }
        level->sound_novel_quit.fade_out = level->sound_novel_quit.fade_in;
    }
}

/* Restore the saved HUD position and play-time fields, leaving the display hidden.
 * Cache the flags and time before publishing the display fields. */
static void hud_restore(void) {
    u32 flags;
    s32 hours;
    s32 minutes;
    s32 seconds;

    g_hud_value_position = g_hud_saved_value_position;
    g_hud_value_mode = HUD_VALUE_PLAY_TIME;
    g_hud_ot_priority = 1;
    flags = g_world_map_flags;
    hours = g_save_play_time_hours;
    minutes = g_save_play_time_minutes;
    seconds = g_save_play_time_seconds;
    g_world_map_flags = flags | WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
    g_hud_value = hours;
    g_hud_value_secondary = minutes;
    g_hud_value_tertiary = seconds;
}

/* Pushes the sound novel's text history (MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY).
 *
 * Suspends the message box, appends the upper and lower scroll indicators
 * (the lower one hidden) and the 0xf8 x 0xb0 text panel, draws the newest
 * history entry (TEXT_ID_WORLD_MESSAGE_BASE + text_history[3]) centered in it,
 * and copies the four history entries (newest first) into the level. It also
 * grays the novel's Start button window, sets the picture to full brightness
 * and shows the black fade box for the dim. The upper indicator is hidden
 * when there is no older entry.
 *
 * The level is written through the sound_novel_history member of the
 * stack-record union (as in quit_menu_open).
 *
 * Several spellings are load-bearing. `windows` holds the record base in a
 * saved register: the window positions are derived from it and the second
 * window's anim_counter store goes through it, while the other field stores use
 * the absolute `$at` form. The render position and color pointers reproduce
 * the target's shared render-record base. The history is read through a
 * pointer to its newest entry: the indexed reads count as in-struct
 * references, so the scheduler keeps each load after the preceding level
 * store. `top` and `depth` are separate locals for the target's register
 * assignment, and `unused` preserves the 0x50-byte frame. */
static void history_open(void) {
    RECT bounds;
    u8 unused[8];
    display_object_animated_t* windows;
    display_object_position_stride_t* positions;
    point32_t* position;
    CVECTOR* colors;
    s16* newest;
    s32 index;
    s32 depth;
    s32 top;

    thread_suspend(WORLD_MAP_THREAD_SLOT_MESSAGE);

    windows = g_display_object_animated_pool;
    positions = (display_object_position_stride_t*)&windows->position;
    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_history.upper_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_HISTORY_UPPER;
    g_display_object_animated_pool[index].priority = 9;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    positions[index].x = 0;
    positions[index].y = -0x6A;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_history.lower_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_HISTORY_LOWER;
    g_display_object_animated_pool[index].priority = 9;
    windows[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    positions[index].x = 0;
    positions[index].y = 0x5E;

    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_history.render_index = index;
    bounds.w = 0xF8;
    bounds.h = 0xB0;
    bounds.x = 0;
    bounds.y = 0;
    window_build_render_record_image(index, bounds, 1, g_window_image_buffer);
    position = &g_display_object_gs_image_pool[index].position;
    newest = &g_sound_novel_reading.text_history[3];
    g_display_object_gs_image_pool[index].priority = g_menu_stack_ot_offset;
    position->x = -0x80;
    position->y = -0x64;
    g_display_object_gs_image_pool[index].clut.x = 0x3C0;
    g_display_object_gs_image_pool[index].clut.y = 0x1F2;
    window_centered_text_draw(index, *newest + TEXT_ID_WORLD_MESSAGE_BASE);

    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].sound_novel.quit_prompt_window].palette
        = 0xA;
    /* Direct record.color access changes the column base and instruction scheduling. */
    colors = &g_display_object_gs_image_pool->color;
    ((CVECTOR*)((u8*)colors + g_sound_novel_reading.picture_render_index * sizeof(g_display_object_gs_image_pool[0])))
        ->r = 0x80;
    ((CVECTOR*)((u8*)colors + g_sound_novel_reading.picture_render_index * sizeof(g_display_object_gs_image_pool[0])))
        ->g = 0x80;
    ((CVECTOR*)((u8*)colors + g_sound_novel_reading.picture_render_index * sizeof(g_display_object_gs_image_pool[0])))
        ->b = 0x80;
    g_world_map_fade.boxes[0].r = 0;
    g_world_map_fade.boxes[0].g = 0;
    g_world_map_fade.boxes[0].b = 0;
    g_world_map_fade.flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;

    top = g_menu_stack_depth;
    g_menu_stack_records[top].sound_novel_history.scroll = 0;
    g_menu_stack_records[top].sound_novel_history.newest_text_id = *newest;
    g_menu_stack_records[top].sound_novel_history.text_ids[0] = newest[-1];
    g_menu_stack_records[top].sound_novel_history.text_ids[1] = newest[-2];
    g_menu_blocked_buttons = -1;
    g_menu_stack_records[top].sound_novel_history.text_ids[2] = newest[-3];
    if (g_menu_stack_records[top].sound_novel_history.text_ids[0] == -1) {
        g_display_object_animated_pool[g_menu_stack_records[top].sound_novel_history.upper_window].flags
            |= DISPLAY_OBJECT_HIDDEN;
    }

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].sound_novel_history.phase = SOUND_NOVEL_HISTORY_PHASE_DIM;
    g_menu_stack_records[depth].sound_novel_history.step = 0;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY;
    g_menu_stack_depth = depth + 1;
}

/* Step of the sound novel's text history.
 *
 * DIM darkens the picture (g_sound_novel_reading.picture_render_index)
 * and g_hud_color from 0x80 to 0x60 in five steps while the fade box
 * brightens. BROWSE scrolls the history with Up and Down, hiding each
 * indicator at its end, until Square is released. RESTORE runs the reverse
 * ramp and then, if g_sound_novel_reading.text_id names a text,
 * clears the panel image (width * height / 8 32-bit values), redraws that
 * text into it past its choice marks, uploads it and clears the pending
 * sound, weather and music variables; it then pops the level and resumes the
 * message box.
 *
 * Load-bearing spellings: the nonzero phases are a switch nested under
 * `phase != 0`, matching the target's test order; each fade case holds
 * &g_sound_novel_reading.picture_render_index in its own pointer local, and `shade` is shared between the
 * cases (a global pseudo, as in the target's $a2); the text box position is
 * read into `x`/`y` so the s32 point is loaded with `lw`; `unused` preserves
 * the 0x58-byte frame. */
void sound_novel_history_update(menu_stack_record_t* level) {
    glyph_blit_t box;
    point32_t point;
    u8 unused[0x20];
    u32* image;
    s16* index1;
    s16* index2;
    s32 shade;
    s32 i;
    s32 text_id;
    s32 step;
    s32 depth;
    s32 render_index;
    s32 x;
    s32 y;

    if (level->sound_novel_history.phase != SOUND_NOVEL_HISTORY_PHASE_BROWSE) {
        switch (level->sound_novel_history.phase) {
        case SOUND_NOVEL_HISTORY_PHASE_DIM:
            shade = 0x80 - level->sound_novel_history.step * 8;
            index1 = &g_sound_novel_reading.picture_render_index;
            g_display_object_gs_image_pool[*index1].color.r = shade;
            g_display_object_gs_image_pool[*index1].color.g = shade;
            g_display_object_gs_image_pool[*index1].color.b = shade;
            g_hud_color.r = shade;
            g_hud_color.g = shade;
            g_hud_color.b = shade;
            shade = level->sound_novel_history.step * 8;
            g_world_map_fade.boxes[0].r = shade;
            g_world_map_fade.boxes[0].g = shade;
            g_world_map_fade.boxes[0].b = shade;
            step = level->sound_novel_history.step + 1;
            level->sound_novel_history.step = step;
            if (step >= 5) {
                level->sound_novel_history.step = 0;
                level->sound_novel_history.phase = SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            }
            break;
        case SOUND_NOVEL_HISTORY_PHASE_RESTORE:
            index2 = &g_sound_novel_reading.picture_render_index;
            shade = level->sound_novel_history.step * 0x10 + 0x40;
            g_display_object_gs_image_pool[*index2].color.r = shade;
            g_display_object_gs_image_pool[*index2].color.g = shade;
            g_display_object_gs_image_pool[*index2].color.b = shade;
            g_hud_color.r = shade;
            g_hud_color.g = shade;
            g_hud_color.b = shade;
            shade = 0x20 - level->sound_novel_history.step * 8;
            g_world_map_fade.boxes[0].r = shade;
            g_world_map_fade.boxes[0].g = shade;
            g_world_map_fade.boxes[0].b = shade;
            step = level->sound_novel_history.step + 1;
            level->sound_novel_history.step = step;
            if (step < 5) {
                break;
            }
            text_id = g_sound_novel_reading.text_id;
            if (text_id != -1) {
                text_id += TEXT_ID_WORLD_MESSAGE_BASE;
                i = 0;
                image = g_window_image_buffer;
                point.x = 8;
                point.y = 8;
                for (;;) {
                    render_index = level->sound_novel_history.render_index;
                    if (i >= g_display_object_gs_image_pool[render_index].bounds.w
                            * g_display_object_gs_image_pool[render_index].bounds.h / 8) {
                        break;
                    }
                    i++;
                    *image = 0;
                    image++;
                }
                x = point.x;
                y = point.y;
                box.rect.x = x;
                box.rect.y = y;
                box.stride = g_display_object_gs_image_pool[level->sound_novel_history.render_index].bounds.w;
                menu_display_text_entry_past_choice_marks(text_id, g_window_image_buffer, &box);
                window_load_render_record_image(level->sound_novel_history.render_index, g_window_image_buffer);
                script_variable_set(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
                script_variable_set(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
                script_variable_set(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
            }
            level->sound_novel_history.step = 0;
            level->sound_novel_history.phase = SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_world_map_fade.flags[0] ^= WORLD_MAP_FADE_BOX_VISIBLE;
            depth = g_menu_stack_depth;
            g_display_object_animated_count -= 2;
            g_display_object_image_count--;
            g_display_object_queue_count -= 3;
            g_menu_stack_depth = depth - 1;
            g_display_object_animated_pool[g_menu_stack_records[depth - 2].sound_novel.quit_prompt_window].palette = 0;
            thread_resume(WORLD_MAP_THREAD_SLOT_MESSAGE);
            break;
        }
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0) {
        if (level->sound_novel_history.scroll < 3
            && level->sound_novel_history.text_ids[level->sound_novel_history.scroll] != -1) {
            history_entry_draw(&level->sound_novel_history, 4);
            level->sound_novel_history.scroll = level->sound_novel_history.scroll + 1;
            if (level->sound_novel_history.scroll == 3
                || level->sound_novel_history.text_ids[level->sound_novel_history.scroll] == -1) {
                g_display_object_animated_pool[level->sound_novel_history.upper_window].flags |= DISPLAY_OBJECT_HIDDEN;
            } else {
                g_display_object_animated_pool[level->sound_novel_history.upper_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
            }
            g_display_object_animated_pool[level->sound_novel_history.lower_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
    }

    if (world_map_input_repeat_update(PSX_PAD_DOWN) != 0 && level->sound_novel_history.scroll != 0) {
        history_entry_draw(&level->sound_novel_history, 2);
        level->sound_novel_history.scroll = level->sound_novel_history.scroll - 1;
        if (level->sound_novel_history.scroll == 0) {
            g_display_object_animated_pool[level->sound_novel_history.lower_window].flags |= DISPLAY_OBJECT_HIDDEN;
        } else {
            g_display_object_animated_pool[level->sound_novel_history.lower_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
        g_display_object_animated_pool[level->sound_novel_history.upper_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
    }

    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        level->sound_novel_history.phase = SOUND_NOVEL_HISTORY_PHASE_RESTORE;
    }
}

/* Draw the history entry at 32-bit slot `index + scroll` of the level (counted
 * from the level's start) centered in the panel; an empty entry (-1) draws
 * nothing. */
static void history_entry_draw(sound_novel_history_level_t* level, s32 index) {
    /* Named text-array access reverses the final address-add operands. */
    s32 text_id = ((s32*)level)[index + level->scroll];

    if (text_id != -1) {
        window_centered_text_draw(level->render_index, text_id + TEXT_ID_WORLD_MESSAGE_BASE);
    }
}
