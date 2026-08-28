#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/open.h"
#include "fft/world.h"

/* Runtime flag bits: 0x80 world overlay loaded, 0x100 party chosen,
 * 0x4000 << party_mode selects the party (three bits, 0x1C000). */
#define OPEN_FLAG_WORLD_OVERLAY_LOADED 0x80
#define OPEN_FLAG_PARTY_CHOSEN         0x100
#define OPEN_PARTY_MODE_FLAG_BASE      0x4000
#define OPEN_PARTY_MODE_FLAG_MASK      0x1C000

/* Load WORLD if needed, then initialize the selected new-game party.
 *
 * The empty compiler barrier keeps the party-mask constant load after both
 * flag tests; GCC otherwise hoists it into the first branch delay slot. */
void open_title_init_new_game_party(s32 party_mode, s32 world_load_mode) {
    u32 initial_flags;
    u32 flags;
    u32 party_mode_flag;
    u32 value;

    initial_flags = g_open_system_runtime_flags;
    if ((initial_flags & OPEN_FLAG_WORLD_OVERLAY_LOADED) == 0) {
        g_open_system_runtime_flags = initial_flags | OPEN_FLAG_WORLD_OVERLAY_LOADED;
        main_overlay_open_world_bin(world_load_mode);
    }

    value = OPEN_PARTY_MODE_FLAG_BASE;
    flags = g_open_system_runtime_flags;
    party_mode_flag = value << party_mode;
    if ((flags & OPEN_FLAG_PARTY_CHOSEN) == 0 || (flags & party_mode_flag) == 0) {
        /* Pin required: unpinned, the flags word moves to $v1 and the mask
         * and party bit shift to $v0/$a0. */
        register u32 clear_mask __asm__("$3");

        /* Keeps the mask's lui out of the first branch's delay slot. */
        __asm__ volatile("" ::: "memory");
        clear_mask = ~OPEN_PARTY_MODE_FLAG_MASK;
        value = (flags | OPEN_FLAG_PARTY_CHOSEN) & clear_mask;
        g_open_system_runtime_flags = value | party_mode_flag;
        main_party_init_new_game(party_mode);
        world_text_generate_formation_unit_name_string();
    }
}
