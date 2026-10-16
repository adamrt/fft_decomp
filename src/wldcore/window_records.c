/* Window record pools, dispatch, drawing and appending, shared by the opening and the world map with their own record
 * layouts (56 and 52 bytes). */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/open.h"
#include "fft/wldcore.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

static void gfx_calculate_scaled_rectangle(s32 scale_mode, const RECT* source, RECT* rectangle);
static void open_gfx_draw_animated_render_record(world_animated_render_record_t* actor, u32* ot);
static void open_gfx_draw_window_record(open_render_record_56_t* record, u32* ot);
static void wldcore_gfx_draw_display_object_sprite(wldcore_window_render_record_t* object, GsOT* ot);

#ifdef FFT_WLDCORE
/* Reset both world-map window pools and their allocation counters. */
void window_init_record_pools(void) {
    /* Member pointers preserve the separate color and CLUT address walks. */
    CVECTOR* color;
    point32_t* clut;
    s32 i;

    g_wldcore_window_render_record_count = 0;
    g_wldcore_window_record_count = 0;
    g_wldcore_window_render_object_count = 0;
    g_wldcore_window_aux_render_object_count = 0;
    for (i = 0; i < WLDCORE_WINDOW_RECORD_CAPACITY; i++) {
        g_wldcore_window_records[i].flags = WORLD_RENDER_RECORD_ANIMATED;
        g_wldcore_window_records[i].priority = 1;
        color = &g_wldcore_window_records[i].color;
        color->b = color->g = color->r = 0x80;
    }

    for (i = 0; i < WLDCORE_WINDOW_RECORD_CAPACITY; i++) {
        g_wldcore_window_render_records[i].flags = WORLD_RENDER_RECORD_IMAGE;
        g_wldcore_window_render_records[i].priority = 1;
        clut = &g_wldcore_window_render_records[i].clut;
        clut->x = 0x3c0;
        clut->y = 0x1f0;
        color = &g_wldcore_window_render_records[i].color;
        color->b = color->g = color->r = 0x80;
    }
}
#endif

#ifdef FFT_OPENING
/* Empty both render record pools and the pointer list. Every record gets its
 * layout flag, ordering-table entry 8 and the neutral color 0x80. */
void window_init_record_pools(void) {
    s32 i;
    CVECTOR* color;

    g_open_gfx_primitive_count = 0;
    g_open_gfx_next_render_record_56 = 0;
    g_open_gfx_next_render_record_36 = 0;
    g_open_gfx_render_record_pointer_count = 0;

    for (i = 0; i < OPEN_GFX_RENDER_RECORD_CAPACITY; i++) {
        g_open_gfx_render_records_36[i].flags = WORLD_RENDER_RECORD_ANIMATED;
        g_open_gfx_render_records_36[i].priority = 8;
        color = &g_open_gfx_render_records_36[i].color;
        color->b = color->g = color->r = 0x80;
    }

    for (i = 0; i < OPEN_GFX_RENDER_RECORD_CAPACITY; i++) {
        g_open_gfx_render_records_56[i].flags = WORLD_RENDER_RECORD_IMAGE;
        g_open_gfx_render_records_56[i].ot_layer = 8;
        color = &g_open_gfx_render_records_56[i].color;
        color->b = color->g = color->r = 0x80;
    }
}
#endif

#ifdef FFT_OPENING
/* Clear the active buffer's 16-entry ordering table and release the frame's
 * sprite primitives. */
void open_gfx_clear_current_otag(void) {
    u32 stack_padding[2];

    ClearOTagR(g_open_gfx_otags[g_main_gfx_active_buffer_index], 0x10);
    g_open_gfx_primitive_count = 0;
}
#endif

#ifdef FFT_WLDCORE
/* Dispatch a list of display objects to one of two handlers by object kind.
 * Objects with flags 0x18 are skipped, and the whole pass is suppressed while
 * system flag 0x400 is set. */
void window_dispatch_record_list(GsOT* otag, void** list, s32 count) {
    void* object;
    s32* flags;
    s32 i;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS) {
        return;
    }
    for (i = 0; i < count; i++) {
        object = list[i];
        flags = object;
        if (*flags & (WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN)) {
            continue;
        }
        if (*flags & WORLD_RENDER_RECORD_ANIMATED) {
            world_animated_render_record_t* animated = object;
            wldcore_gfx_draw_animated_display_object(animated, otag);
        } else if (*flags & WORLD_RENDER_RECORD_IMAGE) {
            wldcore_window_render_record_t* image = object;
            wldcore_gfx_draw_display_object_sprite(image, otag);
        }
    }
}
#endif

