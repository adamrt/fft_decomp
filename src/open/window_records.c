/* Window record pools, animation and direct GPU sprite drawing for the title menus. */

#include "fft/gfx.h"
#include "fft/system.h"
#include "fft/window.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

static void gfx_calculate_scaled_rectangle(s32 scale_mode, const RECT* source, RECT* rectangle);
static void window_draw_gpu_animated_record(display_object_animated_t* actor, u32* ot);
static void window_draw_gpu_image_record(window_gpu_image_record_t* record, u32* ot);

/* Empty both render record pools and the pointer list. Every record gets its
 * layout flag, ordering-table entry 8 and the neutral color 0x80. */
void window_init_record_pools(void) {
    s32 i;
    CVECTOR* color;

    g_window_primitive_count = 0;
    g_window_render_record_count = 0;
    g_window_record_count = 0;
    g_window_render_object_count = 0;

    for (i = 0; i < WINDOW_GPU_RECORD_CAPACITY; i++) {
        g_window_records[i].flags = DISPLAY_OBJECT_ANIMATED;
        g_window_records[i].priority = 8;
        color = &g_window_records[i].color;
        color->b = color->g = color->r = 0x80;
    }

    for (i = 0; i < WINDOW_GPU_RECORD_CAPACITY; i++) {
        g_window_gpu_image_records[i].flags = DISPLAY_OBJECT_IMAGE;
        g_window_gpu_image_records[i].ot_layer = 8;
        color = &g_window_gpu_image_records[i].color;
        color->b = color->g = color->r = 0x80;
    }
}

/* Clear the active buffer's 16-entry ordering table and release the frame's
 * sprite primitives. */
void gfx_clear_current_otag(void) {
    /* Removing this unused array shrinks the original 32-byte frame to 24. */
    u32 stack_padding[2];

    ClearOTagR(g_gfx_otags[g_gfx_active_buffer_index], 16);
    g_window_primitive_count = 0;
}

/* Draw visible animated and image records into the GPU ordering table. */
void window_dispatch_gpu_record_list(u32* otag, void** list, s32 count) {
    void* object;
    u32* flags;
    s32 i;

    /* OPEN never sets its runtime hide flag. */
    if (g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_RENDER_RECORDS_HIDDEN) {
        return;
    }
    for (i = 0; i < count; i++) {
        object = list[i];
        flags = object;
        if (*flags & (DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN)) {
            continue;
        }
        if (*flags & DISPLAY_OBJECT_ANIMATED) {
            display_object_animated_t* animated = object;
            window_draw_gpu_animated_record(animated, otag);
        } else if (*flags & DISPLAY_OBJECT_IMAGE) {
            window_gpu_image_record_t* image = object;
            window_draw_gpu_image_record(image, otag);
        }
    }
}

/* A stored part pointer changes register allocation and address calculation. */
#define PART(index) ((display_object_part_t*)&sprite[index])

/* Advances one OPEN sprite actor's animation and emits its parts into an
 * ordering table.
 *
 * The actor names an animation in the table at g_window_anim_sequences; each animation is
 * a frame count followed by {sprite_id, duration} pairs, and a duration of
 * 0xffff holds the frame indefinitely. The named sprite in g_window_anim_cels is a
 * part list whose odd 32-bit entries carry the texture page, semi-transparency and
 * palette bits and whose byte fields give the part offset, size and texture
 * coordinates. Each part consumes one DR_MODE + SPRT pair from the shared
 * 32-entry per-buffer primitive pool at g_window_sprite_primitive_pool.
 *
 * Both high halves are masked before the shift. The code is the same as a
 * plain shift, but it matches the target's frame: CSE shares the 0xffff0000
 * constant between the two statements, so the first mask/shift combine keeps
 * that constant for the second and leaves its dead AND result as a USE-only
 * pseudo. That pseudo and the part loop's folded entry test each get an
 * unreferenced 8-byte reload slot (0x38 and 0x40). Reading x and y into their
 * own locals within the assignments keeps the target's x + byte operand order
 * and v0/v1 allocation. Separate assignments change those registers.
 */
