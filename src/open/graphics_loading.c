/* Load OPEN's title, prologue and ending images from OPNTEX.BIN and OPNBK.BIN.
 * OPNBK images share one asynchronous CD request and a cached image index. */

#include "fft/ending.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/prologue.h"
#include "fft/title.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Seven OPNBK images use eight sector boundaries. */
static const s32 s_opnbk_sector_offsets[8] = { 0, 75, 135, 172, 209, 246, 283, 403 };

/* Load the title screen's graphics: upload the three TIMs at sectors 0, 0x11 and
 * 0x22 of OPNTEX.BIN, then read OPNBK image 0 as the frame image. */
void title_graphics_load(void) {
    file_load_descriptor_t* header = &g_file_cd_state;

    file_pending_read_request(header, DISC_SECTOR_OPEN_OPNTEX_BIN, 0x11, g_file_destination);
    file_pending_wait();
    gfx_load_opntex(g_file_destination);

    file_pending_read_request(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x11, 0x11, g_file_destination);
    file_pending_wait();
    gfx_load_opntex(g_file_destination);

    file_pending_read_request(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x22, 0x21, g_file_destination);
    file_pending_wait();
    gfx_load_opntex(g_file_destination);

    file_request_openbk_image(0, g_file_destination);
    file_pending_wait();
}

/* Load the ending's graphics from OPNTEX.BIN and OPNBK.BIN: the TIM at sector
 * 0x65 (the script's glyph page, uploaded at once), OPNBK image 6 (uploaded as
 * the 512x240 picture at VRAM (512, 256)) and the 16 OPNTEX frames from sector
 * 0x76, which stay at g_gfx_opntex_data for the script to upload. */
void ending_graphics_load(void) {
    file_load_descriptor_t* header = &g_file_cd_state;
    RECT frame;

    file_pending_read_request(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x65, 0x11, g_file_destination);
    file_pending_wait();
    gfx_load_opntex(g_file_destination);

    file_request_openbk_image(6, g_gfx_opntex_data);
    file_pending_wait();

    frame.x = 0x200;
    frame.y = 0x100;
    frame.w = 0x200;
    frame.h = 0xf0;
    LoadImage(&frame, g_gfx_opntex_data);
    DrawSync(0);

    file_pending_read_request(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x76, 0x170, g_gfx_opntex_data);
    file_pending_wait();
}

/* Upload the two TIMs at sectors 0x43 and 0x54 of OPNTEX.BIN: the text pages at
 * VRAM (256, 256) and (320, 256) that the new-game prologue draws its lines from.
 * The only caller is prologue_start. */
void prologue_text_pages_load(void) {
    file_load_descriptor_t* header = &g_file_cd_state;

    file_pending_read_request(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x43, 0x11, g_file_destination);
    file_pending_wait();
    gfx_load_opntex(g_file_destination);

    file_pending_read_request(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x54, 0x11, g_file_destination);
    file_pending_wait();
    gfx_load_opntex(g_file_destination);
}

/* Start reading image image_id of OPNBK.BIN into destination, unless it is the
 * image read last (g_file_current_openbk_image_id). The read is
 * asynchronous; only a read already pending is waited for.
 *
 * The end-offset table is the start-offset table one entry further on. By their
 * sector counts images 0 and 1 are 320x240 and 256x240 frame images, 2 to 5 are
 * 210x180 pictures and 6 is the 512x240 ending picture. */
void file_request_openbk_image(s32 image_id, void* destination) {
    s32 start_offset;

    if (image_id == g_file_current_openbk_image_id) {
        return;
    }

    g_file_current_openbk_image_id = image_id;
    start_offset = s_opnbk_sector_offsets[image_id];
    file_pending_read_request(&g_file_cd_state, DISC_SECTOR_OPEN_OPNBK_BIN + start_offset,
        s_opnbk_sector_offsets[image_id + 1] - start_offset, destination);
}

/* Poll the pending CD command: 1 when it has completed, -1 on a disc error and 0
 * while it runs. ending_update_xa_audio adds the result to its step, so
 * completion advances the sequence and an error repeats the previous step. */
s32 file_get_cd_sync_state_delta(void) {
    s32 status = CdSync(1, 0);

    if (status == CdlComplete) {
        return 1;
    }

    if (status == CdlDiskError) {
        return -1;
    }
    return 0;
}

/* Upload an OPNTEX image, preceded by its CLUT when flag bit 3 is set.
 *
 * The second 32-bit value holds the flags. Each block is a byte length (including itself), the
 * VRAM position packed as y << 16 | x, the size packed as h << 16 | w, then
 * the pixel data. The CLUT rectangle is flattened to one row of w * h
 * entries. */
void gfx_load_opntex(u32* tim) {
    RECT rect;
    s32 offset;
    /* Unused; the target's frame is 16 bytes larger than rect needs. */
    s32 stack_padding[4];

    /* Packed 32-bit RECT reads preserve the retail lw/lhu mix. */
    if (tim[1] & 8) {
        offset = (tim[2] >> 2) + 2;
        rect.x = tim[3];
        rect.y = tim[3] >> 16;
        rect.w = tim[4];
        rect.h = tim[4] >> 16;
        rect.w = rect.w * rect.h;
        rect.h = 1;
        LoadImage(&rect, tim + 5);
    } else {
        offset = 2;
    }
    rect.x = tim[offset + 1];
    rect.y = tim[offset + 1] >> 16;
    rect.w = tim[offset + 2];
    rect.h = tim[offset + 2] >> 16;
    LoadImage(&rect, &tim[offset + 3]);
    DrawSync(0);
}
