#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/gs.h"
#include "psx/gte.h"
#include "psx/types.h"

/* One frame of an animation sequence: the cel it draws and how long it holds.
 * duration 0xffff pins the frame. */
typedef struct wldcore_anim_frame {
    u16 cel;      /* 0x00 */
    u16 duration; /* 0x02 */
} wldcore_anim_frame_t;

typedef struct wldcore_anim_sequence {
    s32 frame_count;                /* 0x00 */
    wldcore_anim_frame_t frames[1]; /* 0x04 */
} wldcore_anim_sequence_t;

/* One sprite part of a cel, two packed words. The byte view supplies the
 * position and the source rectangle; the word view supplies the bitfields. */
typedef union wldcore_sprite_part {
    struct {
        u8 x; /* 0x00; biased by 0x80 */
        u8 y; /* 0x01; biased by 0x80 */
        u8 unknown_02[2];
        u8 h; /* 0x04 */
        u8 w; /* 0x05 */
        u8 unknown_06;
        u8 u; /* 0x07 */
    } field;
    struct {
        u32 flags;   /* 0x00 */
        u32 texture; /* 0x04 */
    } packed;
} wldcore_sprite_part_t;

extern wldcore_anim_sequence_t** g_wldcore_anim_sequence_table;

u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
void world_gs_sortflipsprite(GsSPRITE* sprite, void* ot, s32 pri);

/* Draw one animated display object: advance its sequence, then convert every
 * part of the current cel into a libgs sprite primitive and sort it into the
 * ordering table.
 *
 * Flag 0x400 copies the object's colour verbatim, otherwise the global fade
 * tint is applied. Flag 0x800, or system flag 0x08000000 being clear, steps the
 * frame counter. Flag 0x20 overrides the part's own blend mode with the
 * object's (flags 0xc0).
 *
 * The parts are walked back to front as raw word indices: the target spells the
 * subscript ((frame_count - i) * 2) | 1 into the cel's word array, which is the
 * part's second word, and reaches the part itself from there. Indexing a part
 * array of the union type instead produces a different address computation, so
 * the word view is kept and the union carries the field names.
 *
 * Both high halves are masked before the shift. The code is the same as a
 * plain shift, but it matches the target's 0x88-byte frame: CSE shares the
 * 0xffff0000 constant between the two statements, so the first mask/shift
 * combine keeps that constant for the second and leaves its dead AND result
 * as a USE-only pseudo. That pseudo and the part loop's folded entry test
 * (combine turns it into `blez count`, keeps `i = 0`, and leaves the slt
 * pseudo only a USE) get the unreferenced reload slots at 0x50 and 0x58. */
void wldcore_gfx_draw_animated_display_object(wldcore_anim_object_t* object, GsOT* ot) {
    GsSPRITE sprite;
    CVECTOR color;
    wldcore_anim_sequence_t* sequence;
    wldcore_sprite_part_t* part;
    u32** cels;
    u32* words;
    s32 count;
    s32 i;
    s32 index;
    s32 mode;
    s32 clut_x;
    s32 clut_y;
    s32 cx_base;
    s32 cy;
    s32 palette;
    s32 ox;
    s32 oy;
    s32 px;
    s32 py;

    sequence = g_wldcore_anim_sequence_table[object->sequence];
    sprite.my = 0;
    sprite.mx = 0;
    sprite.scaley = ONE;
    sprite.scalex = ONE;
    sprite.rotate = 0;
    cels = g_wldcore_anim_cel_table;
    if (object->flags & 0x400) {
        color = object->color;
    } else {
        wldcore_gfx_copy_color_with_tint(&object->color, &color);
    }
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    if ((object->flags & 0x800) || !(g_main_system_flags & 0x08000000)) {
        object->anim_counter = object->anim_counter + 1;
    }
    /* A goto loop: as a for/while loop, GCC's loop pass hoists the 0xffff
     * constant into a register, which the target does not. */
step_frame:
    index = ((wldcore_anim_frame_t*)sequence)[object->frame_index + 1].duration;
    if (index == object->anim_counter && index != 0xffff) {
        object->anim_counter = 0;
        object->frame_index = object->frame_index + 1;
        index = sequence->frame_count;
        if (index == object->frame_index) {
            object->frame_index = 0;
        }
        goto step_frame;
    }
    index = ((wldcore_anim_frame_t*)sequence)[object->frame_index + 1].cel;
    words = cels[index];
    clut_x = words[1] & 0xffff;
    clut_y = (words[1] & 0xffff0000) >> 16;
    cx_base = words[2] & 0xffff;
    count = words[0];
    cy = (words[2] & 0xffff0000) >> 16;
    for (i = 0; i < count; i++) {
        index = ((count - i) * 2) | 1;
        sprite.attribute = words[index] & 0x80000000;
        if (object->flags & 0x20) {
            sprite.attribute |= 0x40000000;
            sprite.attribute |= (object->flags & 0xc0) << 22;
            mode = (object->flags & 0xc0) >> 6;
        } else {
            sprite.attribute |= words[index] & 0x70000000;
            mode = ((u32)words[index] >> 28) & 3;
        }
        part = (wldcore_sprite_part_t*)(index * 4 + (s32)words);
        sprite.attribute |= (part->packed.flags & 0x07000000) >> 2;
        px = part->field.x - 0x80;
        sprite.x = object->x + px;
        px = part->field.y - 0x80;
        sprite.y = object->y + px;
        sprite.w = part->field.w;
        sprite.h = part->field.h;
        sprite.tpage = GetTPage(0, mode, clut_x, clut_y);
        sprite.u = part->field.u;
        sprite.v = (part->packed.texture & 0xff0000) >> 16;
        palette = object->palette;
        if (palette == 0 || (part->packed.flags & 0x08000000)) {
            sprite.cx = (cx_base + (part->packed.flags & 0xff0000)) >> 12;
        } else {
            sprite.cx = cx_base + ((palette - 1) << 4);
        }
        sprite.cy = cy;
        if (sprite.attribute & 0x800000) {
            sprite.u = sprite.u + 1;
        }
        if (sprite.attribute & 0x400000) {
            sprite.v = sprite.v + 1;
        }
        world_gs_sortflipsprite(&sprite, ot, object->priority);
    }
}