static void window_draw_gpu_animated_record(display_object_animated_t* actor, u32* ot) {
    display_object_cel_header_t** sprites = g_window_anim_cels;
    display_object_sequence_header_t* anim = g_window_anim_sequences[actor->sequence];
    display_object_cel_header_t* cel;
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
     * constant into a register, which the target does not. Frame records follow the serialized header. Pointer addition
     * keeps the index-first address sum; array indexing reverses it. */
loop:
    value = ((display_object_frame_t*)(anim + 1) + actor->frame_index)->duration;
    if (value == actor->anim_counter && value != 0xffff) {
        actor->anim_counter = 0;
        actor->frame_index++;
        value = anim->frame_count;
        if (value == actor->frame_index) {
            actor->frame_index = 0;
        }
        goto loop;
    }

    value = ((display_object_frame_t*)(anim + 1) + actor->frame_index)->cel;
    cel = sprites[value];
    /* Preserve the reverse 32-bit cursor; a part array changes address computation. */
    sprite = (u32*)cel;
    tpage_x = cel->packed_texture_page_origin & 0xffff;
    tpage_y = (cel->packed_texture_page_origin & 0xffff0000) >> 16;
    part_count = cel->part_count;
    clut_x = cel->packed_clut_origin & 0xffff;
    clut_y = (cel->packed_clut_origin & 0xffff0000) >> 16;

    for (i = 0; i < part_count; i++) {
        if (g_window_primitive_count >= 32) {
            return;
        }
        setRGB0((&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt),
            actor->color.r, actor->color.g, actor->color.b);
        SetSprt(&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt);
        value = ((part_count - i) * 2) | 1;
        SetDrawMode(&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].mode, 1, 0,
            GetTPage(PART(value)->texture_flags >> 2, PART(value)->blend_rate, tpage_x, tpage_y), 0);
        if (PART(value)->semi_transparent) {
            SetSemiTrans(&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt, 1);
        } else {
            SetSemiTrans(&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt, 0);
        }
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.x0
            = (x = actor->position.x, x + PART(value)->x_offset - 0x80);
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.y0
            = (y = actor->position.y, y + PART(value)->y_offset - 0x80);
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.u0 = PART(value)->u;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.v0 = PART(value)->v;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.w = PART(value)->width;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.h
            = PART(value)->height;
        /* A byte-field palette read shortens the native 32-bit flags load. */
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.clut = GetClut(clut_x
                + ((actor->palette == 0 || (PART(value)->use_part_palette)) ? ((sprite[value] >> 12) & 0xf0)
                                                                            : ((actor->palette - 1) * 16)),
            clut_y);
        AddPrim(&ot[actor->priority],
            &g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt);
        AddPrim(&ot[actor->priority],
            &g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].mode);
        g_window_primitive_count++;
    }
}

/* Emits one 56-byte render record as a DR_MODE + SPRT pair from the shared
 * 32-entry per-buffer primitive pool at g_window_sprite_primitive_pool.
 *
 * 8BIT_CLUT and BLEND_RATE_MASK select the texture page mode, SEMI_TRANSPARENT
 * enables semi-transparency, and POP_IN runs the pop-in scale: pop_in_step counts the
 * animation, gfx_calculate_scaled_rectangle turns half of it into a
 * shrunken source rectangle, and the flag clears once the record is at full
 * size. A nonzero palette picks the CLUT (palette - 1) * 16 pixels right of clut.
 *
 * The retail code loads record fields at their declared width before
 * truncating them into the primitive (as if built with -fforce-mem); the
 * px/py/qx/qy/cx/qu/qv locals preserve those load widths before the stores.
 * Sharing the position locals across the pop-in and full-size arms changes
 * load scheduling and register allocation.
 */
