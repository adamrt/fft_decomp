#ifndef NAME_ENTRY_H
#define NAME_ENTRY_H

/* The name-entry screen and the keyboard embedded in shop menus. */

#include "psx/types.h"

u8* name_entry_run_screen(const u8* list, u32* image);
u8* name_entry_keyboard_frame(const u8* list, s32 unused_1);
void name_entry_confirm_menu(void);

#endif
