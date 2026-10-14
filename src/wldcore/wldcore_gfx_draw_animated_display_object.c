#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Draw one animated display object: advance its sequence, then convert every
 * part of the current cel into a libgs sprite primitive and sort it into the
 * ordering table.
 *
 * Flag 0x400 copies the object's color verbatim, otherwise the global fade
 * tint is applied. Flag 0x800, or system flag 0x08000000 being clear, steps the
 * frame counter. Flag 0x20 overrides the part's own blend mode with the
 * object's (flags 0xc0).
 *
 * The parts are walked back to front as raw 32-bit element indices: the target spells the
 * subscript ((frame_count - i) * 2) | 1 into the cel's 32-bit array, which is the
 * part's first 32-bit value, and reaches the part itself from there. Indexing a part
 * array of records instead produces a different address computation, so
 * the 32-bit element index is retained.
 *
 * Both high halves are masked before the shift. The code is the same as a
 * plain shift, but it matches the target's 0x88-byte frame: CSE shares the
 * 0xffff0000 constant between the two statements, so the first mask/shift
 * combine keeps that constant for the second and leaves its dead AND result
 * as a USE-only pseudo. That pseudo and the part loop's folded entry test
 * (combine turns it into `blez count`, keeps `i = 0`, and leaves the slt
 * pseudo only a USE) get the unreferenced reload slots at 0x50 and 0x58. */
void wldcore_gfx_draw_animated_display_object(world_animated_render_record_t* object, GsOT* ot) {
    GsSPRITE sprite;
    CVECTOR color;
    world_sprite_animation_sequence_t* sequence;
    world_sprite_part_t* part;
    world_sprite_cel_header_t** cels;
    world_sprite_cel_header_t* cel;
    u32* words;
    s32 count;
    s32 i;
    s32 index;
    s32 mode;
    s32 tpage_x;
    s32 tpage_y;
    s32 clut_x;
    s32 clut_y;
    s32 palette;
    s32 px;

    sequence = g_wldcore_anim_sequence_table[object->sequence];
    sprite.my = 0;
    sprite.mx = 0;
    sprite.scaley = ONE;
    sprite.scalex = ONE;
    sprite.rotate = 0;
    cels = g_wldcore_anim_cel_table;
    if (object->flags & WORLD_RENDER_RECORD_UNTINTED) {
        color = object->color;
    } else {
        wldcore_gfx_copy_color_with_tint(&object->color, &color);
    }
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    if ((object->flags & WORLD_RENDER_RECORD_ANIMATE_WHILE_PAUSED)
        || !(g_main_system_flags & MAIN_SYSTEM_FLAG_FREEZE_ANIMATION)) {
        object->anim_counter = object->anim_counter + 1;
    }
    /* A goto loop: as a for/while loop, GCC's loop pass hoists the 0xffff
     * constant into a register, which the target does not. */
step_frame:
    index = (sequence->frames + object->frame_index)->duration;
    if (index == object->anim_counter && index != 0xffff) {
        object->anim_counter = 0;
        object->frame_index = object->frame_index + 1;
        index = sequence->frame_count;
        if (index == object->frame_index) {
            object->frame_index = 0;
        }
        goto step_frame;
    }
    index = (sequence->frames + object->frame_index)->cel;
    cel = cels[index];
    /* Preserve the reverse 32-bit cursor; a part array changes address computation. */
    words = (u32*)cel;
    tpage_x = cel->packed_texture_page_origin & 0xffff;
    tpage_y = (cel->packed_texture_page_origin & 0xffff0000) >> 16;
    clut_x = cel->packed_clut_origin & 0xffff;
    count = cel->part_count;
    clut_y = (cel->packed_clut_origin & 0xffff0000) >> 16;
    for (i = 0; i < count; i++) {
        index = ((count - i) * 2) | 1;
        sprite.attribute = words[index] & 0x80000000;
        if (object->flags & WORLD_RENDER_RECORD_SEMI_TRANSPARENT) {
            sprite.attribute |= 0x40000000;
            sprite.attribute |= (object->flags & WORLD_RENDER_RECORD_BLEND_RATE_MASK) << 22;
            mode = (object->flags & WORLD_RENDER_RECORD_BLEND_RATE_MASK) >> 6;
        } else {
            sprite.attribute |= words[index] & 0x70000000;
            mode = (words[index] >> 28) & 3;
        }
        /* Pointer indexing reverses this address-add instruction's operands. */
        part = (world_sprite_part_t*)(index * 4 + (s32)words);
        /* These packed reads must stay 32 bits wide; named byte fields shorten the code. */
        sprite.attribute |= (words[index] & 0x07000000) >> 2;
        px = part->x_offset - 0x80;
        sprite.x = object->position.x + px;
        px = part->y_offset - 0x80;
        sprite.y = object->position.y + px;
        sprite.w = part->width;
        sprite.h = part->height;
        sprite.tpage = GetTPage(0, mode, tpage_x, tpage_y);
        sprite.u = part->u;
        sprite.v = (words[index + 1] & 0xff0000) >> 16;
        palette = object->palette;
        if (palette == 0 || (words[index] & 0x08000000)) {
            sprite.cx = (clut_x + (words[index] & 0xff0000)) >> 12;
        } else {
            sprite.cx = clut_x + ((palette - 1) << 4);
        }
        sprite.cy = clut_y;
        if (sprite.attribute & 0x800000) {
            sprite.u = sprite.u + 1;
        }
        if (sprite.attribute & 0x400000) {
            sprite.v = sprite.v + 1;
        }
        GsSortFlipSprite(&sprite, ot, object->priority);
    }
}
