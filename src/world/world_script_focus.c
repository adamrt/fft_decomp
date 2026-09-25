#include "fft/world.h"
#include "psx/types.h"

/*
 * Focus event instruction: patches the parameters of the next camera
 * instruction (0x19) with twice the midpoint of the two units' positions,
 * raised by 0x20 (lowered for Altima's second and first forms), and, unless
 * the flag byte is set, the camera angle
 * world_camera_pick_nearest_unblocked_yaw picks for the pair.
 * An absent unit disables that camera instruction (opcode 0xc0) instead.
 *
 * first/second hold each unit id and are then reused for its misc id; the
 * target keeps both in the same callee-saved registers.
 */
void world_script_focus(u8* parameters) {
    s32 coords[2][4];
    s32 unit_a;
    s32 unit_b;
    s16 first;
    s16 second;
    u8 keep_angle;
    s32 i;

    first = world_script_load_halfword(parameters);
    parameters += 2;
    second = world_script_load_halfword(parameters);
    parameters += 2;
    keep_angle = *parameters;
    parameters++;
    while (*parameters != 0x19) {
        parameters += g_world_event_instruction_sizes[*parameters] + 1;
    }
    parameters++;
    unit_a = first;
    unit_b = second;
    first = world_get_misc_id(unit_a);
    second = world_get_misc_id(unit_b);
    if (first == EVENT_MISC_ID_NONE || second == EVENT_MISC_ID_NONE) {
        parameters[-1] = 0xc0;
        main_system_handle_malloc_exception(7, 0);
        return;
    }
    world_unit_copy_screen_coordinates(first, (VECTOR*)coords[0]);
    world_unit_copy_screen_coordinates(second, (VECTOR*)coords[1]);
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
        /* The target sign-extends the value; the definition's u16 parameter would zero-extend it. */
        ((void (*)(u8*, s16))world_script_store_halfword)(parameters, coords[0][i]);
        parameters += 2;
    }
    if (keep_angle == 0) {
        parameters += 2;
        /* The target sign-extends the value; the definition's u16 parameter would zero-extend it. */
        ((void (*)(u8*, s16))world_script_store_halfword)(
            parameters, world_camera_pick_nearest_unblocked_yaw(first, second));
    }
}
