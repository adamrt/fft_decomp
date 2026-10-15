#include "fft/event_card.h"
#include "fft/menu.h"
#include "psx/types.h"

/*
 * Submit the finished CARD frame and swap to the other graphics context.
 *
 * Resets the primitive counts, waits for the GPU and the configured VSync
 * count, installs the other context's display and draw environments, draws
 * the finished ordering table from entry draw_otag (-1 skips the draw) and
 * clears the new one from first_otag onward.
 */
void card_gfx_submit_frame_and_swap_buffers(s32 first_otag, s32 draw_otag) {
    u32* previous_otag = g_card_gfx_context->otag;
    card_graphics_context_t* context;

    main_system_frame_stub();
    g_card_gfx_poly_ft4_count = 0;
    g_gfx_tile_count = 0;
    while (DrawSync(1) != 0) { }
    VSync(g_menu_event_speed);
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
    ClearOTag(g_card_gfx_context->otag + first_otag, g_gfx_otag_count - first_otag);
}
