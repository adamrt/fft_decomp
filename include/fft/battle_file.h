#ifndef FFT_BATTLE_FILE_H
#define FFT_BATTLE_FILE_H

#include "psx/types.h"

/* Disc extent of a BATTLE resource file. */
typedef struct battle_file_extent {
    s32 sector;
    s32 size;
} battle_file_extent_t;

#endif
