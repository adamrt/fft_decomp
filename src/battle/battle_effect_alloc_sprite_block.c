#include "fft/battle.h"

/* Allocate the variable-length sprite block for one effect frame.
 *
 * The four-byte header is followed by one word per sprite. The renderer tracks
 * both current and peak occupancy for the shared effect pool. */
battle_effect_sprite_block_t* battle_effect_alloc_sprite_block(u8 sprite_count) {
    battle_effect_sprite_block_t* block;
    s32 total_count;

    block = battle_heap_alloc_block(((u32)sprite_count * 4) + 8, 0);
    block->color.field.sprite_count = sprite_count;
    block->color.field.blue = 0x80;
    block->color.field.green = 0x80;
    block->color.field.red = 0x80;
    total_count = g_battle_effect_sprite_count + sprite_count;
    g_battle_effect_sprite_count = total_count;
    if (g_battle_effect_sprite_count_peak < total_count) {
        g_battle_effect_sprite_count_peak = total_count;
    }
    return block;
}
