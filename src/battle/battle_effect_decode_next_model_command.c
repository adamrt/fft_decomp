#include "fft/battle.h"
#include "psx/types.h"

/* Provisional: one command of the effect resource's fourth section
 * (g_battle_effect_model_commands, see battle_init_effect_resource_sections). The top header
 * byte is the opcode and header byte 1 the word count after the header. */
typedef union battle_effect_command {
    u32 header;
    struct {
        u32 header;
        s32 values[3]; /* 0x04 */
        s32 x;         /* 0x10 */
        s32 y;         /* 0x14 */
    } op31;
    struct {
        u32 header;
        s32 values[4]; /* 0x04 */
        s32 x;         /* 0x14 */
        s32 y;         /* 0x18 */
    } op39;
    struct {
        u32 header;
        u16 halves[5]; /* 0x04 */
        u16 _pad0e;
        s32 values[3]; /* 0x10 */
        s32 x;         /* 0x1c */
        s32 y;         /* 0x20 */
    } op35;
} battle_effect_command_t;

/* Provisional: decoded effect command. */
typedef struct battle_effect_decoded {
    union {
        u32 word;
        u8 bytes[4];
    } header;      /* 0x00 */
    s32 values[4]; /* 0x04 */
    u16 halves[5]; /* 0x14 */
    u8 _unknown1e[0x4a];
    s32 x; /* 0x68 */
    s32 y; /* 0x6c */
} battle_effect_decoded_t;

/* Decode the next effect command and advance the command index.
 *
 * Opcodes 0x31 and 0x39 carry three or four values plus a position; 0x35 and
 * 0x37 add five halfwords. Any other opcode is skipped by its length byte.
 *
 * The 0x35/0x37 stream uses variable-length commands, so its index is a word
 * offset rather than a battle_effect_command_t array index. The final
 * volatile halfword store preserves the target's store ordering. Without it,
 * reorg moves that store past the command-index update into the jump delay
 * slot and removes the target's trailing nop. */
void battle_effect_decode_next_model_command(battle_effect_decoded_t* out) {
    battle_effect_command_t* command;
    s32 index;
    battle_effect_decoded_t* dst;
    u32 header;

    dst = out;
    index = g_battle_effect_model_command_index;
    command = (battle_effect_command_t*)&g_battle_effect_model_commands[index];
    header = command->header;
    out->header.word = header;
    switch (header >> 24) {
    case 0x39:
        out->values[0] = command->op39.values[0];
        out->values[1] = command->op39.values[1];
        out->values[2] = command->op39.values[2];
        out->values[3] = command->op39.values[3];
        out->x = command->op39.x;
        out->y = command->op39.y;
        g_battle_effect_model_command_index = index + 7;
        break;
    case 0x31:
        out->values[0] = command->op31.values[0];
        out->values[1] = command->op31.values[1];
        out->values[2] = command->op31.values[2];
        out->x = command->op31.x;
        out->y = command->op31.y;
        g_battle_effect_model_command_index = index + 6;
        break;
    case 0x35:
    case 0x37: {
        battle_effect_command_t* extended;

        extended = (battle_effect_command_t*)&g_battle_effect_model_commands[g_battle_effect_model_command_index];
        dst->values[0] = extended->op35.values[0];
        dst->values[1] = extended->op35.values[1];
        dst->values[2] = extended->op35.values[2];
        dst->x = extended->op35.x;
        dst->y = extended->op35.y;
        out->halves[0] = extended->op35.halves[3];
        out->halves[1] = extended->op35.halves[1];
        out->halves[2] = extended->op35.halves[0];
        out->halves[3] = extended->op35.halves[2];
        *(volatile u16*)&out->halves[4] = extended->op35.halves[4];
        g_battle_effect_model_command_index += 9;
        break;
    }
    default:
        g_battle_effect_model_command_index += out->header.bytes[1] + 1;
        break;
    }
}
