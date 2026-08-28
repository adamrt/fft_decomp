#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef struct world_menu_icon_prims_t world_menu_icon_prims_t;
extern void world_menu_submit_icon_primitives(world_menu_icon_prims_t* menu);

/* Menu icon strip thread: rebuilds the text image every seventh frame and
 * alternates between two sprite records until input arrives. */
void world_menu_icon_strip_thread(void) {
    RECT rect;
    u8 records[2][0x7C];
    s32 input;
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    s32 fresh;
    u8* record;

    input = 0;
    param = world_thread_get_current_parameter_1();
    record = records[0];
    world_menu_build_icon_record(&rect, param, record);
    world_script_copy_bytes(records[1], record, 0x7C);
    fresh = 1;
    for (i = 0;; i++) {
        if (i % 7 == 0) {
            fresh = 1;
            buffer = world_menu_build_and_upload_window_frame_image(param->width, param->height, &rect, 1);
            world_menu_set_text_origin(8, 9);
            g_world_menu_text_state.stride = param->width;
            world_menu_display_text_entry(param->text_id, buffer, &g_world_menu_text_state.origin_x);
            LoadImage(&rect, buffer);
        }
        world_thread_yield();
        if (fresh == 1) {
            world_menu_free_memory(buffer);
            fresh = 0;
        }
        if (world_menu_check_thread_completion(&input) != 0) {
            break;
        }
        record = records[i & 1];
        world_menu_select_icon_cluts((world_menu_icon_sprites_t*)record);
        world_menu_submit_icon_primitives((world_menu_icon_prims_t*)record);
    }
    world_thread_yield();
    world_thread_exit_current();
}
