/* Sound-driver setup, wave resources, output controls and SPU transfers. */

#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/libcd.h"
#include "psx/libspu.h"
#include "psx/types.h"

extern SpuReverbAttr g_suzuki_sound_reverb_attr; /* 0x80037008 */

/* Ordinary driver-status view; the transfer wait keeps its measured volatile view. */
extern s16 g_suzuki_sound_driver_flags;

enum { SUZUKI_SOUND_HEAP_BYTE_COUNT = 32768 };

enum {
    SUZUKI_SOUND_SPU_ALLOC_BLOCK_CAPACITY = 6,
    SUZUKI_SOUND_SPU_ALLOC_RECORD_COUNT = SUZUKI_SOUND_SPU_ALLOC_BLOCK_CAPACITY + 1
};

extern u8 g_suzuki_sound_heap_memory[SUZUKI_SOUND_HEAP_BYTE_COUNT]; /* the CPU heap arena */

/* Allocation records and one trailing free-range record. */
extern psyq_spu_heap_record_t g_suzuki_sound_spu_malloc_table[SUZUKI_SOUND_SPU_ALLOC_RECORD_COUNT];
typedef char suzuki_spu_allocator_storage_size_must_be_56[sizeof(g_suzuki_sound_spu_malloc_table) == 56 ? 1 : -1];

/* Cleared during driver initialization; their purposes are unknown. */
extern u32 g_suzuki_unknown_80032a34;
extern s16 g_suzuki_unknown_80032a58;

/* Output mixing and transfer state. */
extern CdlATV g_suzuki_sound_cd_mix;           /* 0x80032a3c; written by Put Sound Type */
extern s16 g_suzuki_sound_decoded_data_result; /* last decoded-data read result */
extern s32 g_suzuki_sound_spu_event;           /* OpenEvent handle of suzuki_sound_spu_event_handler */

/* Provisional: retained driver value in main RAM; no reader or SPU transfer of it has been identified. */
extern u16 g_suzuki_sound_unread_value;

/* Starts the Suzuki driver once (status bit 15 marks it initialized).
 *
 * flags seeds the 16-bit status. Sets up SPU RAM allocation and the driver
 * heap, clears the driver lists, creates the SFX music record, installs the
 * root-counter-2 tick and SPU events and the transfer callback, then applies
 * sound type 1, reverb mode 4, CD mixing and the default volumes. */
void suzuki_sound_init(s32 flags) {
    /* Bit 15 (initialized) is tested as a sign, on an `lh` load. */
    if (g_suzuki_sound_driver_flags < 0) {
        return;
    }
    g_suzuki_sound_driver_flags = flags | SUZUKI_SOUND_DRIVER_INITIALIZED;
    SpuInitMalloc(SUZUKI_SOUND_SPU_ALLOC_BLOCK_CAPACITY, (char*)g_suzuki_sound_spu_malloc_table);
    suzuki_sound_init_heap(g_suzuki_sound_heap_memory, sizeof(g_suzuki_sound_heap_memory));
    suzuki_sound_init_stub();
    g_suzuki_smd_random_state = 0x12345678;
    g_suzuki_sound_active_music_list = 0;
    g_suzuki_sound_sfx_music = 0;
    g_suzuki_sound_resource_list = 0;
    g_suzuki_sound_waveset_list = 0;
    g_suzuki_sound_sfx_reserved_voices = 0;
    g_suzuki_sound_sfx_restart_channels = 0;
    g_suzuki_sound_sfx_restart_voices = 0;
    g_suzuki_sound_music_key_off_voices = 0;
    g_suzuki_sound_sfx_key_off_voices = 0;
    g_suzuki_sound_sfx_instrument = 0;
    g_suzuki_unknown_80032a58 = 0;
    g_suzuki_sound_sfx_music = suzuki_sound_init_sfx_music();
    g_suzuki_sound_spu_state.common.mvolmode.left = SPU_VOICE_DIRECT;
    g_suzuki_sound_spu_state.common.mvolmode.right = SPU_VOICE_DIRECT;
    g_suzuki_sound_spu_state.common.mask = SPU_COMMON_MVOLMODEL | SPU_COMMON_MVOLMODER;
    EnterCriticalSection();
    g_suzuki_sound_root_counter_2_event = OpenEvent(RCntCNT2, EvSpINT, EvMdINTR, suzuki_sound_root_counter_2_handler);
    g_suzuki_sound_spu_event = OpenEvent(HwSPU, EvSpTRAP, EvMdINTR, suzuki_sound_spu_event_handler);
    g_suzuki_sound_tick_count = 0;
    g_suzuki_unknown_80032a34 = 0;
    SetRCnt(RCntCNT2, 0x44e8, RCntMdINTR);
    StartRCnt(RCntCNT2);
    EnableEvent(g_suzuki_sound_root_counter_2_event);
    SpuSetTransferCallback(suzuki_sound_spu_transfer_callback);
    g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT;
    ExitCriticalSection();
    suzuki_sound_set_type(SUZUKI_SOUND_TYPE_STEREO);
    g_suzuki_sound_reverb_attr.mode = -1;
    suzuki_sound_set_reverb_mode(SPU_REV_MODE_STUDIO_C, 0, 0, 0);
    suzuki_sound_set_cd_reverb(SPU_OFF, SPU_ON);
    suzuki_sound_set_master_volume(0x3fff, 0);
    suzuki_sound_set_cd_volume(0x6400, 0);
    if (g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_PUT_TYPE) {
        suzuki_sound_put_type(0xc0);
    }
    suzuki_sound_set_sfx_enabled(1);
    g_suzuki_sound_unread_value = 0;
}

