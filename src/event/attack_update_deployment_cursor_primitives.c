#include "fft/event_attack.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Animates the deployment cursor palette and positions both cursor sprites.
 *
 * Each cursor n owns a bobbing marker quad and a tile-highlight diamond in
 * buffer->cursor[n]; the cursor tile comes from the two-entry row/column
 * arrays at g_attack_deploy_cursor_row/column. The ternary with the full
 * expression in each arm and the separate height variable reproduce the
 * target's cross-jumped arm tails and its unfolded lift + 10 addition. */
void attack_update_deployment_cursor_primitives(s32 frame, attack_deploy_render_buffer_t* buffer) {
    native_thread_t* thread = &g_battle_threads[g_battle_current_thread_id];
    s32 bob = battle_menu_get_cursor_bob_offset(0);
    s32 i;
    u8(*tiles)[5];

    thread->task_words[0] += g_battle_event_speed;
    battle_copy_bytes(
        &g_attack_deploy_cursor_clut[1], &g_attack_deploy_cursor_palette_cycle[1 + (frame / 4) % 14], 0x1e);
    g_attack_deploy_cursor_clut[0] = 0;
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

        buffer->cursor[i][0].x0 = x - 8;
        buffer->cursor[i][0].y0 = top;
        buffer->cursor[i][0].x1 = x + 8;
        buffer->cursor[i][0].y1 = top;
        buffer->cursor[i][0].x2 = x - 8;
        buffer->cursor[i][0].y2 = top + 0x18;
        buffer->cursor[i][0].x3 = x + 8;
        buffer->cursor[i][0].y3 = top + 0x18;
        buffer->cursor[i][1].x0 = x;
        buffer->cursor[i][1].y0 = y - 10;
        buffer->cursor[i][1].x1 = x + 20;
        buffer->cursor[i][1].y1 = y;
        buffer->cursor[i][1].x2 = x - 20;
        buffer->cursor[i][1].y2 = y;
        buffer->cursor[i][1].x3 = x;
        buffer->cursor[i][1].y3 = y + 10;
    }
}
