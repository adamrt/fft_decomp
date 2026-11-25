/* Power-on and session setup in the resident executable: the Now Loading message that file loads and
 * battle transitions blink, the GPU display and the Squaresoft and SCEA boot logos, hardware and
 * shared-resource setup for the first boot and later session resets, ENTD loading and roster
 * construction for an encounter, the new-game inventory and roster, and the Orbonne caption shown as
 * a new game begins. BATTLE can replace the debug print font with EFC_FNT.TIM. The original compiled
 * everything from the display setup to the caption as one object; the loading message just before it
 * is unproven but builds the same display buffers. */

#include "fft/battle/encounter.h"
#include "fft/game/battle_unit.h"
#include "fft/game/entd.h"
#include "fft/game/item.h"
#include "fft/game/party.h"
#include "fft/game/status.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/zodiac.h"
#include "fft/menu/menu_screen.h"
#include "fft/menu/menu_sort.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/math.h"
#include "fft/sys/memcard.h"
#include "fft/sys/memory.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/system.h"
#include "fft/world/save.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* loading message */

extern palette_t g_loading_palette;
static u32 s_loading_frame_counter;
extern u32 g_loading_visible;
extern POLY_FT4 g_loading_primitives[2][7];

/* Build the "Now Loading" message: seven glyph quads from texture page 0x1f with
 * the CLUT loaded to VRAM (0x70, 0x1e2), one copy per display buffer, the text
 * starting at (x + 0xa0, y + 0xc8).
 *
 * visible is stored for loading_message_append_to_otag, which blinks the
 * message, and the blink counter restarts. */
void loading_message_build(u32 visible, s32 x, s32 y) {
    POLY_FT4* prims = g_loading_primitives[0];
    POLY_FT4* poly;
    s32 i;

    g_loading_visible = visible;
    setPolyFT4(&prims[0]);
    s_loading_frame_counter = 0;
    setRGB0(&prims[0], 0x80, 0x80, 0x80);
    SetSemiTrans(&prims[0], 0);
    prims[0].tpage = 0x1f;
    prims[0].clut = getClut(0x70, 0x1e2);
    for (i = 1; i < 7; i++) {
        prims[i] = prims[0];
    }
    /* Dead store, deleted by the compiler: a mention of prims after the copy
     * loop is what makes GCC hoist the block-move end bound (prims + 0x20)
     * out of the loop and allocate prims to s0. */
    poly = prims;

    poly = &g_loading_primitives[0][0];
    setXY4(poly, x + 160, y + 200, x + 172, y + 200, x + 160, y + 208, x + 172, y + 208);
    setUV4(poly, 228, 155, 240, 155, 228, 163, 240, 163);
    g_loading_primitives[1][0] = *poly;

    poly = &g_loading_primitives[0][1];
    setXY4(poly, x + 171, y + 200, x + 178, y + 200, x + 171, y + 208, x + 178, y + 208);
    setUV4(poly, 228, 163, 235, 163, 228, 171, 235, 171);
    g_loading_primitives[1][1] = *poly;

    poly = &g_loading_primitives[0][2];
    setXY4(poly, x + 183, y + 200, x + 195, y + 200, x + 183, y + 208, x + 195, y + 208);
    setUV4(poly, 173, 85, 185, 85, 173, 93, 185, 93);
    g_loading_primitives[1][2] = *poly;

    poly = &g_loading_primitives[0][3];
    setXY4(poly, x + 195, y + 200, x + 200, y + 200, x + 195, y + 208, x + 200, y + 208);
    setUV4(poly, 209, 96, 214, 96, 209, 104, 214, 104);
    g_loading_primitives[1][3] = *poly;

    poly = &g_loading_primitives[0][4];
    setXY4(poly, x + 199, y + 200, x + 202, y + 200, x + 199, y + 208, x + 202, y + 208);
    setUV4(poly, 124, 82, 127, 82, 124, 90, 127, 90);
    g_loading_primitives[1][4] = *poly;

    poly = &g_loading_primitives[0][5];
    setXY4(poly, x + 201, y + 200, x + 206, y + 200, x + 201, y + 208, x + 206, y + 208);
    setUV4(poly, 206, 204, 211, 204, 206, 212, 211, 212);
    g_loading_primitives[1][5] = *poly;

    poly = &g_loading_primitives[0][6];
    setXY4(poly, x + 205, y + 202, x + 210, y + 202, x + 205, y + 210, x + 210, y + 210);
    setUV4(poly, 235, 164, 240, 164, 235, 172, 240, 172);
    g_loading_primitives[1][6] = *poly;

    /* 16-color palette of the NOW LOADING glyph sheet, loaded to VRAM (0x70, 0x1e2). */
    LoadClut2((u32*)g_loading_palette, 0x70, 0x1e2);
}