static void window_draw_gpu_image_record(window_gpu_image_record_t* record, u32* ot) {
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

    if (g_window_primitive_count >= 32) {
        return;
    }
    SetSprt(&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt);

    if (record->palette != 0) {
        cx = record->clut.x;
        clut.x = cx + (record->palette - 1) * 16;
        clut.y = record->clut.y;
    } else {
        clut = record->clut;
    }
    g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.clut
        = GetClut(clut.x, clut.y);
    setRGB0((&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt),
        record->color.r, record->color.g, record->color.b);

    /* The shift before the boolean test preserves the target's mask-and-test sequence. */
    SetDrawMode(&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].mode, 1, 0,
        GetTPage(((record->flags & DISPLAY_OBJECT_8BIT_CLUT) >> 2) != 0,
            (record->flags & DISPLAY_OBJECT_BLEND_RATE_MASK) >> 6, record->vram_x, record->vram_y),
        0);

    if (record->flags & DISPLAY_OBJECT_SEMI_TRANSPARENT) {
        SetSemiTrans(&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt, 1);
    } else {
        SetSemiTrans(&g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt, 0);
    }

    if (record->flags & DISPLAY_OBJECT_POP_IN) {
        gfx_calculate_scaled_rectangle(record->pop_in_step / 2, &record->bounds, &scaled);
        px = record->x;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.x0 = scaled.x + px;
        py = record->y;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.y0 = scaled.y + py;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.u0
            = record->bounds.x + scaled.x;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.v0
            = record->bounds.y + scaled.y;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.w = scaled.w;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.h = scaled.h;
        step = record->pop_in_step;
        if (step / 2 >= 4) {
            record->pop_in_step = 0;
            record->flags ^= DISPLAY_OBJECT_POP_IN;
        } else {
            record->pop_in_step = step + 1;
        }
    } else {
        qx = record->x;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.x0 = qx;
        qy = record->y;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.y0 = qy;
        qu = record->bounds.x;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.u0 = qu;
        qv = record->bounds.y;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.v0 = qv;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.w = record->bounds.w;
        g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt.h = record->bounds.h;
    }

    AddPrim(&ot[record->ot_layer],
        &g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].sprt);
    AddPrim(&ot[record->ot_layer],
        &g_window_sprite_primitive_pool[g_gfx_active_buffer_index][g_window_primitive_count].mode);
    g_window_primitive_count++;
}

/* Appends the next free 36-byte render record to `list`, resets it and
 * returns its pool index. */
s32 window_append_animated_record(void** list, s32* count) {
    list[*count] = &g_window_records[g_window_record_count];
    g_window_records[g_window_record_count].flags
        &= ~(DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_SEMI_TRANSPARENT);
    g_window_records[g_window_record_count].palette = 0;
    /* Direct color members change the shared base and add four instructions. */
    (&g_window_records[g_window_record_count].color)->r = 0x80;
    (&g_window_records[g_window_record_count].color)->g = 0x80;
    (&g_window_records[g_window_record_count].color)->b = 0x80;
    (*count)++;
    return g_window_record_count++;
}

/* Appends the next free 56-byte render record to `list`, resets it and
 * returns its pool index. */
s32 window_append_record(void** list, s32* count) {
    list[*count] = &g_window_gpu_image_records[g_window_render_record_count];
    g_window_gpu_image_records[g_window_render_record_count].flags
        &= ~(DISPLAY_OBJECT_8BIT_CLUT | DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_SEMI_TRANSPARENT);
    g_window_gpu_image_records[g_window_render_record_count].palette = 0;
    /* Direct color members change the shared base and add four instructions. */
    (&g_window_gpu_image_records[g_window_render_record_count].color)->r = 0x80;
    (&g_window_gpu_image_records[g_window_render_record_count].color)->g = 0x80;
    (&g_window_gpu_image_records[g_window_render_record_count].color)->b = 0x80;
    g_window_gpu_image_records[g_window_render_record_count].pop_in_step = 0;
    (*count)++;
    return g_window_render_record_count++;
}

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
