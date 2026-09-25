#include "psx/types.h"

/* The volatiles keep the target's order: read-modify-write of the entry
 * flags first, then the `li -1` store; without them GCC hoists the constant
 * and fills the load-delay nops. */
/* Same 0x34-byte record as fft/wldcore.h's wldcore_window_render_record_t,
 * named identically so the declarations fold; only this file's `volatile` on
 * the leading flags word differs, and the checker ignores qualifiers. */
typedef struct wldcore_window_render_record {
    volatile s32 flags;
    u8 _unused_04[0x30];
} wldcore_window_render_record_t;

/* volatile (see above): the target keeps the read-modify-write order. */
extern volatile u16 g_wldcore_state_flags;
extern s16 g_wldcore_sound_novel_picture_render_index;
extern volatile s16 g_wldcore_sound_novel_picture_id;
extern wldcore_window_render_record_t g_wldcore_window_render_records[];

void wldcore_opcode_hide_picture(void) {
    u16 flags;

    g_wldcore_window_render_records[g_wldcore_sound_novel_picture_render_index].flags |= 0x10;
    flags = g_wldcore_state_flags;
    g_wldcore_sound_novel_picture_id = -1;
    g_wldcore_state_flags = flags | 0xc;
}
