/* Proposition speaker pick
 * (0x8007920c-0x800793f0). Returns the only participant, else the one with
 * the highest/lowest bravery (mode 1/2) or faith (3/4) by the u16 mode at
 * 0x8009f312 (unpacked proposition row 0x8009f2ec + 0x26). The goto form
 * for the shared bravery/faith tails matches the target, which reloads the
 * stat after the compare. */
#include "fft/wldcore.h"

s32 wldcore_proposition_select_report_speaker(s32 index) {
    party_data_t* unit;
    s32 best;
    s32 i;
    s32 selected;

    if (g_main_active_propositions[index].participant_count == 1) {
        return g_main_active_propositions[index].participant_indices[0];
    }
    best = (g_wldcore_selected_proposition_row[0].fields.speaker_mode == 1
               || g_wldcore_selected_proposition_row[0].fields.speaker_mode == 3)
        ? -1
        : 99999;
    for (i = 0; i < g_main_active_propositions[index].participant_count; i++) {
        unit = wldcore_get_party_data_pointer(g_main_active_propositions[index].participant_indices[i]);
        switch (g_wldcore_selected_proposition_row[0].fields.speaker_mode) {
        case 1:
            if (best < unit->bravery) {
                goto take_bravery;
            }
            break;
        case 2:
            if (unit->bravery < best) {
            take_bravery:
                best = unit->bravery;
                selected = g_main_active_propositions[index].participant_indices[i];
            }
            break;
        case 3:
            if (best < unit->faith) {
                goto take_faith;
            }
            break;
        case 4:
            if (unit->faith < best) {
            take_faith:
                best = unit->faith;
                selected = g_main_active_propositions[index].participant_indices[i];
            }
            break;
        }
    }
    return selected;
}
