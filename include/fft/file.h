#ifndef FILE_H
#define FILE_H

/* File loading helpers shared by the overlays, OPEN and WLDCORE around the resident loader. */

#include "fft/main.h"
#include "psx/types.h"

/* loading */
void file_load_sync(s32 lba, s32 size, void* destination);
void file_wait_then_build_header(main_file_load_descriptor_t* header, s32 sector, s32 sector_count, void* destination);
void file_wait_for_pending(void);

#endif
