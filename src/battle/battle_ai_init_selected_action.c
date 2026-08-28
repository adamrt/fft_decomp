#include "fft/battle_ai.h"
#include "psx/types.h"

/* Target 0x8019ae30. */
void battle_ai_init_selected_action(void) {
    battle_ai_reset_action_ranking_fields(&g_battle_ai_data_base.selected_action);
}
