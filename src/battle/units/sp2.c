/* Load SP2 attack images and expand their nibble-compressed spritesheet data. */

#include "fft/ability.h"
#include "fft/file.h"
#include "fft/sprite_sheet.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/types.h"

extern file_extent_t g_sprite_sp2_files[];
extern file_extent_t g_sprite_worker8_sp2_files[];

static s32 s_sprite_compressed_nibble_phase;
static s32 s_sprite_compressed_offset;
/* Cursor state for the compressed attack-image tail of a unit SPR file. */
static const u8* s_sprite_compressed_data;

/* Start reading the SP2 file that holds the attack animation of the unit's SP2 ability into
 * destination. Worker 8 uses its own file table, indexed from animation 0x73. Returns -1 when the
 * animation is in the SP1 range or has no file, else whether the read was queued. */
s32 sprite_sp2_file_open(unit_runtime_t* unit, u32* destination) {
    s32 spritesheet_id;
    u8 animation_id;
    s32 index;
    s32 lba;
    s32 byte_length;

    spritesheet_id = g_sprite_sheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
    animation_id = g_ability_animation_data[unit->sp2_ability_id].attack_animation;
    if (spritesheet_id == SPRITE_SHEET_ID_STEEL_GIANT) {
        /* Nothing checks for an animation below 0x73, which would read before the table. */
        index = animation_id - 0x73;
        lba = g_sprite_worker8_sp2_files[index].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_sprite_worker8_sp2_files[index].size;
            return file_bytes_read_request(lba, byte_length, destination) != 0;
        }
    } else {
        lba = g_sprite_sp2_files[spritesheet_id].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_sprite_sp2_files[spritesheet_id].size;
            return file_bytes_read_request(lba, byte_length, destination) != 0;
        }
    }
    return -1;
}

/* Free the EVTCHR slots whose page holds the unit's SP2 image. */
void sprite_evtchr_invalidate_sp2(unit_runtime_t* unit) {
    u32 invalid; /* loaded before the loop, as in the target */
    sprite_evtchr_slot_t* slot;
    s32 count = 0;

    invalid = SPRITE_EVTCHR_SLOT_OWNER_NONE;
    slot = g_sprite_evtchr_slots;
    for (; count < SPRITE_EVTCHR_SLOT_COUNT; count++, slot++) {
        u32 descriptor = slot->owner;

        if (((descriptor & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK) == unit->runtime_id)
            && ((descriptor & SPRITE_EVTCHR_SLOT_OWNER_SP2) != 0)) {
            slot->owner = invalid;
        }
    }
}

/* The next nibble of the compressed spritesheet stream, the high nibble of each
 * byte first. */
static u32 compressed_nibble_read(void) {
    u32 nibble;
    u8 value;

    if (s_sprite_compressed_nibble_phase == 0) {
        nibble = s_sprite_compressed_data[s_sprite_compressed_offset] >> 4;
    } else {
        value = s_sprite_compressed_data[s_sprite_compressed_offset];
        s_sprite_compressed_offset++;
        nibble = value & 0xf;
    }
    s_sprite_compressed_nibble_phase ^= 1;
    return nibble;
}

/* Expands a nibble-RLE attack spritesheet into 0x6400 bytes of 4bpp pixels.
 *
 * A non-zero nibble is one pixel. A zero nibble starts a run of transparent
 * (index 0) pixels; the next nibble is the run length itself, except that 0
 * takes the length from one more nibble, and 7 and 8 from two or three more,
 * low nibble first. Output packs the first pixel into the high nibble.
 * Decoding stops when the output is full. */
void sprite_attack_image_decompress(const u8* source, u8* destination) {
    s32 high = 0;
    s32 count = 0;
    s32 value;
    s32 i;
    s32 size = 0x6400;
    u8* out = destination;

    s_sprite_compressed_data = source;
    s_sprite_compressed_offset = 0;
    s_sprite_compressed_nibble_phase = 0;
    while (1) {
        value = compressed_nibble_read();
        if (value == 0) {
            value = compressed_nibble_read();
            switch (value) {
            case 0:
                value = compressed_nibble_read();
                break;
            case 7:
                value = compressed_nibble_read();
                value |= compressed_nibble_read() << 4;
                break;
            case 8:
                value = compressed_nibble_read();
                value |= compressed_nibble_read() << 4;
                value |= compressed_nibble_read() << 8;
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
