#include "fft/event_attack.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Animates the deployment cursor palette and positions both cursor sprites.
 *
 * Each cursor n owns a bobbing marker quad and a tile-highlight diamond in
 * buffer->cursor[n]; the cursor tile comes from the two-entry row/column
 * arrays at g_attack_deploy_cursor_row/column. The ternary with the full
 * expression in each arm and the separate height variable reproduce the
 * target's cross-jumped arm tails and its unfolded lift + 10 addition. */
void attack_deploy_update_cursor_primitives(s32 frame, attack_deploy_render_buffer_t* buffer) {
    thread_t* thread = &g_threads[g_thread_current_id];
    s32 bob = menu_get_cursor_bob_offset(0);
    s32 i;
    u8(*tiles)[5];

    thread->task_words[0] += g_script_event_speed;
    mem_copy_bytes_nonempty(
        &g_attack_deploy_cursor_clut[1], &g_attack_deploy_cursor_palette_cycle[0][1 + (frame / 4) % 14], 0x1e);
    g_attack_deploy_cursor_clut[0].red = g_attack_deploy_cursor_clut[0].green = g_attack_deploy_cursor_clut[0].blue
        = g_attack_deploy_cursor_clut[0].semi_transparent = 0;
    LoadImage(&g_attack_deploy_cursor_clut_rect, (u32*)g_attack_deploy_cursor_clut);

    for (i = 0, tiles = g_attack_deploy_valid_tiles; i < 2; i++) {
        s32 row = g_attack_deploy_cursor_row[i];
        s32 column = g_attack_deploy_cursor_column[i];
        s32 x;
        s32 y;
        s32 top;
        s32 lift;
        s32 height;
        s32 offset = bob - 0x40;

        height = 10;
        lift = -4;
        if (tiles[row][column] == 0) {
            lift = 4;
        }
        x = row * 20 + 0x100 - column * 20 + i;
        y = (g_attack_deploy_tiles_only_mode != 0 ? 0x88 - row * 10 - column * 10 + (lift + height)
                                                  : 0xc4 - row * 10 - column * 10 + (lift + height))
            - i * 3;
        top = y + offset;

        setXY4(&buffer->cursor[i][0], x - 8, top, x + 8, top, x - 8, top + 0x18, x + 8, top + 0x18);
        setXY4(&buffer->cursor[i][1], x, y - 10, x + 20, y, x - 20, y, x, y + 10);
    }
}