/* Build the Now Loading message for BATTLE's screen: x = 0x80 is GFX_BATTLE_SCREEN_X,
 * BATTLE's draw-area origin. */
void loading_message_build_offset(u32 visible) {
    loading_message_build(visible, 0x80, 0);
}

/* Link the "Now Loading" quads of the current display buffer into otag while the
 * message is visible. It blinks: 60 calls shown, 60 calls hidden. */
void loading_message_append_to_otag(u32* otag) {
    int i;

    if (g_loading_visible != 0) {
        if (!((s_loading_frame_counter / 60) & 1)) {
            for (i = 0; i < 7; i++) {
                AddPrim(otag, &g_loading_primitives[g_gfx_screen_polarity][i]);
            }
        }
        s_loading_frame_counter++;
    }
}

/* display and boot logos */

/* Low immediate byte of FntLoad's CLUT-Y addiu instruction. */
extern u8 g_psyq_gpu_font_clut_y_offset_immediate;
extern u32 g_boot_squaresoft_logo_image_words[180 * 16 / 2]; /* 180x16 16-bit pixels */

/* Install EFC_FNT.TIM as the debug print font: load the built-in font at VRAM (0x280, 0) with its
 * CLUT moved to row 0x7f, overwrite both with EFC_FNT.TIM (retrying until it reads) and open a
 * 512-character print stream at (0x88, 0x10) as the dump font. */
void gfx_load_debug_font(void) {
    TIM_IMAGE image;
    u32* data;
    int loaded = 0;

    g_psyq_gpu_font_clut_y_offset_immediate = 0x7f;
    FntLoad(0x280, 0);

    while (!loaded) {
        data = file_load_into_heap_checked(DISC_SECTOR_BATTLE_EFC_FNT_TIM, 0x8800);
        if (data != 0) {
            if (OpenTIM(data) == 0 && ReadTIM(&image) != 0) {
                LoadTPage(image.paddr, 0, 0, 0x280, 0, image.prect->w * 4, image.prect->h);
                loaded = 1;
                LoadClut2(image.caddr, 0x280, 0x7f);
            }
            heap_release_block(data);
        } else {
            VSync(0);
            file_poll_load(&g_file_cd_state);
        }
    }

    SetDumpFnt(FntOpen(0x88, 0x10, 0x100, 0x100, 0, 0x200));
    DrawSync(0);
    DrawSync(0);
}

/* Set up double-buffered drawing at width x height without restarting the GPU:
 * the second buffer sits 240 lines below the first (a 480-line mode uses one),
 * both draw environments clear to the given color, and the display is masked
 * until the caller turns it on. image_rect is not used. */
static void set_up_display(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;

    SetDispMask(0);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, width, height);

    g_gfx_draw_envs[0].dtd = g_gfx_draw_envs[1].dtd = 0;
    g_gfx_draw_envs[0].isbg = g_gfx_draw_envs[1].isbg = 1;
    setRGB0(&g_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_gfx_draw_envs[1], red, green, blue);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
}

