#include "psx/types.h"

/* Store the top-left text origin used by the WORLD menu renderer. */
void world_menu_set_text_origin(s16 x, s16 y) {
    *(s16*)0x80153284 = x;
    *(s16*)0x80153286 = y;
}
