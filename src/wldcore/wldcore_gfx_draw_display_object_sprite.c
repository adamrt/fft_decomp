#include "fft/gfx.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Convert one display object into a libgs sprite primitive and sort it into the
 * ordering table handed down by wldcore_dispatch_display_object_list.
 *
 * Flag 0x004 selects the 8-bit color mode (attribute 0x01000000); flag 0x400 copies
 * the object's color verbatim, otherwise 0x80069718 applies the global fade
 * tint to it; flag 0x100 selects the animated path, where 0x8006b678 looks up
 * the frame rectangle for anim_counter/2 and the counter is then stepped, the
 * flag clearing itself once the halved counter reaches 4.
 *
 * A field-to-field copy narrows the load to the store's mode at expand time;
 * routing the value through a named local of the field's own width does not,
 * and combine cannot re-narrow it afterwards. The target narrows exactly where
 * no local is involved (`sprite.u = object->bounds.x + fx` is `lbu`), so the
 * locals and their absence both follow the target's load widths.
 *
 * - The anim test is `>= 4`: the target puts the counter-step arm at the
 *   branch target and the clear-and-toggle arm in the fall-through.
 * - `frame.x` and `frame.y` need their own locals, or the `sb` sites re-load
 *   them narrowed instead of reusing the `lhu` value.
 * - `object->palette` needs one local shared by the `!= 0` test and the
 *   arithmetic, or the arithmetic re-loads it.
 * - `ox = object->x` must precede `fx = frame.x`: the three loads form one
 *   scheduler ready list ordered by source, and the other order transposes
 *   two of them.
 */
void wldcore_gfx_draw_display_object_sprite(wldcore_window_render_record_t* object, GsOT* ot) {
    GsSPRITE sprite;
    RECT frame;
    CVECTOR color;
    s32 center_x;
    s32 center_y;
    s32 page;
    s32 step;

    if (object->flags & WORLD_RENDER_RECORD_8BIT_CLUT) {
        sprite.attribute = 0x01000000;
    } else {
        sprite.attribute = 0;
    }
    center_x = object->clut.x;
    sprite.cx = center_x;
    center_y = object->clut.y;
    sprite.cy = center_y;
    step = object->palette;
    if (step != 0) {
        sprite.cx = object->clut.x + ((step - 1) << 4);
    }
    if (object->flags & WORLD_RENDER_RECORD_UNTINTED) {
        color = object->color;
    } else {
        wldcore_gfx_copy_color_with_tint(&object->color, &color);
    }
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    page = object->tpage;
    sprite.tpage = page;
    if (object->flags & WORLD_RENDER_RECORD_POP_IN) {
        s16 fx;
        s16 fy;
        s32 ox;
        s32 oy;

        gfx_calculate_scaled_rectangle(object->anim_counter / 2, &object->bounds, &frame);
        ox = object->base.x;
        fx = frame.x;
        fy = frame.y;
        sprite.x = fx + ox;
        oy = object->base.y;
        sprite.y = fy + oy;
        sprite.u = object->bounds.x + fx;
        sprite.v = object->bounds.y + fy;
        sprite.w = frame.w;
        sprite.h = frame.h;
        if (object->anim_counter / 2 >= 4) {
            object->anim_counter = 0;
            object->flags ^= WORLD_RENDER_RECORD_POP_IN;
        } else {
            object->anim_counter = object->anim_counter + 1;
        }
    } else {
        s32 sx;
        s32 sy;
        s16 su;
        s16 sv;

        sx = object->base.x;
        sprite.x = sx;
        sy = object->base.y;
        sprite.y = sy;
        su = object->bounds.x;
        sprite.u = su;
        sv = object->bounds.y;
        sprite.v = sv;
        sprite.w = object->bounds.w;
        sprite.h = object->bounds.h;
    }
    GsSortFastSprite(&sprite, ot, object->priority);
}
