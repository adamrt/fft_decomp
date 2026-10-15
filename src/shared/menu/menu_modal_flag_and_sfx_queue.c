#include "fft/battle_menu.h"
#include "fft/main.h"
#include "fft/sound.h"

/* Sets the "an options/menu overlay is on screen" flag; the matching clear is
 * battle_menu_clear_modal_open. */
void menu_set_modal_open(void) {
    g_menu_modal_open = 1;
}

/* Clears the "an options/menu overlay is on screen" flag. Called from the
 * BATTLE.BIN overlay loaders (EQUIP.OUT, DEBUGCHR.OUT, ...) once the overlay
 * has been torn down. */
void menu_clear_modal_open(void) {
    g_menu_modal_open = 0;
}

void sound_queue_sfx_confirm(void) {
    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
}

void sound_queue_sfx_cancel(void) {
    g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
}

void sound_queue_sfx_cursor_move(void) {
    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
}

void sound_queue_sfx_invalid(void) {
    g_sound_pending_sfx_id = MAIN_SFX_INVALID;
}
