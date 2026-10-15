#include "fft/gfx.h"
#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
void open_gfx_draw_window_record(open_render_record_56_t* record, u32* ot) {
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
    SetSprt(&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt);

    if (record->palette != 0) {
        clut.x = (cx = record->tail.clut.x) + (record->palette - 1) * 0x10;
        clut.y = record->tail.clut.y;
    } else {
        clut = record->tail.clut;
    }
    g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.clut
        = GetClut(clut.x, clut.y);
    setRGB0((&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt),
        record->color.r, record->color.g, record->color.b);

    SetDrawMode(&g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].mode, 1,
        0,
        GetTPage(((record->flags & WORLD_RENDER_RECORD_8BIT_CLUT) >> 2) != 0,
            (record->flags & WORLD_RENDER_RECORD_BLEND_RATE_MASK) >> 6, record->vram_x, record->vram_y),
        0);

    if (record->flags & WORLD_RENDER_RECORD_SEMI_TRANSPARENT) {
        SetSemiTrans(
            &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt, 1);
    } else {
        SetSemiTrans(
            &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt, 0);
    }

    if (record->flags & WORLD_RENDER_RECORD_POP_IN) {
        gfx_calculate_scaled_rectangle(record->pop_in_step / 2, &record->tail.quad, &scaled);
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.x0
            = scaled.x + (px = record->x);
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.y0
            = scaled.y + (py = record->y);
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.u0
            = record->tail.quad.x + scaled.x;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.v0
            = record->tail.quad.y + scaled.y;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.w = scaled.w;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.h = scaled.h;
        step = record->pop_in_step;
        if (step / 2 >= 4) {
            record->pop_in_step = 0;
            record->flags ^= WORLD_RENDER_RECORD_POP_IN;
        } else {
            record->pop_in_step = step + 1;
        }
    } else {
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.x0 = qx
            = record->x;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.y0 = qy
            = record->y;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.u0 = qu
            = record->tail.quad.x;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.v0 = qv
            = record->tail.quad.y;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.w
            = record->tail.quad.w;
        g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt.h
            = record->tail.quad.h;
    }

    AddPrim(&ot[record->ot_layer],
        &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].sprt);
    AddPrim(&ot[record->ot_layer],
        &g_open_gfx_sprite_primitive_pool[g_active_graphics_buffer_index][g_open_gfx_primitive_count].mode);
    g_open_gfx_primitive_count++;
}