/* Present one frame of the Squaresoft logo fade: swap the display buffers after VSync, upload the
 * 180x16 logo at (70, 112) of the new draw buffer and draw the caller's fade list over it. Returns
 * VSync's result. */
static int draw_squaresoft_logo(u32* otag, u32* image) {
    RECT destination;
    int elapsed;

    g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);

    destination.x = 70;
    destination.y = g_gfx_draw_envs[g_gfx_screen_polarity].clip.y + 112;
    destination.w = 180;
    destination.h = 16;
    LoadImage(&destination, image);

    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Fade in the Squaresoft logo from the image linked into the executable
 * (g_boot_squaresoft_logo_image_words).
 *
 * A full-screen subtractive quad steps from 248 to 0 in 32 frames while
 * draw_squaresoft_logo uploads the logo each frame. The logo stays on
 * screen while the caller loads its files; fade_out_squaresoft_logo
 * removes it. */
static void fade_in_squaresoft_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    /* Initialized here, not in a for loop: the bytes need the while loop over it. */
    s16 intensity = 248;
    int frame;
    int buffer;

    set_up_display(320, 240, 512, 0, 0, 0);

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 256, 320, 256);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    for (frame = 0; frame < 15; frame++) {
        VSync(0);
    }

    while (intensity >= 0) {
        buffer = g_gfx_screen_polarity;
        ClearOTag(otags[buffer], 2);
        /* Reversed indexing preserves the retail compiler's addu operand order. */
        setRGB0(&g_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(otags[buffer], &polygons[g_gfx_screen_polarity]);
        AddPrim(otags[buffer], &draw_modes[g_gfx_screen_polarity]);
        draw_squaresoft_logo(otags[buffer], g_boot_squaresoft_logo_image_words);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}

/* Fade out the Squaresoft logo that fade_in_squaresoft_logo left
 * on screen: the subtractive quad steps from 0 to 255 in 32 frames, then both
 * display buffers are cleared. */
static void fade_out_squaresoft_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* otag_base;
    /* Initialized here, not in a for loop: the bytes need the while loop over it. */
    s16 intensity = 0;
    int buffer;

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 256, 320, 256);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];
    /* The bytes need this base pointer; indexing otags[buffer] is 12 bytes shorter. */
    otag_base = otags[0];

    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        buffer = g_gfx_screen_polarity;
        ClearOTag(otag_base + buffer * 2, 2);
        setRGB0(&g_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(otag_base + buffer * 2, &polygons[g_gfx_screen_polarity]);
        AddPrim(otag_base + buffer * 2, &draw_modes[g_gfx_screen_polarity]);
        draw_squaresoft_logo(otag_base + buffer * 2, g_boot_squaresoft_logo_image_words);
        intensity += 8;
    }

    DrawSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
}

/* Present one frame of the SCEAP.DAT fade: swap the display buffers after VSync,
 * upload the 320x32 strip at line 104 of the new draw buffer and draw the
 * caller's fade list over it. Returns VSync's result. */
static int draw_sceap_logo(u32* otag, u32* image) {
    RECT destination;
    int elapsed;

    g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);

    destination.x = 0;
    destination.y = g_gfx_draw_envs[g_gfx_screen_polarity].clip.y + 104;
    destination.w = 320;
    destination.h = 32;
    LoadImage(&destination, image);

    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Show the publisher's logo strip at power-on.
 *
 * Loads SCEAP.DAT (sector 198, a 320x32 16-bit image) to the high overlay address
 * and switches to a 320x240 display. A full-screen subtractive black quad fades
 * the strip in over 32 frames, it stays for 180 frames, and the same quad fades
 * it out over 32; each frame uploads the image again through
 * draw_sceap_logo. Both display buffers are cleared before and after. */