#ifdef FFT_OPENING
/* Draw the listed render records into the ordering table. A record with BLANK or
 * HIDDEN set is skipped; ANIMATED selects the 36-byte layout and IMAGE the
 * 56-byte one. Nothing is drawn while
 * OPEN_SYSTEM_RUNTIME_FLAG_RENDER_RECORDS_HIDDEN is set, which no code in
 * OPEN.BIN does. */
void window_dispatch_record_list(GsOT* otag, void** list, s32 count) {
    s32 i;
    void* object;
    u32* flags_word;
    u32 flags;

    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RENDER_RECORDS_HIDDEN) != 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        object = list[i];
        flags_word = object;
        flags = *flags_word;
        /* Both layouts start with the 32-bit flags. */
        if ((flags & (WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN)) == 0) {
            if ((flags & WORLD_RENDER_RECORD_ANIMATED) != 0) {
                world_animated_render_record_t* animated = object;
                open_gfx_draw_animated_render_record(animated, otag);
            } else if ((flags & WORLD_RENDER_RECORD_IMAGE) != 0) {
                open_render_record_56_t* window = object;
                open_gfx_draw_window_record(window, otag);
            }
        }
    }
}
#endif

#ifdef FFT_WLDCORE
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
#endif

#ifdef FFT_WLDCORE
/* Convert one display object into a libgs sprite primitive and sort it into the
 * ordering table handed down by window_dispatch_record_list.
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
static void wldcore_gfx_draw_display_object_sprite(wldcore_window_render_record_t* object, GsOT* ot) {
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
#endif

#ifdef FFT_OPENING
#    define PART(index) ((world_sprite_part_t*)&sprite[index])

/* Advances one OPEN sprite actor's animation and emits its parts into an
 * ordering table.
 *
 * The actor names an animation in the table at g_open_gfx_record_36_animations; each animation is
 * a frame count followed by {sprite_id, duration} pairs, and a duration of
 * 0xffff holds the frame indefinitely. The named sprite in g_open_gfx_record_36_sprites is a
 * part list whose odd 32-bit entries carry the texture page, semi-transparency and
 * palette bits and whose byte fields give the part offset, size and texture
 * coordinates. Each part consumes one DR_MODE + SPRT pair from the shared
 * 32-entry per-buffer primitive pool at g_open_gfx_sprite_primitive_pool.
 *
 * Both high halves are masked before the shift. The code is the same as a
 * plain shift, but it matches the target's frame: CSE shares the 0xffff0000
 * constant between the two statements, so the first mask/shift combine keeps
 * that constant for the second and leaves its dead AND result as a USE-only
 * pseudo. That pseudo and the part loop's folded entry test each get an
 * unreferenced 8-byte reload slot (0x38 and 0x40). Reading x and y into their
 * own locals keeps the target's x + byte operand order and v0/v1 allocation.
 */
static void open_gfx_draw_animated_render_record(world_animated_render_record_t* actor, u32* ot) {
    world_sprite_cel_header_t** sprites = g_open_gfx_record_36_sprites;
    world_sprite_animation_sequence_t* anim = g_open_gfx_record_36_animations[actor->sequence];
    world_sprite_cel_header_t* cel;
    u32* sprite;
    s32 value;
    s32 i;
    s32 part_count;
    u32 tpage_x;
    u32 tpage_y;
    u32 clut_x;
    u32 clut_y;
    s32 x;
    s32 y;

    actor->anim_counter++;
    /* A goto loop: as a for/while loop, GCC's loop pass hoists the 0xffff
     * constant into a register, which the target does not. */
loop:
    value = (anim->frames + actor->frame_index)->duration;
    if (value == actor->anim_counter && value != 0xffff) {
        actor->anim_counter = 0;
        actor->frame_index++;
        value = anim->frame_count;
        if (value == actor->frame_index) {
            actor->frame_index = 0;
        }
        goto loop;
    }

    value = (anim->frames + actor->frame_index)->cel;
    cel = sprites[value];
    /* Preserve the reverse 32-bit cursor; a part array changes address computation. */
    sprite = (u32*)cel;
    tpage_x = cel->packed_texture_page_origin & 0xffff;
    tpage_y = (cel->packed_texture_page_origin & 0xffff0000) >> 16;
    part_count = cel->part_count;
    clut_x = cel->packed_clut_origin & 0xffff;
    clut_y = (cel->packed_clut_origin & 0xffff0000) >> 16;

    for (i = 0; i < part_count; i++) {
        if (g_open_gfx_primitive_count >= 0x20) {
            return;
        }
        setRGB0((&g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt),
            actor->color.r, actor->color.g, actor->color.b);
        SetSprt(&g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt);
        value = ((part_count - i) * 2) | 1;
        SetDrawMode(&g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].mode,
            1, 0, GetTPage(PART(value)->texture_flags >> 2, PART(value)->blend_rate, tpage_x, tpage_y), 0);
        if (PART(value)->semi_transparent) {
            SetSemiTrans(
                &g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt, 1);
        } else {
            SetSemiTrans(
                &g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt, 0);
        }
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.x0
            = (x = actor->position.x, x + PART(value)->x_offset - 0x80);
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.y0
            = (y = actor->position.y, y + PART(value)->y_offset - 0x80);
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.u0
            = PART(value)->u;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.v0
            = PART(value)->v;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.w
            = PART(value)->width;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.h
            = PART(value)->height;
        /* A byte-field palette read shortens the native 32-bit flags load. */
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.clut
            = GetClut(clut_x
                    + ((actor->palette == 0 || (PART(value)->use_part_palette)) ? ((sprite[value] >> 12) & 0xf0)
                                                                                : ((actor->palette - 1) * 0x10)),
                clut_y);
        AddPrim(&ot[actor->priority],
            &g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt);
        AddPrim(&ot[actor->priority],
            &g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].mode);
        g_open_gfx_primitive_count++;
    }
}
#endif

