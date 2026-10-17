#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/libspu.h"
#include "psx/types.h"

enum { SUZUKI_SOUND_HEAP_BYTE_COUNT = 32768 };

enum {
    SUZUKI_SOUND_SPU_ALLOC_BLOCK_CAPACITY = 6,
    SUZUKI_SOUND_SPU_ALLOC_RECORD_COUNT = SUZUKI_SOUND_SPU_ALLOC_BLOCK_CAPACITY + 1
};

extern u8 g_suzuki_sound_heap_memory[SUZUKI_SOUND_HEAP_BYTE_COUNT]; /* the CPU heap arena */

/* Allocation records and one trailing free-range record. */
extern psyq_spu_heap_record_t g_suzuki_sound_spu_malloc_table[SUZUKI_SOUND_SPU_ALLOC_RECORD_COUNT];
typedef char suzuki_spu_allocator_storage_size_must_be_56[sizeof(g_suzuki_sound_spu_malloc_table) == 56 ? 1 : -1];
extern u32 g_suzuki_unknown_80032a34; /* only ever stored */
extern s16 g_suzuki_unknown_80032a58; /* only ever stored */

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
