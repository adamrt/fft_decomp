/* SCUS_942.21 0x800212ec..0x80021c5b. */
#include "psx/libcd.h"

/* Signed status stores retain native pointer reloads and store scheduling. */
void StCdInterrupt(void) {
    /* Volatile preserves the native 16-bit stack stores and first-response reload. */
    volatile u16 response_status[3];
    CdlLOC sector_position;
    u8 response[8];
    u8* position_byte;
    u32 index;
    u16 frame_count;
    s16 ring_status;
    u32 dma_control;
    s32 emulation_base;
    if (g_psyq_cd_stream_frame_dma_pending == 1)
        return;
    if (g_psyq_cd_stream_mode && (*g_psyq_cd_stream_mdec_out_dma_control_register & 0x01000000)) {
        g_psyq_cd_stream_interrupt_pending = 1;
        if (g_psyq_cd_stream_emulation_base)
            g_psyq_cd_stream_emulation_sector_index++;
        g_psyq_cd_stream_diagnostic_code = 1;
        return;
    }
    if (CdReady(1, response) == CdlDiskError)
        return;
    response_status[1] = response[0];
    response_status[2] = response[1];
    if (response_status[1] & 4) {
        g_psyq_cd_stream_diagnostic_code = 3;
        return;
    }
    g_psyq_cd_stream_current_header = &g_psyq_cd_stream_ring[g_psyq_cd_stream_write_index];
    /* The addressed 16-bit field preserves the status load's native delay-slot scheduling. */
    if (*&g_psyq_cd_stream_current_header->id) {
        if (g_psyq_cd_stream_emulation_base)
            g_psyq_cd_stream_emulation_sector_index++;
        g_psyq_cd_stream_diagnostic_code = 4;
        return;
    }
    *g_psyq_cd_stream_index_register = 0;
    *g_psyq_cd_stream_request_register = 0;
    *g_psyq_cd_stream_index_register = 0;
    *g_psyq_cd_stream_request_register = 0x80;
    *g_psyq_cd_stream_access_delay_register = 0x20943;
    *g_psyq_cd_stream_common_delay_register = 0x1323;
    if (!g_psyq_cd_stream_skip_sector_position) {
        position_byte = (u8*)&sector_position;
        do {
            *position_byte++ = *g_psyq_cd_stream_data_register;
        } while (position_byte < (u8*)(&sector_position + 1));
        for (index = 0; index < PSYQ_CD_STREAM_HEADER_WORDS; index++)
            *g_psyq_cd_stream_data_register;
    }
    dma_control = 0x11000000;
    if ((emulation_base = g_psyq_cd_stream_emulation_base) != 0)
        mem2mem((u32*)g_psyq_cd_stream_current_header,
            (u32*)((g_psyq_cd_stream_emulation_sector_index << 11) + emulation_base), PSYQ_CD_STREAM_HEADER_WORDS, 0);
    else
        dma_execute(PSYQ_CD_DMA_CHANNEL, (u32*)g_psyq_cd_stream_current_header, 0, PSYQ_CD_STREAM_HEADER_WORDS,
            dma_control, 0, 0);
    while (*g_psyq_cd_stream_dma_control_register & 0x01000000) { }
    g_psyq_cd_stream_current_header->loc = sector_position;
    *g_psyq_cd_stream_access_delay_register = 0x20843;
    *g_psyq_cd_stream_common_delay_register = 0x1325;
    if (g_psyq_cd_stream_start_mask == 1) {
        u32 required_frame = g_psyq_cd_stream_start_frame;
        if (required_frame) {
            /* The original comparison reads the low 16 bits of the 32-bit frame field. */
            frame_count = g_psyq_cd_stream_current_header->frameCount;
            if (required_frame != frame_count) {
                *(s16*)&g_psyq_cd_stream_current_header->id = StFREE;
                if (g_psyq_cd_stream_emulation_base)
                    g_psyq_cd_stream_emulation_sector_index++;
                return;
            }
            g_psyq_cd_stream_start_mask = 0;
        }
    }
    if (*(volatile u16*)&g_psyq_cd_stream_current_header->id != PSYQ_CD_STREAM_HEADER_MAGIC
        || ((g_psyq_cd_stream_current_header->type >> 10) & 0x1f) != g_psyq_cd_stream_current_channel) {
        if (g_psyq_cd_stream_emulation_base)
            g_psyq_cd_stream_emulation_sector_index = 0;
        else
            *(volatile u16*)&g_psyq_cd_stream_current_header->id;
        g_psyq_cd_stream_diagnostic_code = 5;
        *(volatile s16*)&g_psyq_cd_stream_current_header->id = StFREE;
        return;
    }
    if (g_psyq_cd_stream_expected_sector_index != g_psyq_cd_stream_current_header->secCount
        || (g_psyq_cd_stream_frame_count
            && g_psyq_cd_stream_frame_count != (frame_count = g_psyq_cd_stream_current_header->frameCount))) {
        g_psyq_cd_stream_frame_count = 0;
        g_psyq_cd_stream_expected_sector_index = 0;
        init_ring_status(
            g_psyq_cd_stream_frame_start_index, g_psyq_cd_stream_write_index - g_psyq_cd_stream_frame_start_index);
        g_psyq_cd_stream_write_index = g_psyq_cd_stream_frame_start_index;
        *(s16*)&g_psyq_cd_stream_current_header->id = StFREE;
        if (g_psyq_cd_stream_emulation_base)
            g_psyq_cd_stream_emulation_sector_index++;
        g_psyq_cd_stream_diagnostic_code = 6;
        return;
    }
    if (g_psyq_cd_stream_current_header->secCount == 0) {
        frame_count = g_psyq_cd_stream_current_header->frameCount;
        g_psyq_cd_stream_expected_sector_index = 0;
        g_psyq_cd_stream_frame_count = frame_count;
        if (g_psyq_cd_stream_end_frame && g_psyq_cd_stream_frame_count >= g_psyq_cd_stream_end_frame) {
            g_psyq_cd_stream_frame_count = 0;
            g_psyq_cd_stream_expected_sector_index = 0;
            init_ring_status(
                g_psyq_cd_stream_frame_start_index, g_psyq_cd_stream_write_index - g_psyq_cd_stream_frame_start_index);
            g_psyq_cd_stream_write_index = g_psyq_cd_stream_frame_start_index;
            *(s16*)&g_psyq_cd_stream_current_header->id = StFREE;
            g_psyq_cd_stream_start_mask = 1;
            if (g_psyq_cd_stream_end_callback)
                g_psyq_cd_stream_end_callback();
            if (g_psyq_cd_stream_emulation_base)
                g_psyq_cd_stream_emulation_sector_index++;
            g_psyq_cd_stream_diagnostic_code = 7;
            return;
        }
        if ((g_psyq_cd_stream_ring_sectors - g_psyq_cd_stream_write_index - 1)
            < g_psyq_cd_stream_current_header->nSectors) {
            if (!g_psyq_cd_stream_end_frame) {
                *(s16*)&g_psyq_cd_stream_current_header->id = StREWIND;
                g_psyq_cd_stream_start_mask = 1;
                if (g_psyq_cd_stream_end_callback)
                    g_psyq_cd_stream_end_callback();
                if (g_psyq_cd_stream_emulation_base)
                    g_psyq_cd_stream_emulation_sector_index++;
                g_psyq_cd_stream_diagnostic_code = 8;
                return;
            }
            ring_status = g_psyq_cd_stream_ring[0].id;
            if (ring_status) {
                *(s16*)&g_psyq_cd_stream_current_header->id = StFREE;
                if (g_psyq_cd_stream_emulation_base)
                    g_psyq_cd_stream_emulation_sector_index++;
                g_psyq_cd_stream_diagnostic_code = 9;
                return;
            }
            *(s16*)&g_psyq_cd_stream_current_header->id = StREWIND;
            {
                u32* destination = (u32*)g_psyq_cd_stream_ring;
                u32* source = (u32*)g_psyq_cd_stream_current_header;
                g_psyq_cd_stream_write_index = 0;
                for (index = 0; index < PSYQ_CD_STREAM_HEADER_WORDS; index++)
                    *destination++ = *source++;
            }
            g_psyq_cd_stream_current_header = g_psyq_cd_stream_ring;
        }
        g_psyq_cd_stream_frame_start_index = g_psyq_cd_stream_write_index;
    }
    g_psyq_cd_stream_diagnostic_code = 10;
    g_psyq_cd_stream_expected_sector_index++;
    g_psyq_cd_stream_payload = (u32*)((u8*)(g_psyq_cd_stream_ring + g_psyq_cd_stream_ring_sectors)
        + g_psyq_cd_stream_write_index * PSYQ_CD_STREAM_PAYLOAD_BYTES);
    dma_control = 0x11000000;
    if (g_psyq_cd_stream_mode) {
        *g_psyq_cd_stream_access_delay_register = 0x20943;
        *g_psyq_cd_stream_common_delay_register = 0x1323;
    } else {
        *g_psyq_cd_stream_access_delay_register = 0x21020843;
        dma_control = 0x11400100;
    }
    if (g_psyq_cd_stream_current_header->nSectors - 1 == g_psyq_cd_stream_current_header->secCount) {
        g_psyq_cd_stream_frame_dma_pending = 1;
        if ((emulation_base = g_psyq_cd_stream_emulation_base) != 0) {
            mem2mem(g_psyq_cd_stream_payload,
                (u32*)((g_psyq_cd_stream_emulation_sector_index << 11) + emulation_base + 32),
                PSYQ_CD_STREAM_PAYLOAD_WORDS, 1);
            g_psyq_cd_stream_emulation_sector_index++;
        } else {
            dma_execute(
                PSYQ_CD_DMA_CHANNEL, g_psyq_cd_stream_payload, 0, PSYQ_CD_STREAM_PAYLOAD_WORDS, dma_control, 1, 0);
        }
        g_psyq_cd_stream_expected_sector_index = 0;
        g_psyq_cd_stream_frame_count = 0;
        g_psyq_cd_stream_current_channel = g_psyq_cd_stream_requested_channel;
    } else {
        if ((emulation_base = g_psyq_cd_stream_emulation_base) != 0) {
            mem2mem(g_psyq_cd_stream_payload,
                (u32*)((g_psyq_cd_stream_emulation_sector_index << 11) + emulation_base + 32),
                PSYQ_CD_STREAM_PAYLOAD_WORDS, 0);
            g_psyq_cd_stream_emulation_sector_index++;
        } else {
            dma_execute(
                PSYQ_CD_DMA_CHANNEL, g_psyq_cd_stream_payload, 0, PSYQ_CD_STREAM_PAYLOAD_WORDS, dma_control, 0, 0);
        }
    }
    *g_psyq_cd_stream_common_delay_register = 0x1325;
    g_psyq_cd_stream_current_header->id = StBUSY;
    g_psyq_cd_stream_write_index++;
    if (g_psyq_cd_stream_emulation_base && g_psyq_cd_stream_frame_dma_pending)
        data_ready_callback();
}
