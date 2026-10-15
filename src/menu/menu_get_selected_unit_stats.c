#include "fft/battle_camera.h"
#include "fft/battle_event.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Return the stats for the menu's selected battle slot. */
unit_stats_t* menu_get_selected_unit_stats(void) {
    return battle_unit_get_stats_from_battle_id(g_menu_selected_unit.battle_id);
}
