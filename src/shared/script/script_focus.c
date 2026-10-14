#include "fft/battle_camera.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/script.h"

/* Patch the next camera instruction to frame two units.
 * A missing unit disables that instruction. */
void script_focus(event_focus_parameters_t* parameters) {
    s32 coords[2][4];
    s32 unit_a;
    s32 unit_b;
    s16 first;
    s16 second;
    u8 keep_angle;
    s32 i;
    u8* cursor;

    cursor = parameters->first_unit_id_le;
    first = script_read_s16(cursor);
    cursor += sizeof(parameters->first_unit_id_le);
    second = script_read_s16(cursor);
    cursor += sizeof(parameters->second_unit_id_le);
    keep_angle = *cursor;
    cursor++;
    while (*cursor != EVENT_OPCODE_CAMERA) {
        cursor += g_script_event_instruction_sizes[*cursor] + 1;
    }
    cursor = ((event_camera_instruction_t*)cursor)->parameters.values.position_le[0];
    unit_a = first;
    unit_b = second;
    first = script_resolve_runtime_id(unit_a);
    second = script_resolve_runtime_id(unit_b);
    if (first == EVENT_RUNTIME_ID_NONE || second == EVENT_RUNTIME_ID_NONE) {
        ((event_camera_instruction_t*)(cursor - offsetof(event_camera_instruction_t, parameters)))->opcode
            = EVENT_OPCODE_CAMERA_DISABLED;
        main_system_handle_malloc_exception(7, 0);
        return;
    }
    unit_copy_world_coords_to_vector(first, (VECTOR*)coords[0]);
    unit_copy_world_coords_to_vector(second, (VECTOR*)coords[1]);
    coords[0][1] += 0x20;
    coords[1][1] += 0x20;
    if (unit_a == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM) {
        coords[0][1] -= 0x50;
        coords[1][1] -= 0x50;
    }
    if (unit_b == CHARACTER_IDENTITY_ALTIMA_FIRST_FORM) {
        coords[0][1] -= 0x20;
        coords[1][1] -= 0x20;
    }
    for (i = 0; i < 3; i++) {
        coords[0][i] = (coords[0][i] + coords[1][i]) * 2;
        script_write_s16(cursor, coords[0][i]);
        cursor += sizeof(((event_camera_values_t*)0)->position_le[0]);
    }
    if (keep_angle == 0) {
        cursor += sizeof(((event_camera_values_t*)0)->position_le[0]);
        script_write_s16(cursor, camera_pick_nearest_unblocked_yaw(first, second));
    }
}
