#ifndef CONDITIONS_H
#define CONDITIONS_H

/* REQUIRE's battle conditions banners (DisplayConditions): the ready, special, general and congratulations banners
 * from BONUS.BIN, DarkScreen's banner bursts and color fade, and the REQUIRE entrypoints that run them. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* banners */
/* The BONUS.BIN banner selected by conditions_special_thread's first parameter.
 * These labels are separate from the event instruction's display-condition operations. */
typedef enum conditions_banner {
    CONDITIONS_BANNER_READY = 0,
    CONDITIONS_BANNER_CONGRATULATIONS = 1,
    CONDITIONS_BANNER_BONUS_MONEY = 2,
    CONDITIONS_BANNER_GIL_AMOUNT = 3,
    CONDITIONS_BANNER_WAR_TROPHIES = 4,
    CONDITIONS_BANNER_WARNING = 5,
    CONDITIONS_BANNER_PARTING_SHOT = 6,
    CONDITIONS_BANNER_BATTLE_COMPLETE = 7,
    CONDITIONS_BANNER_HE_LEFT_COMPANY = 8,
    CONDITIONS_BANNER_SHE_LEFT_COMPANY = 9,
    CONDITIONS_BANNER_JOIN_UP = 10
} conditions_banner_e;

void conditions_ready_bugged_thread(void);
void conditions_show_general_banner(s32 hold_frames);
void conditions_special_thread(void);
void conditions_congratulations_thread(void);

/* REQUIRE's condition/reward dispatcher, called by the event library in BATTLE and WORLD. */
void require_conditions_entrypoint(void);

/* banner bursts */
extern RECT g_conditions_banner_palette_rect;

void conditions_load_banner_image(s32 image_id);

/* REQUIRE's banner-burst thread, entered by DarkScreen after the overlay loads. */
void require_dark_screen_entrypoint(void);

/* dark screen */
extern u8 g_after_battle_gfx_fade_rgb[3];

void conditions_fade_dark_screen_color(s32 target_red, s32 target_green, s32 target_blue);

#endif