/* Shuts the driver down: closes its events, releases every voice with
 * release rate 6 and turns reverb off.
 *
 * The 16-bit status is tested as signed (`lh`); a u16 test loads with `lhu`. */
void suzuki_sound_quit(void) {
    s32 voice;
    s16 status = g_suzuki_sound_driver_flags;

    if (status != 0) {
        EnterCriticalSection();
        CloseEvent(g_suzuki_sound_root_counter_2_event);
        CloseEvent(g_suzuki_sound_spu_event);
        g_suzuki_sound_driver_flags = 0;
        ExitCriticalSection();
        for (voice = 0; voice < 24; voice++) {
            SpuSetVoiceRR(voice, 6);
        }
        SpuSetKey(SPU_OFF, SPU_ALLCH);
        suzuki_sound_set_reverb_mode(0, 0, 0, 0);
        g_suzuki_sound_unread_value = 0;
    }
}

/* Enable the root-counter-2 event that drives the driver tick, unless
 * SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT says it already is. Nothing in the tree calls it. */
void suzuki_sound_enable_root_counter_2_event(void) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT) == 0) {
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT;
        EnableEvent(g_suzuki_sound_root_counter_2_event);
    }
}

/* Disable the root-counter-2 tick event if SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT says it is
 * enabled, and clear that flag. Nothing in the tree calls it. */
void suzuki_sound_disable_root_counter_2_event(void) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT) != 0) {
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        g_suzuki_sound_driver_flags &= ~SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT;
    }
}

/* Uploads a loaded WAVESET.WD to SPU RAM and appends a heap copy of its
 * header to g_suzuki_sound_waveset_list.
 *
 * The first list test reads the head global itself, not through link. */
suzuki_waveset_t* suzuki_sound_load_waveset(suzuki_waveset_t* file) {
    u32 spu_address = suzuki_sound_alloc_spu_ram(file->wave_size);
    suzuki_waveset_t* waveset;
    suzuki_waveset_t** link;

    suzuki_sound_transfer_spu_data(spu_address, (u8*)file + file->wave_offset, file->wave_size,
        SUZUKI_SOUND_SPU_TRANSFER_WAIT | SUZUKI_SOUND_SPU_TRANSFER_WRITE);
    waveset = suzuki_sound_alloc(file->header_size);
    suzuki_sound_copy_memory(waveset, file, file->header_size);
    waveset->spu_address = spu_address;
    link = &g_suzuki_sound_waveset_list;
    if (g_suzuki_sound_waveset_list != 0) {
        do {
            link = &(*link)->next;
        } while (*link != 0);
    }
    *link = waveset;
    waveset->next = 0;
    suzuki_sound_wait_for_spu_transfer(SUZUKI_SOUND_SPU_TRANSFER_WAIT);
    return waveset;
}