#ifdef FFT_OPENING
/* Emits one 56-byte render record as a DR_MODE + SPRT pair from the shared
 * 32-entry per-buffer primitive pool at g_open_gfx_sprite_primitive_pool.
 *
 * Flags bit 2 and bits 6-7 select the texture page, bit 5 enables
 * semi-transparency, and bit 8 runs the pop-in scale: pop_in_step counts the
 * animation, gfx_calculate_scaled_rectangle turns half of it into a
 * shrunken source rectangle, and the flag clears once the record is at full
 * size. A nonzero palette picks the CLUT (palette - 1) * 16 pixels right of tail.clut.
 *
 * The retail code loads record fields at their declared width before
 * truncating them into the primitive (as if built with -fforce-mem); the
 * otherwise unused px/py/qx/qy/cx/qu/qv copies reproduce that. The SPRT casts
 * on the color stores keep their index order and the 0x50-byte frame.
 */
static void open_gfx_draw_window_record(open_render_record_56_t* record, u32* ot) {
    point32_t clut;
    RECT scaled;
    s32 step;
    s32 px;
    s32 py;
    s32 qx;
    s32 qy;
    s32 cx;
    u16 qu;
    u16 qv;

    if (g_open_gfx_primitive_count >= 0x20) {
        return;
    }
    SetSprt(&g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt);

    if (record->palette != 0) {
        clut.x = (cx = record->tail.clut.x) + (record->palette - 1) * 0x10;
        clut.y = record->tail.clut.y;
    } else {
        clut = record->tail.clut;
    }
    g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.clut
        = GetClut(clut.x, clut.y);
    setRGB0((&g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt),
        record->color.r, record->color.g, record->color.b);

    SetDrawMode(&g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].mode, 1,
        0,
        GetTPage(((record->flags & WORLD_RENDER_RECORD_8BIT_CLUT) >> 2) != 0,
            (record->flags & WORLD_RENDER_RECORD_BLEND_RATE_MASK) >> 6, record->vram_x, record->vram_y),
        0);

    if (record->flags & WORLD_RENDER_RECORD_SEMI_TRANSPARENT) {
        SetSemiTrans(
            &g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt, 1);
    } else {
        SetSemiTrans(
            &g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt, 0);
    }

    if (record->flags & WORLD_RENDER_RECORD_POP_IN) {
        gfx_calculate_scaled_rectangle(record->pop_in_step / 2, &record->tail.quad, &scaled);
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.x0
            = scaled.x + (px = record->x);
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.y0
            = scaled.y + (py = record->y);
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.u0
            = record->tail.quad.x + scaled.x;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.v0
            = record->tail.quad.y + scaled.y;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.w = scaled.w;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.h = scaled.h;
        step = record->pop_in_step;
        if (step / 2 >= 4) {
            record->pop_in_step = 0;
            record->flags ^= WORLD_RENDER_RECORD_POP_IN;
        } else {
            record->pop_in_step = step + 1;
        }
    } else {
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.x0 = qx
            = record->x;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.y0 = qy
            = record->y;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.u0 = qu
            = record->tail.quad.x;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.v0 = qv
            = record->tail.quad.y;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.w
            = record->tail.quad.w;
        g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt.h
            = record->tail.quad.h;
    }

    AddPrim(&ot[record->ot_layer],
        &g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].sprt);
    AddPrim(&ot[record->ot_layer],
        &g_open_gfx_sprite_primitive_pool[g_main_gfx_active_buffer_index][g_open_gfx_primitive_count].mode);
    g_open_gfx_primitive_count++;
}
#endif

