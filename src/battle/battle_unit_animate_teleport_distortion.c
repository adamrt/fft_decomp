#include "psx/gte.h"
#include "psx/types.h"

/* Primitive the pulse animation scales; only the two 16.16-fraction scale
 * fields at 0x08/0x0a are touched here. */
typedef struct battle_unit_teleport_pulse_prim {
    u8 _unused_00[0x8];
    s16 scale_x; /* 0x08 */
    s16 scale_y; /* 0x0a */
} battle_unit_teleport_pulse_prim_t;

/* Renderer-side object shared with battle_move_interpolate_height_toward_fixed_drop (0x087..0x090 agree). */
typedef struct battle_unit_teleport_pulse_obj {
    u8 _unused_000[0x87];
    u8 active;      /* 0x087 */
    s32 phase;      /* 0x088 */
    s32 angle;      /* 0x08c */
    s32 fade_angle; /* 0x090 */
    u8 _unused_094[0x170];
    battle_unit_teleport_pulse_prim_t* prim; /* 0x204 */
} battle_unit_teleport_pulse_obj_t;

/* Falls off the end: the original returns int, so $v0 stays live at the exit and the
 * dispatch branch's delay slot is left empty. */
s32 battle_unit_animate_teleport_distortion(battle_unit_teleport_pulse_obj_t* pulse) {
    s32 angle;
    s32 fade;

    switch (pulse->phase) {
    case 0:
        pulse->phase = 1;
        pulse->angle = 0x300;
        pulse->fade_angle = 0;
    case 1:
        angle = pulse->fade_angle;
        pulse->prim->scale_x = rsin(pulse->angle);
        fade = rsin(angle);
        pulse->prim->scale_y = fade + fade / 2 + ONE;
        if ((pulse->angle & 0xfff) != 0x800) {
            pulse->angle = pulse->angle + 32;
            pulse->fade_angle = pulse->fade_angle + 51;
        } else {
            pulse->phase = pulse->phase + 1;
        }
        break;
    case 2:
        pulse->active = 0;
        pulse->prim->scale_x = ONE;
        pulse->prim->scale_y = ONE;
        break;
    }
}