/* Frees every loaded waveset whose id is 0x20 or above, keeping the
 * resident low-id sets. */
void suzuki_sound_free_wavesets(void) {
    suzuki_waveset_t* waveset;
    suzuki_waveset_t* node = g_suzuki_sound_waveset_list;

    while (node != 0) {
        waveset = node;
        node = node->next;
        if (waveset->id >= 0x20) {
            suzuki_sound_free_waveset(waveset);
        }
    }
}

/* Unlinks a loaded waveset and frees its SPU RAM and heap header; does
 * nothing if it is not in g_suzuki_sound_waveset_list. */
void suzuki_sound_free_waveset(suzuki_waveset_t* waveset) {
    suzuki_waveset_t* node;
    suzuki_waveset_t* previous = 0;

    node = g_suzuki_sound_waveset_list;
    while (node != 0) {
        if (node == waveset) {
            break;
        }
        previous = node;
        node = node->next;
    }
    if (node != 0) {
        suzuki_sound_free_spu_ram(waveset->spu_address);
        if (previous != 0) {
            previous->next = waveset->next;
        } else {
            g_suzuki_sound_waveset_list = waveset->next;
        }
        suzuki_sound_free(waveset);
    }
}

/* Returns the loaded waveset with the given id, or 0. */
suzuki_waveset_t* suzuki_sound_find_waveset(s16 id) {
    suzuki_waveset_t* waveset = g_suzuki_sound_waveset_list;

    while ((waveset != 0) && (waveset->id != id)) {
        waveset = waveset->next;
    }
    return waveset;
}

/* Append a loaded SED sound bank to the driver's resource list. */
void suzuki_sound_append_resource(suzuki_sound_resource_t* resource) {
    suzuki_sound_resource_t** slot = &g_suzuki_sound_resource_list;
    suzuki_sound_resource_t* node;

    if (g_suzuki_sound_resource_list != 0) {
        do {
            node = *slot;
            slot = &node->next;
        } while (node->next != 0);
    }
    *slot = resource;
    resource->next = 0;
}

/* Unlink a sound bank after stopping the SFX channels that play its sounds.
 *
 * The search and unlink use a copy of the parameter while the call passes the
 * parameter itself, which stays in $a0 (the target loads no argument). The
 * copy sched2 schedules for it also keeps the list-head load after the
 * prologue saves; reorg then deletes that copy as redundant. */
void suzuki_sound_remove_resource(suzuki_sound_resource_t* resource) {
    suzuki_sound_resource_t* prev;
    suzuki_sound_resource_t* node;
    suzuki_sound_resource_t* removed = resource;

    prev = 0;
    node = g_suzuki_sound_resource_list;
    while (node != 0) {
        if (node == removed) {
            break;
        }
        prev = node;
        node = node->next;
    }
    if (node != 0) {
        suzuki_sound_stop_resource_sfx(resource);
        if (prev != 0) {
            prev->next = removed->next;
        } else {
            g_suzuki_sound_resource_list = removed->next;
        }
    }
}

/* Stops every SMD music record and silences every SFX channel. */
void suzuki_sound_stop_all(void) {
    suzuki_smd_stop_marked_music();
    suzuki_sound_stop_all_sfx();
}

/* Selects the output sound type and reapplies the volumes, reverb and CD mix
 * that depend on it.
 *
 * Type 0 (mono) and any value outside 1-3 leave the status type bits clear;
 * 1 (stereo) sets 0x100, 2 (wide) 0x300 and 3 0x500 (no menu offers it).
 * The explicit case 0 makes the target's compare tree (root at case 1). */
