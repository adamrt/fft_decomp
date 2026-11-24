/* The sound-novel reader opens from a treasure and manages its quit prompt and reading history. */

#include "fft/gfx/display_object.h"
#include "fft/gfx/gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_screen.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/input.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/suzuki.h"
#include "fft/sys/thread.h"
#include "fft/text/text.h"
#include "fft/world/menu_stack.h"
#include "fft/world/menu_stack_records.h"
#include "fft/world/novel.h"
#include "fft/world/save.h"
#include "fft/world/window.h"
#include "fft/world/world_map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_novel_quit_help_text_ids[];
static point32_t s_hud_saved_value_position;

/* Scalar history views retained for the Quit shuffle: direct member accesses
 * change its original loads and store scheduling. */
extern s16 g_novel_history_0;
extern u16 g_novel_history_2;

static void open_quit_menu(void);
static void restore_hud(void);
static void open_history(void);
static void draw_history_entry(novel_history_level_t* level, s32 index);

/*
 * Pushes the sound-novel menu level (MENU_STACK_LEVEL_SOUND_NOVEL).
 * `argument` is kept in the level and names the treasure detail reopened
 * when the novel ends; the novel read is the one in
 * g_novel_selected.
 *
 * Hides the context value display, clears VRAM and the scratch buffer, and
 * appends the level's display objects, all hidden: the two button prompts,
 * the VRAM strip record and the overlay. It then appends the novel's sound
 * effect data, restores any saved reading state into the first render record
 * (novel_restore_reading), blocks Cross, Square and Select
 * for the menu and starts the screen fade.
 */
void novel_open_reader(s32 argument) {
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
    s_hud_saved_value_position = g_hud_value_position;
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
    world_map_clear_image_buffers(1);
    g_menu_stack_records[g_menu_stack_depth].novel.treasure_id = argument;

    bar_render = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel.quit_prompt_window = index;
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

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel.history_prompt_window = index;
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

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel.brightness_image = index;
    bounds.x = 484;
    bounds.y = 488;
    bounds.w = 32;
    bounds.h = 8;
    origin.x = 32;
    origin.y = 488;
    display_object_init_image(index, bounds, origin, 0);
    pos3 = (point32_t*)((u8*)render_pos + index * sizeof(display_object_gs_image_t));
    pos3->x = 0x34;
    pos3->y = 0x55;
    g_display_object_gs_image_pool[index].priority = 8;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_UNTINTED;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel.brightness_overlay_window = index;
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

    suzuki_sound_append_resource(g_novel_sound_resource);
    novel_restore_reading(bar_render);
    g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    g_world_map_fade.priorities[0] = 8;
    g_world_map_fade.flags[1] |= WORLD_MAP_FADE_BOX_VISIBLE;
    g_menu_stack_records[g_menu_stack_depth].novel._unread_20 = 1;
    g_menu_stack_records[g_menu_stack_depth].novel.countdown = 9999;
    g_menu_stack_records[g_menu_stack_depth].novel.fade_timer = 0;
    g_menu_stack_records[g_menu_stack_depth].novel.phase = NOVEL_PHASE_RUN;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_SOUND_NOVEL;
    g_menu_stack_depth++;
    world_map_start_fade(0, 0x10);
}

/* Resume handler of the sound novel level: reload the saved background set, show the picture again
 * (and the side windows when the script left them visible) and resume the message thread. */
