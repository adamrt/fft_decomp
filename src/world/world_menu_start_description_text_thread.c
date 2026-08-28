#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

/* Start the text thread rendering the pending menu description into the
 * record's text buffer; ids above 0x1ffff are only latched. */
void world_menu_start_description_text_thread(world_menu_description_record_t* record) {
    s32 text_id = g_world_menu_description_text_id;

    if (text_id > 0x1ffff) {
        g_world_grid_menu_id = text_id;
        return;
    }
    if (text_id > 0) {
        world_thread_start(1, world_text_character_handling_thread);
        world_thread_set_parameters(1, (s32)record->description_text, g_world_menu_description_text_id, 0);
        g_world_thread_task_active = 1;
    }
}
