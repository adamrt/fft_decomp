#ifndef MESSAGE_BOX_H
#define MESSAGE_BOX_H

/* WORLD's typewriter message-box thread and sound-assisted display. */

#include "psx/types.h"

void message_box_thread(void);
void message_box_show_with_sound(s32 message_id, s32 sound_id);

#endif
