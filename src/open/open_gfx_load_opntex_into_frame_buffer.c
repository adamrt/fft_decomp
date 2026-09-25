#include "psx/gpu.h"
#include "psx/types.h"

/* One TIM-shaped OPNTEX block header: a byte length, then the VRAM rectangle
 * as two packed words. The packed words are also read whole. */
typedef struct {
    u32 length; /* 0x00; bytes, including this header */
    union {
        u32 word;
        struct {
            u16 x;
            u16 y;
        } half;
    } position; /* 0x04 */
    union {
        u32 word;
        struct {
            u16 w;
            u16 h;
        } half;
    } size; /* 0x08 */
} open_opntex_block_t;

/* Upload an OPNTEX image, preceded by its CLUT when flag bit 3 is set.
 *
 * Word 1 holds the flags, and each block is an open_opntex_block_t followed by the
 * pixel data. The CLUT rectangle is flattened to one row of w * h entries.
 * Reading x and w through word locals keeps the target's full-word loads. The
 * product is pinned to $a3, where the allocator would not otherwise place
 * it. */
void open_gfx_load_opntex_into_frame_buffer(const void* source_data) {
    u32* tim;
    open_opntex_block_t* block;
    RECT rect;
    s32 offset;
    s32 word;
    /* Unused; pads the frame to the target's 0x38 bytes. */
    s32 stack_padding[4];

    tim = (u32*)source_data;
    if (tim[1] & 8) {
        s32 width;
        s32 height;
        register s32 area __asm__("$7");

        block = (open_opntex_block_t*)(tim + 2);
        offset = (block->length >> 2) + 2;
        word = block->position.word;
        rect.x = word;
        rect.y = block->position.half.y;
        width = block->size.word;
        rect.w = width;
        height = block->size.half.h;
        rect.h = height;
        area = width * height;
        rect.w = area;
        rect.h = 1;
        LoadImage(&rect, (u32*)(block + 1));
    } else {
        offset = 2;
    }
    /* Word-index form kept: a block pointer here adds tim and the scaled
     * offset in the opposite operand order from the target. */
    word = tim[offset + 1];
    rect.x = word;
    rect.y = ((u16*)(tim + offset))[3];
    word = tim[offset + 2];
    rect.w = word;
    rect.h = ((u16*)(tim + offset))[5];
    LoadImage(&rect, tim + (offset + 3));
    DrawSync(0);
}
