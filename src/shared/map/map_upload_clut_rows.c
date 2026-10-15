#include "fft/battle_map.h"

/* Upload the pending CLUT rows as one image to the lines below the rows, and
 * on the next call move them into place. */
void map_upload_clut_rows(void) {
    RECT rectangle;
    s32* move_pending;

    move_pending = &g_map_palette_state.move_pending;

    if (*move_pending != 0) {
        rectangle.x = 0;
        rectangle.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_COUNT;
        rectangle.w = MAP_CLUT_ROW_COLOR_COUNT;
        rectangle.h = MAP_CLUT_ROW_COUNT;
        MoveImage(&rectangle, 0, MAP_CLUT_ROWS_Y);
        *move_pending = 0;
    }

    if (g_map_palette_state.upload_pending != 0) {
        rectangle.x = 0;
        rectangle.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_COUNT;
        rectangle.w = MAP_CLUT_ROW_COLOR_COUNT;
        rectangle.h = MAP_CLUT_ROW_COUNT;
        LoadImage(&rectangle, (u32*)g_map_palette_modified_colors);
        g_map_palette_state.upload_pending = 0;
        *move_pending = 1;
    }
}
