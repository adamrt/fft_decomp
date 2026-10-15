#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Start loading the SP2 file that holds the frames of the unit's pending
 * attack animation.
 *
 * Returns -1 when the sheet has no SP2 file or the ability's attack
 * animation is below the SEQ table's first SP2 sequence. Otherwise it asks
 * for the read and returns 0 when it started, 1 when the loader was busy.
 * Sheet 0x9a has a file for each animation from 0x73 up
 * (g_battle_gfx_worker8_sp2_files); every other sheet has one file. */
s32 battle_gfx_open_sp2_file(battle_unit_t* unit, u32* destination) {
    s32 spritesheet_id;
    u8 animation_id;
    s32 index;
    s32 lba;
    s32 byte_length;

    spritesheet_id = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
    animation_id = g_battle_ability_animation_data[unit->sp2_ability_id].attack_animation;
    if (spritesheet_id == BATTLE_SPRITESHEET_ID_STEEL_GIANT) {
        index = animation_id - 0x73;
        lba = g_battle_gfx_worker8_sp2_files[index].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= (u32)unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_battle_gfx_worker8_sp2_files[index].size;
            return main_file_call_build_header(lba, byte_length, destination) != 0;
        }
    } else {
        lba = g_battle_gfx_sp2_files[spritesheet_id].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= (u32)unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_battle_gfx_sp2_files[spritesheet_id].size;
            return main_file_call_build_header(lba, byte_length, destination) != 0;
        }
    }
    return -1;
}

/* Free the EVTCHR slots whose page holds the unit's SP2 image. */
void battle_gfx_invalidate_sp2_vram_slot(battle_unit_t* unit) {
    u32 invalid;
    battle_gfx_vram_slot_t* slot;
    s32 count;

    count = 0;
    invalid = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
    slot = g_battle_gfx_vram_slots;
    for (; count < 2; count++, slot++) {
        u32 descriptor = slot->owner;

        if (((descriptor & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id)
            && ((descriptor & BATTLE_GFX_VRAM_SLOT_OWNER_SP2) != 0)) {
            slot->owner = invalid;
        }
    }
}

/* The next nibble of the compressed spritesheet stream, the high nibble of each
 * byte first. */
u32 battle_gfx_read_compressed_nibble(void) {
    u32 nibble;
    u8 value;

    if (g_battle_gfx_compressed_nibble_phase == 0) {
        nibble = g_battle_gfx_compressed_data[g_battle_gfx_compressed_offset] >> 4;
    } else {
        value = g_battle_gfx_compressed_data[g_battle_gfx_compressed_offset];
        g_battle_gfx_compressed_offset++;
        nibble = value & 0xf;
    }
    g_battle_gfx_compressed_nibble_phase ^= 1;
    return nibble;
}

/* Expands a nibble-RLE attack spritesheet into 0x6400 bytes of 4bpp pixels.
 *
 * A zero nibble introduces a run of transparent pixels whose length follows as
 * one nibble, or as two or three nibbles after the markers 7 and 8. */
void battle_gfx_decompress_attack_spritesheet(const u8* source, u8* destination) {
    s32 high;
    s32 count;
    s32 value;
    s32 i;
    s32 size;
    u8* out;

    high = 0;
    count = 0;
    size = 0x6400;
    out = destination;
    g_battle_gfx_compressed_data = source;
    g_battle_gfx_compressed_offset = 0;
    g_battle_gfx_compressed_nibble_phase = 0;
    while (1) {
        value = battle_gfx_read_compressed_nibble();
        if (value == 0) {
            value = battle_gfx_read_compressed_nibble();
            switch (value) {
            case 0:
                value = battle_gfx_read_compressed_nibble();
                break;
            case 7:
                value = battle_gfx_read_compressed_nibble();
                value |= battle_gfx_read_compressed_nibble() << 4;
                break;
            case 8:
                value = battle_gfx_read_compressed_nibble();
                value |= battle_gfx_read_compressed_nibble() << 4;
                value |= battle_gfx_read_compressed_nibble() << 8;
                break;
            }
            for (i = 0; i < value; i++) {
                if (high == 0) {
                    destination[count] = 0;
                } else {
                    out++;
                    destination[count] &= 0xf0;
                    count++;
                    if (count == size) {
                        return;
                    }
                }
                high ^= 1;
            }
        } else {
            if (high == 0) {
                *out = value << 4;
            } else {
                count++;
                *out |= value;
                out++;
                if (count == size) {
                    return;
                }
            }
            high ^= 1;
        }
    }
}