static void show_sceap_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity;
    int frame;

    set_up_display(320, 240, 512, 0, 0, 0);
    file_load_into_buffer_checked(DISC_SECTOR_SCEAP_DAT, 0x5000, g_heap_high_overlay_load_address);

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 240, 320, 240);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 320;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    for (frame = 0; frame < 15; frame++) {
        VSync(0);
    }

    for (intensity = 248; intensity >= 0; intensity -= 8) {
        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        draw_sceap_logo(current_otag, (u32*)g_heap_high_overlay_load_address);
    }

    for (frame = 0; frame < 180; frame++) {
        VSync(0);
    }

    for (intensity = 0; intensity < 256; intensity += 8) {
        if (intensity >= 248) {
            intensity = 255;
        }

        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        draw_sceap_logo(current_otag, (u32*)g_heap_high_overlay_load_address);
    }

    DrawSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 320;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
}

/* Restart the GPU and set up double-buffered drawing at width x height with the
 * second buffer 240 lines below the first (an interlaced 480-line mode uses one).
 *
 * Unlike set_up_display this also calls ResetGraph(0) and InitGeom, clears
 * the whole 1024x512 VRAM, loads the debug font at (640, 0) and opens a print
 * stream, and leaves background clearing off. The display stays masked. */
static void reset_display(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;

    SetDispMask(0);
    ResetGraph(0);
    InitGeom();
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, width, height);

    g_gfx_draw_envs[0].dtd = g_gfx_draw_envs[1].dtd = 0;
    g_gfx_draw_envs[0].isbg = g_gfx_draw_envs[1].isbg = 0;
    setRGB0(&g_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_gfx_draw_envs[1], red, green, blue);

    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    FntLoad(640, 0);
    SetDumpFnt(FntOpen(40, 16, 512, 256, 0, 512));

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
}

/* startup */

static s32 s_session_initialized = 0;

/* Set the RAM size to 2 MB, initialize the CD library with debug output off, pause the drive and
 * mark the shared file descriptor idle. */
static void init_memory_and_cd(void) {
    SetMem(2);
    CdInit();
    CdSetDebug(0);
    file_cd_pause(&g_file_cd_state);
    g_file_cd_state.state = FILE_LOAD_STATE_IDLE;
}

/* BATTLE/ZODIAC.BIN is a headerless image, not an executable overlay.
 * Sector 0xec61 supplies 0x10000 bytes: 128 * 256 16-bit VRAM transfer
 * values at (640, 0), subsequently rendered as 8-bit indexed texture data. */
void gfx_load_zodiac_and_frame_textures(void) {
    RECT destination = { 640, 0, 128, 256 };
    u32* image = file_load_into_heap_checked(DISC_SECTOR_BATTLE_ZODIAC_BIN, 0x10000);

    LoadImage(&destination, image);
    DrawSync(0);
    heap_free(image);
    gfx_load_frame_texture();
}

/* Power-on initialization: install the VSync, DrawSync and CD callbacks, start
 * the GPU, pad, SPU and CD, show the two logos, open the memory-card events and
 * load the shared sounds and the zodiac frame while the Squaresoft logo is up.
 *
 * rand is seeded with 1, and g_session_initialized is cleared so the first
 * boot_reset_session does not repeat the hardware restart. */
void boot_run_startup(void) {
    heap_clear_game_table();
    ResetCallback();

    g_save_play_time_hours = 0;
    g_save_play_time_minutes = 0;
    g_save_play_time_seconds = 0;
    g_save_play_time_frames = 0;

    VSyncCallback(system_vsync_callback);
    DrawSyncCallback(system_draw_sync_callback);
    CdReadyCallback(file_cd_ready_callback);
    CdReadCallback(file_cd_read_callback);

    ResetGraph(0);
    SetGraphDebug(0);
    PadInit(0);
    SpuInit();
    init_memory_and_cd();
    reset_display(256, 240, 512, 0, 0, 0);
    show_sceap_logo();
    fade_in_squaresoft_logo();
    memcard_init_events();
    sound_init();
    gfx_load_zodiac_and_frame_textures();
    srand(1);
    fade_out_squaresoft_logo();
    s_session_initialized = 0;
}

