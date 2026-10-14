#include "fft/battle_action.h"
#include "fft/memory.h"

/* Reset an action result record to "hit, 100% accuracy, no effect".
 *
 * Bytes 0x00-0x0d, 0x10-0x25 and 0x28-0x29 are cleared; reaction_id (0x0e) and
 * last_received_attack (0x26) keep their values. The canceled-status list is
 * cleared with the record's status changes. */
void action_clear_current_data(action_data_t* action) {
    mem_zero(action, 0xE);
    mem_zero(&action->special_effect, 0x16);
    mem_zero(&action->exp_change, 2);
    /* The target stores the accuracy as a 16-bit value (0x2a..0x2b). */
    action->attack_accuracy = 100;
    action->attack_type = 0;
    action->hit = 1;
    action_clear_status_changes(action);
}
