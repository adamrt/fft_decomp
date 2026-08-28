#include "fft/card.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void card_gfx_submit_frame_and_swap_buffers(s32 first_otag, s32 draw_otag) {
    u32* previous_otag = g_card_gfx_context->otag;
    card_graphics_context_t* context;

    main_noop_800449ec();
    g_card_gfx_poly_ft4_count = 0;
    g_card_gfx_tile_count = 0;
    while (DrawSync(1) != 0) { }
    VSync(g_card_menu_event_speed);
    context = g_card_gfx_context_base;
    if (g_card_gfx_context == context) {
        context = context + 1;
    }
    g_card_gfx_context = context;
    PutDispEnv(&context->display_environment);
    PutDrawEnv(&g_card_gfx_context->draw_environment);
    g_card_gfx_drawenv_y = g_card_gfx_context->draw_environment.clip.y;
    if (draw_otag != -1) {
        DrawOTag(previous_otag + draw_otag);
    }
    ClearOTag(g_card_gfx_context->otag + first_otag, g_card_gfx_otag_length - first_otag);
}