/* Prepare a fresh session before the opening runs: reset the 32-bit save values and
 * options, rebuild the status check sets and restore the save data and item order
 * tables.
 *
 * On the first pass after power-on only that is done, because
 * boot_run_startup has just shown the logos. On every later pass (a soft
 * reset or the end of the game) it first frees the game heap, unloads the
 * scenario music, zeroes the play time, restarts the GPU, SPU and CD, and shows
 * the Squaresoft logo again for at least 60 frames while the shared sounds and
 * the zodiac frame reload. */
void boot_reset_session(void) {
    int frame;

    if (s_session_initialized != 0) {
        heap_clear_game_table();
        music_unload_all();
        g_save_play_time_hours = 0;
        g_save_play_time_minutes = 0;
        g_save_play_time_seconds = 0;
        g_save_play_time_frames = 0;
        ResetGraph(1);
        SetGraphDebug(0);
        SpuInitHot();
        init_memory_and_cd();
        reset_display(256, 240, 512, 0, 0, 0);
        fade_in_squaresoft_logo();
        sound_init();
        gfx_load_zodiac_and_frame_textures();
    }

    save_init_state();
    status_init_check_data();
    save_init_data_tables();
    menu_init_sort_tables();

    if (s_session_initialized != 0) {
        for (frame = 0; frame < 60; frame++) {
            VSync(0);
        }
        fade_out_squaresoft_logo();
    }

    s_session_initialized = 1;
}

/* encounters */

/* Start loading the ENTD file that holds g_encounter_current_id into a new
 * 0x14000-byte heap block (g_encounter_set). The four files cover 128 encounters
 * each.
 *
 * Returns 1 once the read is queued, and also for encounter 0, which loads nothing.
 * Returns 0 with the block freed when the loader is busy; the caller retries. */
int encounter_start_file_load(void) {
    int lba;
    entd_file_t* entd_set;

    if (g_encounter_current_id != 0) {
        /* Each branch makes its own allocation: one shared call is 56 bytes shorter. */
        if (g_encounter_current_id < ENTD2_FIRST_ENCOUNTER) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD1_ENT;
        } else if (g_encounter_current_id < ENTD3_FIRST_ENCOUNTER) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD2_ENT;
        } else if (g_encounter_current_id < ENTD4_FIRST_ENCOUNTER) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD3_ENT;
        } else {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD4_ENT;
        }

        if (file_request_read_bytes(lba, sizeof(entd_file_t), entd_set) != 0) {
            heap_release_block(g_encounter_set);
            return 0;
        }
    }
    return 1;
}

/* Return the selected encounter in the loaded ENTD file: -1 when there
 * is no selected encounter, NULL while the file is still loading. Encounter ids 0-127
 * index the file directly; each further range of 128 ids (the last one
 * open-ended) indexes it from its own base. */
const entd_encounter_t* encounter_get_current(void) {
    /* The bytes need the id read through this pointer; direct reads are 8 bytes shorter. */
    int* encounter_id = &g_encounter_current_id;
    int id;

    if (*encounter_id == 0) {
        return (const entd_encounter_t*)-1;
    }
    if (file_get_load_state() == 0) {
        id = *encounter_id;
        if (id < ENTD2_FIRST_ENCOUNTER) {
            return &g_encounter_set->encounters[id];
        }
        return &g_encounter_set->encounters[id < ENTD3_FIRST_ENCOUNTER ? id - ENTD2_FIRST_ENCOUNTER
                : id < ENTD4_FIRST_ENCOUNTER                           ? id - ENTD3_FIRST_ENCOUNTER
                                                                       : id - ENTD4_FIRST_ENCOUNTER];
    }
    return 0;
}

/* Rebuild the party roster from an ENTD encounter: load its file (blocking),
 * empty the roster, save its 16 unit slots through party_save_encounter_unit,
 * and free the file. */