void novel_resume_reader(menu_stack_record_t* level) {
    s16 set = g_novel_reading.background_set;

    if (set != -1) {
        world_map_load_background_picture_set(set);
        file_wait_pending();
    }
    g_display_object_gs_image_pool[g_novel_reading.picture_render_index].flags &= ~DISPLAY_OBJECT_BLANK;
    if (g_novel_reading.state_flags & NOVEL_STATE_PARENT_WINDOWS_VISIBLE) {
        g_display_object_animated_pool[level->novel.quit_prompt_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->novel.history_prompt_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
    }
    thread_resume(WORLD_MAP_THREAD_SLOT_MESSAGE);
}

/* The g_level_suspend_handlers entry of MENU_STACK_LEVEL_SOUND_NOVEL: nothing to suspend. */
void novel_suspend_reader(menu_stack_record_t* unused_level) {
}

/*
 * Per-frame step of the sound novel (MENU_STACK_LEVEL_SOUND_NOVEL).
 *
 * It runs the novel's script until it yields (novel_run_script).
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
void novel_update_reader(menu_stack_record_t* level) {
    s32 unused[2];
    world_map_fade_t* fade;
    u32 buttons;
    s32 width;
    s32 window;
    u16 shade;

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && !(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
        switch (level->novel.phase) {
        case NOVEL_PHASE_OPEN_HISTORY:
            if (level->novel.countdown == 0) {
                g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
                open_history();
                return;
            }
            level->novel.countdown--;
            return;
        case NOVEL_PHASE_END:
            if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
                return;
            }
            g_menu_blocked_buttons = 0;
            g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
            restore_hud();
            fade = &g_world_map_fade;
            g_world_map_fade.priorities[0] = 0xF;
            g_world_map_fade.flags[1] = 0;
            g_display_object_animated_count -= 3;
            g_display_object_image_count -= 2;
            g_display_object_queue_count -= 5;
            g_menu_stack_depth--;
            fade->flags[0] = (fade->flags[0] & ~WORLD_MAP_FADE_BOX_VISIBLE) | WORLD_MAP_FADE_OUT;
            sound_stop_all_sfx();
            suzuki_sound_remove_resource(g_novel_sound_resource);
            novel_reset_selected_reading(g_novel_selected);
            g_novel_countdown_frames = 0;
            world_map_open_screen_transition(MENU_STACK_LEVEL_BRAVE_STORY_TREASURE, level->novel.treasure_id, 0);
            return;
        }
    }

    if (g_novel_reading.state_flags & NOVEL_STATE_END) {
        return;
    }

    if (novel_run_script() != 0) {
        if (g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1)) {
            if (g_input_current_buttons & PSX_PAD_L1) {
                if (g_novel_reading.brightness < 0xFF) {
                    g_novel_reading.brightness++;
                }
            }
            if (g_input_current_buttons & PSX_PAD_R1) {
                if (g_novel_reading.brightness != 0) {
                    g_novel_reading.brightness--;
                }
            }
            shade = g_novel_reading.brightness;
            g_world_map_fade.boxes[1].r = shade;
            g_world_map_fade.boxes[1].g = shade;
            g_world_map_fade.boxes[1].b = shade;
            level->novel.fade_timer = 0x3C;
        }

        if (level->novel.fade_timer != 0) {
            g_display_object_gs_image_pool[level->novel.brightness_image].flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[level->novel.brightness_overlay_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
            width = 0x100 - g_novel_reading.brightness;
            g_display_object_gs_image_pool[level->novel.brightness_image].bounds.w = width / 4;
            level->novel.fade_timer--;
        } else {
            g_display_object_gs_image_pool[level->novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[level->novel.brightness_overlay_window].flags |= DISPLAY_OBJECT_HIDDEN;
        }

        if (menu_screen_is_dialog_waiting() != 0) {
            buttons = g_world_map_input_new_button_presses;
            if (buttons & PSX_PAD_CIRCLE) {
                return;
            }
            g_display_object_animated_pool[level->novel.quit_prompt_window].palette
                = g_display_object_animated_pool[level->novel.history_prompt_window].palette = 0;
            if (buttons & PSX_PAD_SELECT) {
                menu_stack_open_message(0x1036, 1);
                return;
            }
            if ((buttons & PSX_PAD_START) && (g_novel_reading.state_flags & NOVEL_STATE_PARENT_WINDOWS_VISIBLE)
                && !(g_novel_reading.state_flags & NOVEL_STATE_WHITE_OVERLAY)) {
                g_display_object_animated_pool[level->novel.quit_prompt_window].sequence
                    = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_START_BUTTON_PRESS;
                g_display_object_animated_pool[level->novel.quit_prompt_window].frame_index
                    = g_display_object_animated_pool[level->novel.quit_prompt_window].anim_counter = 0;
                level->novel.fade_timer = 0;
                g_display_object_gs_image_pool[level->novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
                g_display_object_animated_pool[level->novel.brightness_overlay_window].flags |= DISPLAY_OBJECT_HIDDEN;
                open_quit_menu();
                return;
            }
            if ((g_input_current_buttons & PSX_PAD_SQUARE)
                && (g_novel_reading.state_flags & NOVEL_STATE_PARENT_WINDOWS_VISIBLE)
                && g_novel_reading.text_history[3] != -1) {
                g_display_object_animated_pool[level->novel.history_prompt_window].sequence
                    = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_SQUARE_BUTTON_PRESS;
                window = level->novel.history_prompt_window;
                g_display_object_animated_pool[window].anim_counter = 0;
                g_display_object_animated_pool[window].frame_index = 0;
                level->novel.fade_timer = 0;
                g_display_object_gs_image_pool[level->novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
                g_display_object_animated_pool[level->novel.brightness_overlay_window].flags |= DISPLAY_OBJECT_HIDDEN;
                g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
                level->novel.phase = NOVEL_PHASE_OPEN_HISTORY;
                level->novel.countdown = 8;
            }
        } else {
            g_display_object_animated_pool[level->novel.quit_prompt_window].palette
                = g_display_object_animated_pool[level->novel.history_prompt_window].palette = 0xA;
        }
    } else {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            menu_screen_thread_request_stop(WORLD_MAP_THREAD_SLOT_MESSAGE);
        }
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x20);
        sound_stop_all_sfx();
        level->novel.phase = NOVEL_PHASE_END;
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        if (!(g_novel_reading.state_flags & NOVEL_STATE_WHITE_OVERLAY)) {
            world_map_start_fade(WORLD_MAP_FADE_OUT, 0x10);
        }
    }
}

/* Pushes the sound novel's Quit/Continue menu (MENU_STACK_LEVEL_SOUND_NOVEL_QUIT):
 * the cursor and side label windows and a 0x3c x 0x30 TEXT_ID_QUIT_CONTINUE
 * record at (-0x1e, -0x18), with the windows placed on it. Blocks every
 * button, shows the fade box black (the step dims the screen with it) and
 * starts on Quit with no fade.
 *
 * The level is written through the novel_quit member of the stack-record
 * union; union field access keeps the target's per-field `$at` bases. The
 * fade state is written through a pointer, which keeps the
 * target's register base for the offset-0 32-bit flags. The unused array
 * preserves the target's 0x68-byte frame. */
static void open_quit_menu(void) {
    RECT bounds;
    u8 unused[0x28];
    point32_t* base;
    s32 index;
    s32 depth;
    world_map_fade_t* fade;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel_quit.cursor_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel_quit.second_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_LIST_SIDE_LABEL;
    g_display_object_animated_pool[index].priority = 8;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel_quit.render_index = index;
    bounds.x = 128;
    bounds.w = 60;
    bounds.h = 48;
    bounds.y = 0;
    window_init_text_record(index, bounds, 0, TEXT_ID_QUIT_CONTINUE, g_window_image_buffer);
    base = &g_display_object_gs_image_pool[index].position;
    g_display_object_gs_image_pool[index].priority = 8;
    depth = g_menu_stack_depth;
    base->x = -(bounds.w / 2);
    base->y = -0x18;
    g_display_object_animated_pool[g_menu_stack_records[depth].novel_quit.cursor_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 6;
    g_display_object_animated_pool[g_menu_stack_records[depth].novel_quit.cursor_window].position.y
        = g_display_object_gs_image_pool[index].position.y + 0xE;
    g_display_object_animated_pool[g_menu_stack_records[depth].novel_quit.second_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 4;
    g_menu_blocked_buttons = -1;
    g_display_object_animated_pool[g_menu_stack_records[depth].novel_quit.second_window].position.y
        = g_display_object_gs_image_pool[index].position.y - 2;
    g_menu_stack_depth = depth + 1;
    fade = &g_world_map_fade;
    fade->boxes[0].r = 0;
    fade->boxes[0].g = 0;
    fade->boxes[0].b = 0;
    fade->flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;
    g_menu_stack_records[depth].novel_quit.choice = 0;
    g_menu_stack_records[depth].novel_quit._unread_18 = 0;
    g_menu_stack_records[depth].novel_quit.fade_in = 0;
    g_menu_stack_records[depth].novel_quit.fade_out = 0;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_SOUND_NOVEL_QUIT;
}

/* Resume handler of the Quit/Continue menu: default palettes and the idle
 * cursor again. */
void novel_resume_quit_menu(menu_stack_record_t* level) {
    s32 cursor_window = level->novel_quit.cursor_window;
    s32 second_window = level->novel_quit.second_window;
    s32 index;
    /* The original frame reserves eight unused bytes. */
    s32 scratch[2];

    g_display_object_gs_image_pool[level->novel_quit.render_index].palette = 0;
    g_display_object_animated_pool[second_window].palette = 0;
    g_display_object_animated_pool[cursor_window].palette = 0;
    g_display_object_animated_pool[level->novel_quit.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    index = level->novel_quit.cursor_window;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
}

/* Gray out the Quit/Continue menu (palettes 10, 6 and 2) and still its cursor
 * while a help message opens over it. */
void novel_suspend_quit_menu(menu_stack_record_t* level) {
    s32 index;

    g_display_object_animated_pool[level->novel_quit.cursor_window].palette = 10;
    g_display_object_animated_pool[level->novel_quit.second_window].palette = 6;
    g_display_object_gs_image_pool[level->novel_quit.render_index].palette = 2;
    g_display_object_animated_pool[level->novel_quit.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_STILL;
    index = level->novel_quit.cursor_window;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
}

/* Step of the sound novel's Quit/Continue menu.
 *
 * The screen dims behind it: fade_in raises the black box by 8 a frame to
 * 0x60, and fade_out (set by Cross or Continue) lowers it again and pops the
 * level at 0. While no fade runs: Select opens the highlighted row's help
 * (g_novel_quit_help_text_ids), Up and Down toggle the rows,
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
void novel_update_quit_menu(menu_stack_record_t* level) {
    u32 flags;
    u32 buttons;
    s32 value;
    s32 fade;
    s32 base;
    s32 offset;
    display_object_animated_t* record;
    s32 window;
    u16 first;

    if (level->novel_quit.fade_in < 0x60) {
        level->novel_quit.fade_in += 8;
        fade = level->novel_quit.fade_in;
        g_world_map_fade.boxes[0].r = fade;
        fade = level->novel_quit.fade_in;
        g_world_map_fade.boxes[0].g = fade;
        fade = level->novel_quit.fade_in;
        g_world_map_fade.boxes[0].b = fade;
    }

    if (level->novel_quit.fade_out != 0) {
        level->novel_quit.fade_out -= 8;
        fade = level->novel_quit.fade_out;
        g_world_map_fade.boxes[0].r = fade;
        fade = level->novel_quit.fade_out;
        g_world_map_fade.boxes[0].g = fade;
        fade = level->novel_quit.fade_out;
        g_world_map_fade.boxes[0].b = fade;
        if (level->novel_quit.fade_out <= 0) {
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

    if (g_display_object_gs_image_pool[level->novel_quit.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    flags = g_world_map_flags;
    if (flags & WORLD_MAP_FLAG_FADING) {
        return;
    }

    if (flags & WORLD_MAP_FLAG_INPUT_BLOCKED) {
        if (level->novel_quit.fade_in < 0xFF) {
            level->novel_quit.fade_in += 8;
            if (level->novel_quit.fade_in >= 0x100) {
                level->novel_quit.fade_in = 0xFF;
            }
            fade = level->novel_quit.fade_in;
            g_world_map_fade.boxes[0].r = fade;
            fade = level->novel_quit.fade_in;
            g_world_map_fade.boxes[0].g = fade;
            fade = level->novel_quit.fade_in;
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
        first = g_novel_history_0;
        g_novel_history_0 = -1;
        g_menu_blocked_buttons = 0;
        g_novel_countdown_frames = 0;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_novel_reading.text_history[3] = g_novel_history_2;
        g_novel_history_2 = g_novel_history_1;
        g_novel_history_1 = first;
        restore_hud();
        g_world_map_fade.priorities[0] = 0xF;
        g_world_map_fade.flags[1] = 0;
        g_display_object_animated_count -= 3;
        g_display_object_image_count -= 2;
        g_display_object_queue_count -= 5;
        g_menu_stack_depth -= 2;
        suzuki_sound_remove_resource(g_novel_sound_resource);
        world_map_open_screen_transition(
            MENU_STACK_LEVEL_BRAVE_STORY_TREASURE, g_menu_stack_records[g_menu_stack_depth].novel.treasure_id, 0);
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->novel_quit.fade_out = level->novel_quit.fade_in;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        novel_suspend_quit_menu(level);
        menu_stack_open_text_window(g_novel_quit_help_text_ids[level->novel_quit.choice] | 0x1000, 1);
        return;
    }

    if (input_update_repeat(PSX_PAD_UP) != 0 || input_update_repeat(PSX_PAD_DOWN) != 0) {
        value = level->novel_quit.choice ^ 1;
        level->novel_quit.choice = value;
        record = &g_display_object_animated_pool[level->novel_quit.cursor_window];
        base = g_display_object_gs_image_pool[level->novel_quit.render_index].position.y;
        offset = value * 0x10 + 0xE;
        record->position.y = base + offset;
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_display_object_animated_pool[level->novel_quit.cursor_window].sequence = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
        window = level->novel_quit.cursor_window;
        g_display_object_animated_pool[window].anim_counter = 0;
        g_display_object_animated_pool[window].frame_index = 0;
        if (level->novel_quit.choice == 0) {
            g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
            g_display_object_animated_count -= 2;
            g_display_object_image_count--;
            g_display_object_queue_count -= 3;
            if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
                menu_screen_thread_request_stop(WORLD_MAP_THREAD_SLOT_MESSAGE);
            }
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x20);
            sound_stop_all_sfx();
            sound_play_effect(SOUND_SFX_CONFIRM);
            return;
        }
        level->novel_quit.fade_out = level->novel_quit.fade_in;
    }
}

/* Restore the saved HUD position and play-time fields, leaving the display hidden.
 * Cache the flags and time before publishing the display fields. */
static void restore_hud(void) {
    u32 flags;
    s32 hours;
    s32 minutes;
    s32 seconds;

    g_hud_value_position = s_hud_saved_value_position;
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
 * The level is written through the novel_history member of the
 * stack-record union (as in open_quit_menu).
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
static void open_history(void) {
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
    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel_history.upper_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_HISTORY_UPPER;
    g_display_object_animated_pool[index].priority = 9;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    positions[index].x = 0;
    positions[index].y = -106;

    index = display_object_append_animated(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel_history.lower_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_SOUND_NOVEL_HISTORY_LOWER;
    g_display_object_animated_pool[index].priority = 9;
    windows[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    positions[index].x = 0;
    positions[index].y = 94;

    index = display_object_append_image(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].novel_history.render_index = index;
    bounds.w = 248;
    bounds.h = 176;
    bounds.x = 0;
    bounds.y = 0;
    window_build_render_record_image(index, bounds, 1, g_window_image_buffer);
    position = &g_display_object_gs_image_pool[index].position;
    newest = &g_novel_reading.text_history[3];
    g_display_object_gs_image_pool[index].priority = g_menu_stack_ot_offset;
    position->x = -0x80;
    position->y = -0x64;
    g_display_object_gs_image_pool[index].clut.x = 960;
    g_display_object_gs_image_pool[index].clut.y = 498;
    novel_draw_history_text(index, *newest + TEXT_ID_WORLD_MESSAGE_BASE);

    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].novel.quit_prompt_window].palette = 0xA;
    /* Direct record.color access changes the column base and instruction scheduling. */
    colors = &g_display_object_gs_image_pool->color;
    ((CVECTOR*)((u8*)colors + g_novel_reading.picture_render_index * sizeof(g_display_object_gs_image_pool[0])))->r
        = 0x80;
    ((CVECTOR*)((u8*)colors + g_novel_reading.picture_render_index * sizeof(g_display_object_gs_image_pool[0])))->g
        = 0x80;
    ((CVECTOR*)((u8*)colors + g_novel_reading.picture_render_index * sizeof(g_display_object_gs_image_pool[0])))->b
        = 0x80;
    g_world_map_fade.boxes[0].r = 0;
    g_world_map_fade.boxes[0].g = 0;
    g_world_map_fade.boxes[0].b = 0;
    g_world_map_fade.flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;

    top = g_menu_stack_depth;
    g_menu_stack_records[top].novel_history.scroll = 0;
    g_menu_stack_records[top].novel_history.newest_text_id = *newest;
    g_menu_stack_records[top].novel_history.text_ids[0] = newest[-1];
    g_menu_stack_records[top].novel_history.text_ids[1] = newest[-2];
    g_menu_blocked_buttons = -1;
    g_menu_stack_records[top].novel_history.text_ids[2] = newest[-3];
    if (g_menu_stack_records[top].novel_history.text_ids[0] == -1) {
        g_display_object_animated_pool[g_menu_stack_records[top].novel_history.upper_window].flags
            |= DISPLAY_OBJECT_HIDDEN;
    }

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].novel_history.phase = NOVEL_HISTORY_PHASE_DIM;
    g_menu_stack_records[depth].novel_history.step = 0;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY;
    g_menu_stack_depth = depth + 1;
}

/* Step of the sound novel's text history.
 *
 * DIM darkens the picture (g_novel_reading.picture_render_index)
 * and g_hud_color from 0x80 to 0x60 in five steps while the fade box
 * brightens. BROWSE scrolls the history with Up and Down, hiding each
 * indicator at its end, until Square is released. RESTORE runs the reverse
 * ramp and then, if g_novel_reading.text_id names a text,
 * clears the panel image (width * height / 8 32-bit values), redraws that
 * text into it past its choice marks, uploads it and clears the pending
 * sound, weather and music variables; it then pops the level and resumes the
 * message box.
 *
 * Load-bearing spellings: the nonzero phases are a switch nested under
 * `phase != 0`, matching the target's test order; each fade case holds
 * &g_novel_reading.picture_render_index in its own pointer local, and `shade` is shared between the
 * cases (a global pseudo, as in the target's $a2); the text box position is
 * read into `x`/`y` so the s32 point is loaded with `lw`; `unused` preserves
 * the 0x58-byte frame. */
void novel_update_history(menu_stack_record_t* level) {
    glyph_blit_t box;
    point32_t point;
    u8 unused[0x20];
    u32* image;
    s16* dim_picture;
    s16* restore_picture;
    s32 shade;
    s32 i;
    s32 text_id;
    s32 step;
    s32 depth;
    s32 x;
    s32 y;

    if (level->novel_history.phase != NOVEL_HISTORY_PHASE_BROWSE) {
        switch (level->novel_history.phase) {
        case NOVEL_HISTORY_PHASE_DIM:
            shade = 0x80 - level->novel_history.step * 8;
            dim_picture = &g_novel_reading.picture_render_index;
            g_display_object_gs_image_pool[*dim_picture].color.r = shade;
            g_display_object_gs_image_pool[*dim_picture].color.g = shade;
            g_display_object_gs_image_pool[*dim_picture].color.b = shade;
            g_hud_color.r = shade;
            g_hud_color.g = shade;
            g_hud_color.b = shade;
            shade = level->novel_history.step * 8;
            g_world_map_fade.boxes[0].r = shade;
            g_world_map_fade.boxes[0].g = shade;
            g_world_map_fade.boxes[0].b = shade;
            step = level->novel_history.step + 1;
            level->novel_history.step = step;
            if (step >= 5) {
                level->novel_history.step = 0;
                level->novel_history.phase = NOVEL_HISTORY_PHASE_BROWSE;
            }
            break;
        case NOVEL_HISTORY_PHASE_RESTORE:
            restore_picture = &g_novel_reading.picture_render_index;
            shade = level->novel_history.step * 0x10 + 0x40;
            g_display_object_gs_image_pool[*restore_picture].color.r = shade;
            g_display_object_gs_image_pool[*restore_picture].color.g = shade;
            g_display_object_gs_image_pool[*restore_picture].color.b = shade;
            g_hud_color.r = shade;
            g_hud_color.g = shade;
            g_hud_color.b = shade;
            shade = 0x20 - level->novel_history.step * 8;
            g_world_map_fade.boxes[0].r = shade;
            g_world_map_fade.boxes[0].g = shade;
            g_world_map_fade.boxes[0].b = shade;
            step = level->novel_history.step + 1;
            level->novel_history.step = step;
            if (step < 5) {
                break;
            }
            text_id = g_novel_reading.text_id;
            if (text_id != -1) {
                text_id += TEXT_ID_WORLD_MESSAGE_BASE;
                i = 0;
                image = g_window_image_buffer;
                point.x = 8;
                point.y = 8;
                while (i < g_display_object_gs_image_pool[level->novel_history.render_index].bounds.w
                        * g_display_object_gs_image_pool[level->novel_history.render_index].bounds.h / 8) {
                    i++;
                    *image++ = 0;
                }
                x = point.x;
                y = point.y;
                box.rect.x = x;
                box.rect.y = y;
                box.stride = g_display_object_gs_image_pool[level->novel_history.render_index].bounds.w;
                text_render_entry_past_choice_marks(text_id, g_window_image_buffer, &box);
                window_load_render_record_image(level->novel_history.render_index, g_window_image_buffer);
                script_variable_set(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
                script_variable_set(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
                script_variable_set(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
            }
            level->novel_history.step = 0;
            level->novel_history.phase = NOVEL_HISTORY_PHASE_BROWSE;
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_world_map_fade.flags[0] ^= WORLD_MAP_FADE_BOX_VISIBLE;
            depth = g_menu_stack_depth;
            g_display_object_animated_count -= 2;
            g_display_object_image_count--;
            g_display_object_queue_count -= 3;
            g_menu_stack_depth = depth - 1;
            g_display_object_animated_pool[g_menu_stack_records[depth - 2].novel.quit_prompt_window].palette = 0;
            thread_resume(WORLD_MAP_THREAD_SLOT_MESSAGE);
            break;
        }
        return;
    }

    if (input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->novel_history.scroll < 3 && level->novel_history.text_ids[level->novel_history.scroll] != -1) {
            draw_history_entry(&level->novel_history, 4);
            level->novel_history.scroll++;
            if (level->novel_history.scroll == 3 || level->novel_history.text_ids[level->novel_history.scroll] == -1) {
                g_display_object_animated_pool[level->novel_history.upper_window].flags |= DISPLAY_OBJECT_HIDDEN;
            } else {
                g_display_object_animated_pool[level->novel_history.upper_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
            }
            g_display_object_animated_pool[level->novel_history.lower_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
    }

    if (input_update_repeat(PSX_PAD_DOWN) != 0 && level->novel_history.scroll != 0) {
        draw_history_entry(&level->novel_history, 2);
        level->novel_history.scroll--;
        if (level->novel_history.scroll == 0) {
            g_display_object_animated_pool[level->novel_history.lower_window].flags |= DISPLAY_OBJECT_HIDDEN;
        } else {
            g_display_object_animated_pool[level->novel_history.lower_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
        g_display_object_animated_pool[level->novel_history.upper_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
    }

    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        level->novel_history.phase = NOVEL_HISTORY_PHASE_RESTORE;
    }
}

/* Draw the history entry at 32-bit slot `index + scroll` of the level (counted
 * from the level's start) centered in the panel; an empty entry (-1) draws
 * nothing. */
static void draw_history_entry(novel_history_level_t* level, s32 index) {
    /* Named text-array access reverses the final address-add operands. */
    s32 text_id = ((s32*)level)[index + level->scroll];

    if (text_id != -1) {
        novel_draw_history_text(level->render_index, text_id + TEXT_ID_WORLD_MESSAGE_BASE);
    }
}
