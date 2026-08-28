#include "fft/wldcore.h"
#include "psx/gs.h"
#include "psx/types.h"

void world_gs_sortfastsprite(GsSPRITE* sprite, GsOT* ot, u16 pri);
void wldcore_gfx_calculate_scaled_rectangle(
    s32 scale_mode, const wldcore_display_rect_t* source, wldcore_display_rect_t* rectangle);

/* Convert one display object into a libgs sprite primitive and sort it into the
 * ordering table handed down by wldcore_dispatch_display_object_list.
 *
 * Flag 0x004 selects the additive/semi-transparent attribute; flag 0x400 copies
 * the object's colour verbatim, otherwise 0x80069718 applies the global fade
 * tint to it; flag 0x100 selects the animated path, where 0x8006b678 looks up
 * the frame rectangle for anim_counter/2 and the counter is then stepped, the
 * flag clearing itself once the halved counter reaches 4.
 *
 * A field-to-field copy narrows the load to the store's mode at expand time;
 * routing the value through a named local of the field's own width does not,
 * and combine cannot re-narrow it afterwards. The target narrows exactly where
 * no local is involved (`sprite.u = fx + object->rect.u` is `lbu`), so the
 * locals and their absence both follow the target's load widths.
 *
 * - The anim test is `>= 4`: the target puts the counter-step arm at the
 *   branch target and the clear-and-toggle arm in the fall-through.
 * - `frame.u` and `frame.v` need their own locals, or the `sb` sites re-load
 *   them narrowed instead of reusing the `lhu` value.
 * - `object->palette` needs one local shared by the `!= 0` test and the
 *   arithmetic, or the arithmetic re-loads it.
 * - `ox = object->x` must precede `fx = frame.u`: the three loads form one
 *   scheduler ready list ordered by source, and the other order transposes
 *   two of them.
 */
void wldcore_gfx_draw_display_object_sprite(wldcore_display_object_t* object, s32 ot) {
    GsSPRITE sprite;
    wldcore_display_rect_t frame;
    CVECTOR color;
    s32 centre_x;
    s32 centre_y;
    s32 stepped_x;
    s32 page;
    s32 step;

    if (object->flags & 4) {
        sprite.attribute = 0x01000000;
    } else {
        sprite.attribute = 0;
    }
    centre_x = object->cx;
    sprite.cx = centre_x;
    centre_y = object->cy;
    sprite.cy = centre_y;
    step = object->palette;
    if (step != 0) {
        stepped_x = object->cx;
        sprite.cx = stepped_x + ((step - 1) << 4);
    }
    if (object->flags & 0x400) {
        color = object->color;
    } else {
        wldcore_gfx_copy_color_with_tint(&object->color, &color);
    }
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    page = object->tpage;
    sprite.tpage = page;
    if (object->flags & 0x100) {
        s32 fx;
        s32 fy;
        s32 ox;
        s32 oy;

        wldcore_gfx_calculate_scaled_rectangle(object->anim_counter / 2, &object->rect, &frame);
        ox = object->x;
        fx = frame.u;
        fy = frame.v;
        sprite.x = fx + ox;
        oy = object->y;
        sprite.y = fy + oy;
        sprite.u = fx + object->rect.u;
        sprite.v = fy + object->rect.v;
        sprite.w = frame.w;
        sprite.h = frame.h;
        if (object->anim_counter / 2 >= 4) {
            object->anim_counter = 0;
            object->flags ^= 0x100;
        } else {
            object->anim_counter = object->anim_counter + 1;
        }
    } else {
        s32 sx;
        s32 sy;
        s32 su;
        s32 sv;

        sx = object->x;
        sprite.x = sx;
        sy = object->y;
        sprite.y = sy;
        su = object->rect.u;
        sprite.u = su;
        sv = object->rect.v;
        sprite.v = sv;
        sprite.w = object->rect.w;
        sprite.h = object->rect.h;
    }
    world_gs_sortfastsprite(&sprite, (GsOT*)ot, object->priority);
}