void suzuki_sound_set_type(s32 type) {
    suzuki_music_t* music;

    g_suzuki_sound_driver_flags
        &= ~(SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE | SUZUKI_SOUND_DRIVER_STEREO);
    switch (type) {
    case SUZUKI_SOUND_TYPE_MONO:
        break;
    case SUZUKI_SOUND_TYPE_STEREO:
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_STEREO;
        break;
    case SUZUKI_SOUND_TYPE_WIDE:
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_WIDE | SUZUKI_SOUND_DRIVER_STEREO;
        break;
    case SUZUKI_SOUND_TYPE_WIDE_OPPOSITE:
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_STEREO;
        break;
    }
    suzuki_sound_commit_volume_change();
    SpuSetReverbModeParam(&g_suzuki_sound_reverb_attr);
    g_suzuki_sound_reverb_attr.mask = 0;
    for (music = g_suzuki_sound_active_music_list; music != 0; music = music->next) {
        suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
    }
    if (g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_PUT_TYPE) {
        suzuki_sound_put_type(g_suzuki_sound_spu_state.cd_mix_level);
    }
}

/* The sound output setting as SUZUKI_SOUND_TYPE_MONO, _STEREO or _WIDE, read from
 * the driver flags. The opposite-phase wide mode is reported as _WIDE. */
suzuki_sound_type_e suzuki_sound_get_type(void) {
    suzuki_sound_type_e result;

    if (g_suzuki_sound_driver_flags
        & (SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE | SUZUKI_SOUND_DRIVER_STEREO)) {
        result = SUZUKI_SOUND_TYPE_STEREO;
        if (g_suzuki_sound_driver_flags & (SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE)) {
            result = SUZUKI_SOUND_TYPE_WIDE;
        }
    } else {
        result = SUZUKI_SOUND_TYPE_MONO;
    }
    return result;
}

/* Sets the CD audio attenuation for the current sound type;
 * level is 0xc0 at start-up.
 *
 * For stereo and wide (status bits 0x700) each CD channel feeds its own side
 * at level; for mono both channels feed both sides at 160/255 of it. The
 * two direct stores sit in each branch: in the join block CSE would share
 * the val0 address register with the CdMix argument. */
void suzuki_sound_put_type(s16 level) {
    s16 cross;
    CdlATV* mix;

    g_suzuki_sound_spu_state.cd_mix_level = level;
    if (g_suzuki_sound_driver_flags
        & (SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE | SUZUKI_SOUND_DRIVER_STEREO)) {
        cross = 0;
        g_suzuki_sound_cd_mix.val0 = g_suzuki_sound_cd_mix.val2 = level;
    } else {
        cross = level * 160 / 255;
        g_suzuki_sound_cd_mix.val0 = g_suzuki_sound_cd_mix.val2 = cross;
    }
    /* Assign the mix pointer before the crossfeed stores to retain their separate address bases. */
    mix = &g_suzuki_sound_cd_mix;
    g_suzuki_sound_cd_mix.val1 = g_suzuki_sound_cd_mix.val3 = cross;
    CdMix(mix);
}

/* Sets the SPU reverb mode and parameters.
 *
 * Modes 0-9 are Psy-Q reverb types (0 off, 4 studio C, the mode of every
 * SMD on the disc): the reverb is switched off while a changed mode (or mode
 * 0) is programmed with the work-area clear flag 0x100; any mode from 10 up
 * only records depth, delay and feedback. A negative mode selects the mode
 * stored here. That copy keeps the 0x100 flag, which LIBSPU strips from its
 * own, so the two never compare equal and the stored mode is not below 10:
 * a negative mode also only records the three values. */
void suzuki_sound_set_reverb_mode(s32 mode, s16 depth, s32 delay, s32 feedback) {
    SpuReverbAttr current;

    SpuGetReverbModeParam(&current);
    if (mode < 0) {
        if (current.mode == g_suzuki_sound_reverb_attr.mode) {
            return;
        }
        mode = g_suzuki_sound_reverb_attr.mode;
    }
    g_suzuki_sound_spu_state.reverb_depth = depth;
    g_suzuki_sound_reverb_attr.delay = delay;
    g_suzuki_sound_reverb_attr.feedback = feedback;
    if (mode < PSYQ_SPU_REVERB_MODE_COUNT) {
        if (current.mode != mode || mode == SPU_REV_MODE_OFF) {
            SpuSetReverb(SPU_OFF);
            g_suzuki_sound_reverb_attr.mode = mode | SPU_REV_MODE_CLEAR_WA;
            g_suzuki_sound_reverb_attr.mask |= SPU_REV_MODE;
            SpuSetReverbModeParam(&g_suzuki_sound_reverb_attr);
            g_suzuki_sound_reverb_attr.mask = 0;
            SpuSetReverb(SPU_ON);
        }
        suzuki_sound_commit_volume_change();
        SpuSetReverbDepth(&g_suzuki_sound_reverb_attr);
        g_suzuki_sound_reverb_attr.mask = 0;
    }
}

