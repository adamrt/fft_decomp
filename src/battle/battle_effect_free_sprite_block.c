#include "fft/battle.h"

/* Release an effect-frame sprite block and update the live sprite count. */
void battle_effect_free_sprite_block(battle_effect_sprite_block_t* block) {
    g_battle_effect_sprite_count -= block->color.field.sprite_count;
    battle_heap_free_block(block);
}
