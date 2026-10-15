#include "fft/battle.h"
#include "fft/battle_event.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/types.h"

/* Set the event speed and the animation speed together. The event speed also
 * divides the input repeat delays. */
void script_set_event_speed_variant(s32 speed) {
    g_script_event_speed = speed;
    battle_state_set_animation_speed(speed);
}
