#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Opcode 27: take the gravity vector and the inertia threshold from the
 * effect file's particle-system section. */
effect_script_result_e effect_code_script_27_init_particle_physics(effect_record_t* record) {
    effect_set_gravity(&g_effect_geometry_table->gravity);
    effect_set_inertia_threshold(g_effect_geometry_table->inertia_threshold);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
