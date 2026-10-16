#ifndef WINDOW_H
#define WINDOW_H

/* Window render records: the display-list records the opening and the world
 * map build their windows and text from. Both programs link their own
 * revision of this source (56-byte records in OPEN, 52-byte in WLDCORE). */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* records */
s32 window_append_animated_record(void** list, s32* count);
s32 window_append_record(void** list, s32* count);
void window_dispatch_record_list(GsOT* otag, void** records, s32 count);
void window_init_text_record(s32 record_index, RECT quad, s32 depth, s32 combined_text_index, u32* image);
void window_display_text_entry(s32 record_index, s32 combined_text_index, point32_t position, u32* image);
void window_init_record_pools(void);

#endif
