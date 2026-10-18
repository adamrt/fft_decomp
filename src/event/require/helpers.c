/* A 16-frame thread stop, the bio variable by unit class, bonus image loading, frame waits with skip and a 31-frame RGB
 * fade. */

#include "fft/after_battle.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu_input.h"
#include "fft/party.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Three unit ids whose presence selects the event variable at +6. */
typedef struct party_class_variable_record {
    u16 unit_ids[3];
    u16 variable;
} party_class_variable_record_t;

extern party_class_variable_record_t g_party_bio_update_records[11];

/* BONUS.BIN stores a 256x200 4bpp image, followed by eight 16-color palette slots. */
typedef struct conditions_banner_file {
    u32 pixels[200][32];   /* 0x0000; eight 4bpp pixels per 32-bit value */
    palette_t palettes[8]; /* 0x6400; four palettes per uploaded VRAM row */
    u8 _unused_6500[0x300];
} conditions_banner_file_t;
typedef char conditions_banner_file_size_must_be_26624[sizeof(conditions_banner_file_t) == 26624 ? 1 : -1];

static RECT g_conditions_banner_rect = { 384, 0, 64, 256 };

/* Thread body that waits 16 frames and exits. Nothing references it. */
void thread_stop_after_16_frames(void) {
    thread_wait_frames(0x10);
    thread_exit_current_wrapper();
}

/* Set to `value` the script variable of every record in
 * g_party_bio_update_records that lists `unit_class` among its three
 * identities, a party_bio_state_e. */
void party_set_class_variable(s32 unit_class, s32 value) {
    s32 i;
    s32 j;

    for (i = 0; i < 11; i++) {
        for (j = 0; j < 3; j++) {
            if (unit_class == g_party_bio_update_records[i].unit_ids[j]) {
                break;
            }
        }
        if (j != 3) {
            script_set_variable(g_party_bio_update_records[i].variable, value);
        }
    }
}

/*
 * Load bonus image `image_id` (a 0x6800-byte file, 13 sectors apart from
 * sector 0x16c0) and upload it to VRAM.
 *
 * Waits until 0x8000 bytes can be reserved at the high overlay address, reads
 * the file there on the main stack, uploads the pixels and the palette that
 * follows them, and frees the buffer after one more frame.
 */
void conditions_load_banner_image(s32 image_id) {
    void* allocation;
    conditions_banner_file_t* banner;
    s32 (*read_operation)(s32 sector, u32 size, void* destination);

    do {
        thread_yield();
        allocation = heap_reserve_at(g_heap_high_overlay_load_address, 0x8000);
    } while (allocation != g_heap_high_overlay_load_address);

    read_operation = file_request_read_bytes;
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))read_operation;
    } while (thread_call_on_main_stack(image_id * 13 + DISC_SECTOR_EVENT_BONUS_BIN, sizeof(*banner), allocation) != 0);

    {
        int (*poll_operation)(void) = file_is_still_loading;
        do {
            thread_yield();
            g_thread_call_target = (void (*)(void))poll_operation;
        } while (thread_call_on_main_stack() != 0);
    }

    banner = allocation;
    /* The 256-row upload includes the reserved suffix beyond the 0x6800-byte file. */
    LoadImage(&g_conditions_banner_rect, banner->pixels[0]);
    LoadImage(&g_conditions_banner_palette_rect, (u32*)banner->palettes);
    thread_yield();
    heap_free(allocation);
}

/* Wait `frames` frames, ending early when Circle is newly pressed. */
void input_wait_frames_or_skip(s32 frames) {
    s32 reserved_stack_words[2];
    s32 i = 0;
    u32* input;

    if (frames > 0) {
        do {
            thread_yield();
            input = script_get_controller_input_pointer(0);
            g_menu_input_controller = input;
            if ((*input & PSX_PAD_CIRCLE) == 0) {
                i++;
                if (i < frames) {
                    continue;
                }
            }
            break;
        } while (1);
    }
}

/* Fade g_after_battle_gfx_fade_rgb linearly from its current value to eight times
 * each target over 31 frames, yielding once per frame. */
void gfx_fade_rgb_31_frames(s32 target_0, s32 target_1, s32 target_2) {
    s32 current[3];
    s32 interpolated[3];
    s32 target[3];
    s32 frame;
    s32 i;

    frame = 0;
    target[0] = target_0 * 8;
    target[1] = target_1 * 8;
    target[2] = target_2 * 8;
    current[0] = g_after_battle_gfx_fade_rgb[0];
    current[1] = g_after_battle_gfx_fade_rgb[1];
    current[2] = g_after_battle_gfx_fade_rgb[2];
    do {
        for (i = 0; i < 3; i++) {
            interpolated[i] = (((target[i] - current[i]) * frame) / 30) + current[i];
            g_after_battle_gfx_fade_rgb[i] = interpolated[i];
        }
        frame++;
        thread_yield();
    } while (frame < 31);
}
