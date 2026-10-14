#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/types.h"

/* ColorUnit: start a palette modulation on a unit or a unit group.
 *
 * Operands: unit id or group (16-bit value), blend mode, red, green, blue (s8 each)
 * and a time. A plain unit id colors that one unit; id 0 and the group ids from
 * EVENT_UNIT_GROUP_FIRST run all 21 unit ids through
 * script_filter_unit_id_by_mode. Nothing happens when a plain unit id does
 * not resolve. */
void script_color_unit_event_instruction(const event_color_unit_parameters_t* parameters) {
    const event_unit_color_t* color;
    u16 unit_id;
    s32 out;
    s32 i;

    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &out) != 0) {
        color = &parameters->color;
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&i, &out) != 0) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    color->mode, color->duration, unit_id, color->rgb.r, color->rgb.g, color->rgb.b);
                if (out == 0) {
                    return;
                }
            }
        }
    }
}
