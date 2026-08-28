#include "fft/effect.h"

/* Returns 0 unless the record has a work list and requested_index is inside
 * it.  Note the retail loop walks work_count links, not requested_index, so
 * the returned node is always the same for a given record. */
effect_work_record_t* battle_effect_get_list_node(s16 record_index, s16 requested_index) {
    effect_record_t* record = &g_effect_state_records[record_index];
    effect_work_record_t* node = 0;
    s16 node_count;
    s16 i;

    if ((record->flags & 1) != 0) {
        if (requested_index >= 0) {
            node_count = record->work_count;
            if (requested_index < node_count) {
                node = record->work_head;
                for (i = 0; i < node_count; i++)
                    node = node->next;
            }
        }
    }
    return node;
}
