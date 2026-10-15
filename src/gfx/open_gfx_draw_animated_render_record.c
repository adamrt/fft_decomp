#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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

#define PART(index) ((world_sprite_part_t*)&sprite[index])

void open_gfx_draw_animated_render_record(world_animated_render_record_t* actor, u32* ot) {
    world_sprite_cel_header_t** sprites;
    world_sprite_animation_sequence_t* anim;
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

    sprites = g_open_gfx_record_36_sprites;
    anim = g_open_gfx_record_36_animations[actor->sequence];
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
        setRGB0((&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt),
            actor->color.r, actor->color.g, actor->color.b);
        SetSprt(&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt);
        value = ((part_count - i) * 2) | 1;
        SetDrawMode(&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].mode,
            1, 0, GetTPage(PART(value)->texture_flags >> 2, PART(value)->blend_rate, tpage_x, tpage_y), 0);
        if (PART(value)->semi_transparent) {
            SetSemiTrans(
                &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt, 1);
        } else {
            SetSemiTrans(
                &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt, 0);
        }
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.x0
            = (x = actor->position.x, x + PART(value)->x_offset - 0x80);
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.y0
            = (y = actor->position.y, y + PART(value)->y_offset - 0x80);
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.u0
            = PART(value)->u;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.v0
            = PART(value)->v;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.w
            = PART(value)->width;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.h
            = PART(value)->height;
        /* A byte-field palette read shortens the native 32-bit flags load. */
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.clut
            = GetClut(clut_x
                    + ((actor->palette == 0 || (PART(value)->use_part_palette)) ? ((sprite[value] >> 12) & 0xf0)
                                                                                : ((actor->palette - 1) * 0x10)),
                clut_y);
        AddPrim(&ot[actor->priority],
            &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt);
        AddPrim(&ot[actor->priority],
            &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].mode);
        g_open_gfx_primitive_count++;
    }
}
