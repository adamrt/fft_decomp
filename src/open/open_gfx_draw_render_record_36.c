#include "fft/main_runtime.h"
#include "fft/open.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Advances one OPEN sprite actor's animation and emits its parts into an
 * ordering table.
 *
 * The actor names an animation in the table at g_open_gfx_record_36_animations; each animation is
 * a frame count followed by {sprite_id, duration} pairs, and a duration of
 * 0xffff holds the frame indefinitely. The named sprite in g_open_gfx_record_36_sprites is a
 * part list whose odd words carry the texture page, semi-transparency and
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

typedef struct open_gfx_sprite_animation_frame {
    /* 0x00 */ u16 sprite_id;
    /* 0x02 */ u16 duration;
} open_gfx_sprite_animation_frame_t;

typedef struct open_gfx_sprite_animation {
    /* 0x00 */ s32 frame_count;
    /* 0x04 */ open_gfx_sprite_animation_frame_t frames[1];
} open_gfx_sprite_animation_t;

/* One sprite part: its first word also carries the texture/palette flag bits
 * read as sprite[t]; the part list is walked two words at a time. */
typedef struct open_gfx_sprite_part {
    /* 0x00 */ u8 x_offset;    /* biased by 0x80 */
    /* 0x01 */ u8 y_offset;    /* biased by 0x80 */
    /* 0x02 */ u16 attributes; /* texture page, semi-transparency and palette bits */
    /* 0x04 */ u8 h;
    /* 0x05 */ u8 w;
    /* 0x06 */ u8 v;
    /* 0x07 */ u8 u;
} open_gfx_sprite_part_t;

#define PART(index) ((open_gfx_sprite_part_t*)&sprite[index])

extern open_gfx_sprite_animation_t** g_open_gfx_record_36_animations;

void open_gfx_draw_render_record_36(open_sprite_actor_t* actor, u32* ot) {
    u32** sprites;
    open_gfx_sprite_animation_t* anim;
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
    anim = g_open_gfx_record_36_animations[actor->anim_id];
    actor->frame_timer++;
    /* A goto loop: as a for/while loop, GCC's loop pass hoists the 0xffff
     * constant into a register, which the target does not. */
loop:
    value = (anim->frames + actor->frame_index)->duration;
    if (value == actor->frame_timer && value != 0xffff) {
        actor->frame_timer = 0;
        actor->frame_index++;
        value = anim->frame_count;
        if (value == actor->frame_index) {
            actor->frame_index = 0;
        }
        goto loop;
    }

    value = (anim->frames + actor->frame_index)->sprite_id;
    sprite = sprites[value];
    tpage_x = sprite[1] & 0xffff;
    tpage_y = (sprite[1] & 0xffff0000) >> 16;
    part_count = sprite[0];
    clut_x = sprite[2] & 0xffff;
    clut_y = (sprite[2] & 0xffff0000) >> 16;

    for (i = 0; i < part_count; i++) {
        if (g_open_gfx_primitive_count >= 0x20) {
            return;
        }
        ((SPRT*)&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt)->r0
            = actor->r;
        ((SPRT*)&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt)->g0
            = actor->g;
        ((SPRT*)&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt)->b0
            = actor->b;
        SetSprt(&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt);
        value = ((part_count - i) * 2) | 1;
        SetDrawMode(&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].mode,
            1, 0, GetTPage((sprite[value] >> 26) & 1, (sprite[value] >> 28) & 3, tpage_x, tpage_y), 0);
        if (sprite[value] & 0x40000000) {
            SetSemiTrans(
                &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt, 1);
        } else {
            SetSemiTrans(
                &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt, 0);
        }
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.x0
            = (x = actor->x, x + PART(value)->x_offset - 0x80);
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.y0
            = (y = actor->y, y + PART(value)->y_offset - 0x80);
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.u0
            = PART(value)->u;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.v0
            = PART(value)->v;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.w
            = PART(value)->w;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.h
            = PART(value)->h;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.clut
            = GetClut(clut_x
                    + ((actor->palette == 0 || (sprite[value] & 0x08000000)) ? ((sprite[value] >> 12) & 0xf0)
                                                                             : ((actor->palette - 1) * 0x10)),
                clut_y);
        AddPrim(&ot[actor->ot_layer],
            &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt);
        AddPrim(&ot[actor->ot_layer],
            &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].mode);
        g_open_gfx_primitive_count++;
    }
}
