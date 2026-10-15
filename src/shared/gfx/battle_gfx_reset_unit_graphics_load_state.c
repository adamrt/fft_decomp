#include "fft/battle_gfx.h"

/* The two clearing loops walk their arrays by byte offset. A struct-subscript
 * walk leaves GCC with both the index and the scaled address as induction
 * variables, adding an `addiu`/`li` pair to each loop; the byte offset is what
 * reproduces the target's single `bgez` on the address offset. */
void battle_gfx_reset_unit_graphics_load_state(void) {
    s32 i;
    s32 offset;

    g_battle_unit_list_head = 0;
    for (offset = (BATTLE_UNIT_RUNTIME_SLOT_COUNT - 1) * sizeof(battle_unit_slot_state_t); offset >= 0;
        offset -= sizeof(battle_unit_slot_state_t)) {
        ((battle_unit_slot_state_t*)((u8*)g_battle_unit_slot_states + offset))->in_use = 0;
    }
    for (offset = 8 * sizeof(battle_gfx_spritesheet_slot_t); offset >= 0;
        offset -= sizeof(battle_gfx_spritesheet_slot_t)) {
        ((battle_gfx_spritesheet_slot_t*)((u8*)g_battle_gfx_spritesheet_slots + offset))->in_use = 0;
    }
    for (i = 0; i < 2; i++) {
        g_battle_gfx_vram_slots[i].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        g_battle_gfx_vram_slots[i].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_FREE;
        g_battle_gfx_vram_slots[i].evtchr_load_state = 0;
    }
    setRECT(&g_battle_gfx_vram_slots[0].image_rect, 0x100, 0, 0x40, 0xc8);
    setRECT(&g_battle_gfx_vram_slots[1].image_rect, 0x140, 0, 0x40, 0xc8);
    g_battle_gfx_seq_data_cursor = g_battle_gfx_unit_animation_data;
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
    g_battle_gfx_shp_frame_data_cursor = g_battle_gfx_shp_frame_data_buffer;
}