void encounter_init_party(int encounter_id) {
    const entd_encounter_t* entd;
    int unit_id;

    g_encounter_current_id = encounter_id;
    while (encounter_start_file_load() == 0) {
        VSync(0);
        file_poll_load(&g_file_cd_state);
    }

    for (;;) {
        entd = encounter_get_current();
        if (entd == (const entd_encounter_t*)-1) {
            break;
        }
        if (entd != 0) {
            party_clear();
            for (unit_id = 0; unit_id < ENTD_UNITS_PER_ENCOUNTER; unit_id++) {
                party_save_encounter_unit(entd, unit_id, 0, 1);
            }
            heap_release_block(g_encounter_set);
            break;
        }
        VSync(0);
        file_poll_load(&g_file_cd_state);
    }
}

/* new game */

/* Empty the inventory, then stock the new-game consumables: five Potions, two each of Hi-Potion,
 * Eye Drop and Phoenix Down, and one of each other basic restorative. */
void item_init_new_game_inventory(void) {
    s32 remaining;
    u8* quantity;

    /* The bytes need both counters; an indexed loop is 4 bytes shorter. */
    for (remaining = ITEM_ID_COUNT - 1, quantity = &g_item_quantities[ITEM_ID_COUNT - 1]; remaining >= 0;
        remaining--, quantity--) {
        *quantity = 0;
    }

    g_item_quantities[ITEM_ID_POTION] = 5;
    g_item_quantities[ITEM_ID_HI_POTION] = 2;
    g_item_quantities[ITEM_ID_X_POTION] = 1;
    g_item_quantities[ITEM_ID_ETHER] = 1;
    g_item_quantities[ITEM_ID_HI_ETHER] = 1;
    g_item_quantities[ITEM_ID_ELIXIR] = 1;
    g_item_quantities[ITEM_ID_ANTIDOTE] = 1;
    g_item_quantities[ITEM_ID_EYE_DROP] = 2;
    g_item_quantities[ITEM_ID_ECHO_GRASS] = 1;
    g_item_quantities[ITEM_ID_MAIDENS_KISS] = 1;
    g_item_quantities[ITEM_ID_SOFT] = 1;
    g_item_quantities[ITEM_ID_HOLY_WATER] = 1;
    g_item_quantities[ITEM_ID_REMEDY] = 1;
    g_item_quantities[ITEM_ID_PHOENIX_DOWN] = 2;
}

enum { STARTING_ENCOUNTER_ID = 254, STARTING_WAR_FUNDS = 2000 };

/* Start a new roster: empty it, then generate Ramza alone (mode 0) or load the
 * party from ENTD encounter 254 (mode 2); any other mode leaves it empty. The
 * inventory is reset to the new-game stock and the war funds to 2000. */
void party_init_new_game(int mode) {
    menu_screen_init_world_text();
    party_clear();

    if (mode == PARTY_NEW_GAME_RAMZA) {
        party_unit_generate(UNIT_GENERATION_TYPE_RAMZA);
    } else if (mode == PARTY_NEW_GAME_ENCOUNTER) {
        encounter_init_party(STARTING_ENCOUNTER_ID);
    }

    item_init_new_game_inventory();
    script_variable_set(SCRIPT_VAR_WAR_FUNDS, STARTING_WAR_FUNDS);
}

/* vectors */

/* Retail calls these out-of-line setters even though PsyQ provides the same stores. */

/* Store x, y and z in an SVECTOR, truncated to 16 bits. */
void vector_set_short(SVECTOR* vector, s32 x, s32 y, s32 z) {
    setVector(vector, x, y, z);
}

/* Store x, y and z in a VECTOR. */
void vector_set_long(VECTOR* vector, s32 x, s32 y, s32 z) {
    setVector(vector, x, y, z);
}

/* new-game caption */