#ifdef FFT_WLDCORE
/* Attach the current window record to the caller's list and reset its color. */
s32 window_append_animated_record(void** queue, s32* count) {
    CVECTOR* color;

    queue[*count] = &g_wldcore_window_records[g_wldcore_window_record_count];
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &g_wldcore_window_records->color;
    g_wldcore_window_records[g_wldcore_window_record_count].palette = 0;
    g_wldcore_window_records[g_wldcore_window_record_count].flags
        &= ~(WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED);
    ((CVECTOR*)((u8*)color + g_wldcore_window_record_count * sizeof(g_wldcore_window_records[0])))->r = 0x80;
    ((CVECTOR*)((u8*)color + g_wldcore_window_record_count * sizeof(g_wldcore_window_records[0])))->g = 0x80;
    ((CVECTOR*)((u8*)color + g_wldcore_window_record_count * sizeof(g_wldcore_window_records[0])))->b = 0x80;
    *count += 1;
    return g_wldcore_window_record_count++;
}
#endif

#ifdef FFT_OPENING
/* Appends the next free 36-byte render record to `list`, resets it and
 * returns its pool index. */
s32 window_append_animated_record(void** list, s32* count) {
    list[*count] = &g_open_gfx_render_records_36[g_open_gfx_next_render_record_36];
    g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].flags
        &= ~(WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_SEMI_TRANSPARENT);
    g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].palette = 0;
    /* Direct color members change the shared base and add four instructions. */
    (&g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].color)->r = 0x80;
    (&g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].color)->g = 0x80;
    (&g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].color)->b = 0x80;
    (*count)++;
    return g_open_gfx_next_render_record_36++;
}
#endif

#ifdef FFT_WLDCORE
/* Attach the current render record to the caller's list and reset it.
 *
 * Sibling of window_append_animated_record, which performs the same append for the 36-byte
 * window records. */
s32 window_append_record(void** queue, s32* count) {
    CVECTOR* color;

    queue[*count] = &g_wldcore_window_render_records[g_wldcore_window_render_record_count];
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &g_wldcore_window_render_records->color;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].flags
        &= ~(WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED);
    ((CVECTOR*)((u8*)color + g_wldcore_window_render_record_count * sizeof(g_wldcore_window_render_records[0])))->r
        = 0x80;
    ((CVECTOR*)((u8*)color + g_wldcore_window_render_record_count * sizeof(g_wldcore_window_render_records[0])))->g
        = 0x80;
    ((CVECTOR*)((u8*)color + g_wldcore_window_render_record_count * sizeof(g_wldcore_window_render_records[0])))->b
        = 0x80;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].palette = 0;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].anim_counter = 0;
    *count += 1;
    return g_wldcore_window_render_record_count++;
}
#endif

#ifdef FFT_OPENING
/* Appends the next free 56-byte render record to `list`, resets it and
 * returns its pool index. */
s32 window_append_record(void** list, s32* count) {
    list[*count] = &g_open_gfx_render_records_56[g_open_gfx_next_render_record_56];
    g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].flags &= ~(WORLD_RENDER_RECORD_8BIT_CLUT
        | WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_SEMI_TRANSPARENT);
    g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].palette = 0;
    /* Direct color members change the shared base and add four instructions. */
    (&g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].color)->r = 0x80;
    (&g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].color)->g = 0x80;
    (&g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].color)->b = 0x80;
    g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].pop_in_step = 0;
    (*count)++;
    return g_open_gfx_next_render_record_56++;
}
#endif

/* Build the centered sub-rectangle of the source area for a scale mode
 * (20%, 50%, 80%, 90% or 100%), aligned to 4 pixels. */
static void gfx_calculate_scaled_rectangle(s32 scale_mode, const RECT* source, RECT* rectangle) {
    s32 scale;
    s32 source_width;
    s32 source_height;
    s32 half_width;
    s32 half_height;
    s32 width;
    s32 height;

    switch (scale_mode) {
    case 0:
        scale = 20;
        break;
    case 1:
        scale = 50;
        break;
    case 2:
        scale = 80;
        break;
    case 3:
        scale = 90;
        break;
    default:
        scale = 100;
        break;
    }

    source_width = source->w;
    source_height = source->h;
    half_width = (source_width / 2) & 0xfffc;
    half_height = (source_height / 2) & 0xfffc;
    width = (source_width * scale / 200) & 0xfffc;
    height = (source_height * scale / 200) & 0xfffc;
    if (half_width < width) {
        width = half_width;
    }
    if (half_height < height) {
        height = half_height;
    }
    rectangle->x = half_width - width;
    rectangle->y = half_height - height;
    rectangle->w = width * 2;
    rectangle->h = height * 2;
}