/* Sets the master volume, at once or as a ramp of time ticks.
 *
 * The root-counter handler steps music_ramp and applies it to common.mvol;
 * a ramp is only started when the 8.8 volume actually changes. */
void suzuki_sound_set_master_volume(s16 volume, s16 time) {
    s32 difference;

    g_suzuki_sound_spu_state.music_ramp.target = volume;
    if (time == 0) {
        g_suzuki_sound_spu_state.music_ramp.value = volume << 16;
        g_suzuki_sound_spu_state.music_ramp.count = 0;
        g_suzuki_sound_spu_state.music_volume = volume;
        suzuki_sound_set_vol_balance(volume, &g_suzuki_sound_spu_state.common.mvol, 0);
        g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
    } else {
        difference = ((volume << 16) >> 8) - (g_suzuki_sound_spu_state.music_ramp.value >> 8);
        if (difference != 0) {
            g_suzuki_sound_spu_state.music_ramp.count = time;
            g_suzuki_sound_spu_state.music_ramp.step = (difference / time) << 8;
        }
    }
}

/* Sets the CD audio volume, at once or as a ramp of time ticks; the
 * CD twin of suzuki_sound_set_master_volume, applied to common.cd.volume. */
void suzuki_sound_set_cd_volume(s16 volume, s16 time) {
    s32 difference;

    g_suzuki_sound_spu_state.cd_ramp.target = volume;
    if (time == 0) {
        g_suzuki_sound_spu_state.cd_ramp.value = volume << 16;
        g_suzuki_sound_spu_state.cd_ramp.count = 0;
        g_suzuki_sound_spu_state.cd_volume = volume;
        suzuki_sound_set_vol_balance(volume, &g_suzuki_sound_spu_state.common.cd.volume, 0);
        g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    } else {
        difference = ((volume << 16) >> 8) - (g_suzuki_sound_spu_state.cd_ramp.value >> 8);
        if (difference != 0) {
            g_suzuki_sound_spu_state.cd_ramp.count = time;
            g_suzuki_sound_spu_state.cd_ramp.step = (difference / time) << 8;
        }
    }
}

/* Sets the CD input's reverb and mix switches and applies the common
 * attributes. */
void suzuki_sound_set_cd_reverb(s32 reverb, s32 mix) {
    g_suzuki_sound_spu_state.common.cd.reverb = reverb;
    g_suzuki_sound_spu_state.common.cd.mix = mix;
    g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_CDREV | SPU_COMMON_CDMIX;
    SpuSetCommonAttr(&g_suzuki_sound_spu_state.common);
}

/* Rebuilds the master, CD and reverb-depth volume pairs for the current
 * sound type and marks them for the next SpuSetCommonAttr and
 * SpuSetReverbDepth. */
void suzuki_sound_commit_volume_change(void) {
    suzuki_sound_set_vol_balance(g_suzuki_sound_spu_state.music_volume, &g_suzuki_sound_spu_state.common.mvol, 0);
    suzuki_sound_set_vol_balance(g_suzuki_sound_spu_state.cd_volume, &g_suzuki_sound_spu_state.common.cd.volume, 0);
    suzuki_sound_set_vol_balance(g_suzuki_sound_spu_state.reverb_depth, &g_suzuki_sound_reverb_attr.depth, 1);
    g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    g_suzuki_sound_reverb_attr.mask |= SPU_REV_DEPTHL | SPU_REV_DEPTHR;
}

