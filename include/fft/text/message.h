#ifndef MESSAGE_H
#define MESSAGE_H

/* Event messages: the dialog thread and its typewriter, timed messages, WORLD's message box, the ability announcement
 * and spell quote (announcement_), and the blocks of WLDCORE's message file WLDMES.BIN. */

#include "fft/text/text.h"
#include "psx/types.h"

struct action_command;

/* dialog */
extern u16 g_message_speaker_swap_unit_a;
extern u16 g_message_speaker_swap_unit_b;

void message_render_record_image(text_dialog_record_t* record);
void message_measure_window_width(s32 offset, s32 flags, s32* out_width);

/* BATTLE and WORLD each have their own body. */
void message_dialog_thread(void);

/* typewriter */
extern text_glyph_state_t g_message_typewriter_glyph;

/* Frees the slot the current thread owns; a closing dialog thread calls it. */
void message_release_typewriter_slot(void);

/* Draws g_message_typewriter_glyph into VRAM, then paces the message: delay 0 waits for the draw, 2 or more waits
 * that many steps per glyph and 1 follows the message speed option. */
void message_draw_glyph_timed(s32 pen_x, s32 pen_y, s32 delay);

/* timed messages */
void message_set_duration(u16 frames);

/* message box */
/* WORLD's typewriter message-box thread. */
void message_box_thread(void);

/* Sound-assisted display; WORLD and EQUIP each define it. */
void message_box_show_with_sound(s32 message_id, s32 sound_id);

/* announcements */
void announcement_spell_quote_thread(void);

/* Picks the spell quote or the plain ability-name announcement for an action. */
void announcement_select(struct action_command* action, s32 unit_id, s32 enabled);

/* message file */
/* Blocks of the message file WLDMES.BIN (message_load_block_if_changed). */
typedef enum message_block {
    MESSAGE_BLOCK_RUMORS = 0,
    MESSAGE_BLOCK_PROPOSITIONS = 1,
    MESSAGE_BLOCK_UNEXPLORED_LANDS = 2,
    MESSAGE_BLOCK_TREASURES = 3,
    MESSAGE_BLOCK_STORY_EVENTS = 4,
    MESSAGE_BLOCK_TUTORIAL_TOPICS = 5,
    MESSAGE_BLOCK_BRAVE_STORY_PROPOSITIONS = 10,
    MESSAGE_BLOCK_FACTS = 11,
    MESSAGE_BLOCK_PROPOSITION_REPORT_FIRST = 16 /* one block per proposition id */
} message_block_e;

extern s32 g_message_block_loaded; /* the block in s_message_block_buffer */

void message_load_block_if_changed(s32 block);

#endif
