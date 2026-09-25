#include "fft/battle.h"
#include "fft/world.h"

struct battle_screen_coords;

/* Copy the three screen-data halfwords of the current misc id into the world's
 * coordinate triple; a missing record (-1) leaves it untouched.
 *
 * The target materialises the destination base with lui/addiu for the first
 * store only and reaches the other two through the symbol. */
void world_copy_misc_unit_screen_coords(void) {
    vec3u16_t* source = ((vec3u16_t * (*)(void)) battle_unit_get_screen_data_ptr_by_misc_id)();
    /* Unpinned, the base and the copied halfword swap $v0 and $v1. */
    register vec3u16_t* destination __asm__("$2");

    if (source != (vec3u16_t*)-1) {
        destination = &g_world_misc_unit_screen_coords;
        /* Keeps combine from folding the base into the first store's address. */
        __asm__("" : : "r"(destination));
        destination->x = source->x;
        g_world_misc_unit_screen_coords.y = source->y;
        g_world_misc_unit_screen_coords.z = source->z;
    }
}
