#include "fft/battle_map.h"
#include "fft/script.h"
#include "fft/thread.h"

/* Thread body of MapDarkness: start the ambient-light blend described by the
 * operands (blend mode, red, green, blue as s8, duration index), then keep the
 * thread alive under the MapDarkness task id for
 * g_script_map_darkness_durations[duration index] frames (1 to 256). WORLD's copy
 * also calls the BATTLE blend at 0x800934d0, which is WLDCORE data there. */
void script_map_darkness(void) {
    event_unit_color_t* parameters;

    thread_set_current_task_id(THREAD_TASK_MAP_DARKNESS);
    parameters = (event_unit_color_t*)thread_get_current_parameter_1_variant();
    map_start_ambient_light_blend(
        parameters->mode, parameters->duration, parameters->rgb.r, parameters->rgb.g, parameters->rgb.b);
    thread_wait_frames(g_script_map_darkness_durations[parameters->duration]);
    thread_exit_current();
}