/* Builds an SpuVolume pair from volume for the current sound type.
 *
 * Mono and stereo use volume on both sides. The wide types phase-invert one
 * side: type 2 (0x300) the right side for mode 0 (master and CD volume) and
 * the left for mode 1 (reverb depth), type 3 (0x500) the opposite.
 *
 * volume is s32: an s16 parameter adds a copy of a0. The type 3 test stores
 * `mode != 1` as a value (`xori` + `bnez`); as a branch condition GCC loads
 * the constant (`li` + `bne`). */
void suzuki_sound_set_vol_balance(s32 volume, SpuVolume* volume_out, u8 mode) {
    s32 invert_left;

    volume_out->right = volume;
    volume_out->left = volume;
    if (g_suzuki_sound_driver_flags & (SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE)) {
        invert_left = mode;
        if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_WIDE) == 0) {
            invert_left = invert_left != 1;
            if (invert_left) {
                volume_out->left = -volume;
            } else {
                volume_out->right = -volume;
            }
        } else if (invert_left) {
            volume_out->left = -volume;
        } else {
            volume_out->right = -volume;
        }
    }
}

/* Store the retained 16-bit driver value. No caller or reader has been
 * identified in the retail code or SPU transfer paths. */
void suzuki_sound_set_unread_value(u16 value) {
    g_suzuki_sound_unread_value = value;
}

/* SPU transfer-complete callback: clears the driver's read, write and
 * transfer-busy flags. */
void suzuki_sound_spu_transfer_callback(void) {
    g_suzuki_sound_driver_flags
        &= ~(SUZUKI_SOUND_DRIVER_SPU_READ | SUZUKI_SOUND_DRIVER_SPU_WRITE | SUZUKI_SOUND_DRIVER_SPU_TRANSFER);
}

/* Handler of the HwSPU event suzuki_sound_init opens. It does nothing and
 * returns 0. */
s32 suzuki_sound_spu_event_handler(void) {
    return 0;
}

/* Starts an SPU RAM transfer at spu_address once the previous one is done.
 *
 * The low nibble of mode selects it: 1 writes size bytes from data, 2 reads
 * them into data, 3 and 4 read the decoded CD/voice buffers (all, or CD only)
 * with the result in g_suzuki_sound_decoded_data_result. The status bits 0x10-0x40 stay set until
 * the transfer callback clears them. The empty case 5 is part of the
 * target's five-entry jump table. */
void suzuki_sound_transfer_spu_data(u32 spu_address, void* data, u32 size, s32 mode) {
    suzuki_sound_wait_for_spu_transfer(SUZUKI_SOUND_SPU_TRANSFER_WAIT);
    g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_SPU_TRANSFER;
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
    SpuSetTransferStartAddr(spu_address);
    switch (mode & SUZUKI_SOUND_SPU_TRANSFER_MODE_MASK) {
    case SUZUKI_SOUND_SPU_TRANSFER_WRITE:
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_SPU_WRITE | SUZUKI_SOUND_DRIVER_SPU_TRANSFER;
        SpuWrite(data, size);
        break;
    case SUZUKI_SOUND_SPU_TRANSFER_READ:
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_SPU_READ | SUZUKI_SOUND_DRIVER_SPU_TRANSFER;
        SpuRead(data, size);
        break;
    case SUZUKI_SOUND_SPU_TRANSFER_READ_DECODED:
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_SPU_READ | SUZUKI_SOUND_DRIVER_SPU_TRANSFER;
        g_suzuki_sound_decoded_data_result = SpuReadDecodedData(data, SPU_ALL);
        break;
    case SUZUKI_SOUND_SPU_TRANSFER_READ_DECODED_CD:
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_SPU_READ | SUZUKI_SOUND_DRIVER_SPU_TRANSFER;
        g_suzuki_sound_decoded_data_result = SpuReadDecodedData(data, SPU_CDONLY);
        break;
    case SUZUKI_SOUND_SPU_TRANSFER_NONE:
        break;
    }
}

/* Empty driver routine; nothing in the tree calls it or takes its address. */
void suzuki_sound_noop_1(void) {
}