/* A second copy of set_up_display in the original, for the Orbonne
 * caption fades. Set up double-buffered drawing at width x height without
 * restarting the GPU: the second buffer sits 240 lines below the first (a
 * 480-line mode uses one), both draw environments clear to the given color,
 * and the display is masked until the caller turns it on. image_rect is not
 * used. */
static void set_up_caption_display(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;

    SetDispMask(0);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, width, height);

    g_gfx_draw_envs[0].dtd = g_gfx_draw_envs[1].dtd = 0;
    g_gfx_draw_envs[0].isbg = g_gfx_draw_envs[1].isbg = 1;
    setRGB0(&g_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_gfx_draw_envs[1], red, green, blue);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
}

/* Present one frame for the Orbonne caption fades: swap the display buffers after VSync, draw otag
 * and flush the debug print stream. Returns VSync's result. */
static int present_caption_frame(u32* otag) {
    /* Unused local retains the retail stack frame. */
    RECT matching_stack_space;
    int elapsed;

    g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Fade in a 256x24 caption strip on a black 256x240 screen.
 *
 * The strip is the texture at VRAM (896, 256) with its CLUT at (896, 287), drawn
 * at y 116. A full-screen subtractive quad steps from 248 to 0 in 32 frames, each
 * presented with present_caption_frame. No function in the tree calls this
 * routine or fade_out_caption. */
static void fade_in_caption(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    /* Initialized here, not in a for loop: the bytes need the while loop over it. */
    s16 intensity = 248;

    set_up_caption_display(256, 240, 512, 0, 0, 0);

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    setPolyFT4(&title_polygons[0]);
    setRGB0(&title_polygons[0], 128, 128, 128);
    setXY4(&title_polygons[0], 0, 116, 255, 116, 0, 140, 255, 140);
    setUV4(&title_polygons[0], 0, 0, 255, 0, 0, 24, 255, 24);
    SetSemiTrans(&title_polygons[0], 0);
    title_polygons[0].tpage = GetTPage(0, 0, 896, 256);
    title_polygons[0].clut = GetClut(896, 287);
    title_polygons[1] = title_polygons[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    while (intensity >= 0) {
        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&fade_polygons[g_gfx_screen_polarity], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_gfx_screen_polarity]);
        present_caption_frame(current_otag);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}

/* Fade out the caption strip fade_in_caption shows: the
 * subtractive quad steps from 0 to 255 in 32 frames over the same strip, then
 * both display buffers are cleared. */
static void fade_out_caption(void) {
    POLY_F4 fade_polygons[2];
    DR_MODE draw_modes[2];
    POLY_FT4 title_polygons[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    /* Initialized here, not in a for loop: the bytes need the while loop over it. */
    s16 intensity = 0;

    SetPolyF4(&fade_polygons[0]);
    setRGB0(&fade_polygons[0], 0, 0, 0);
    setXY4(&fade_polygons[0], 0, 0, 256, 0, 0, 256, 256, 256);
    SetSemiTrans(&fade_polygons[0], 1);
    fade_polygons[1] = fade_polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    setPolyFT4(&title_polygons[0]);
    setRGB0(&title_polygons[0], 128, 128, 128);
    setXY4(&title_polygons[0], 0, 116, 255, 116, 0, 140, 255, 140);
    setUV4(&title_polygons[0], 0, 0, 255, 0, 0, 24, 255, 24);
    SetSemiTrans(&title_polygons[0], 0);
    title_polygons[0].tpage = GetTPage(0, 0, 896, 256);
    title_polygons[0].clut = GetClut(896, 287);
    title_polygons[1] = title_polygons[0];

    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        current_otag = otags[g_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&fade_polygons[g_gfx_screen_polarity], intensity, intensity, intensity);
        AddPrim(current_otag, &fade_polygons[g_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_gfx_screen_polarity]);
        AddPrim(current_otag, &title_polygons[g_gfx_screen_polarity]);
        present_caption_frame(current_otag);
        intensity += 8;
    }

    DrawSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
}
