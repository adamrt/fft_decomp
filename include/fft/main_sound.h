#ifndef FFT_MAIN_SOUND_H
#define FFT_MAIN_SOUND_H

#include "psx/cd.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Suzuki sound driver (SCUS_942.21 0x800120f4-0x800186c4) records, tables and
 * globals. Offsets and widths are taken from the driver code named beside
 * each field; field_XX and _unknown_XX members are provisional. */

/* Linear ramp stepped once per tick by main_smd_step_ramp (0x80014f18):
 * value moves by step until count reaches zero, then snaps to target << 16.
 * The root-counter handler tests count with `lh` before stepping MUS
 * +0x88/+0x94/+0xa0/+0xac and the volume ramps of g_main_sound_spu_state. */
typedef struct suzuki_ramp {
    s32 value;  /* 0x00 */
    s32 step;   /* 0x04 */
    s16 count;  /* 0x08 */
    s16 target; /* 0x0a */
} suzuki_ramp_t;

/* Suzuki heap block header (0x10 bytes; payload follows). The $gp-relative
 * allocator (0x8001423c-0x8001442c) rounds requests to 16 bytes and chains
 * blocks in address order from g_main_sound_heap_blocks. */
typedef struct suzuki_heap_block {
    u16 flags;                      /* 0x00; 0x8000 list head, 0x2 in use */
    u16 _unknown_02;                /* 0x02; cleared on allocation */
    u32 _unknown_04;                /* 0x04; cleared on allocation */
    u8* end;                        /* 0x08; end of the payload */
    struct suzuki_heap_block* next; /* 0x0c */
} suzuki_heap_block_t;

/* WAVESET instrument entry (0x10 bytes). main_smd_set_instrument
 * (0x80016fb4) and main_smd_get_instrument_attr copy it into a channel or an
 * SpuVoiceAttr. */
typedef struct suzuki_instrument {
    u32 start;        /* 0x00; sample offset from the waveset's SPU address */
    u16 loop_offset;  /* 0x04; loop start relative to start */
    u16 fine_tune;    /* 0x06; copied to the channel fine_tune */
    u8 attack_rate;   /* 0x08 */
    u8 decay_rate;    /* 0x09 */
    u8 sustain_rate;  /* 0x0a */
    u8 release_rate;  /* 0x0b */
    u8 sustain_level; /* 0x0c */
    u8 attack_mode;   /* 0x0d */
    u8 sustain_mode;  /* 0x0e */
    u8 release_mode;  /* 0x0f */
} suzuki_instrument_t;

/* WAVESET.WD header as loaded. main_sound_load_waveset
 * (0x80017c8c) uploads wave_size bytes from wave_offset to SPU RAM and keeps
 * header_size bytes on the Suzuki heap, linked from g_main_sound_waveset_list
 * through next. Select Sound Font (0x80016e48) matches id. */
typedef struct suzuki_waveset {
    u8 _unknown_00[0x10];               /* 0x00; "dwdsP" magic, file size at 0x08 */
    u32 header_size;                    /* 0x10 */
    u32 wave_size;                      /* 0x14 */
    u32 wave_offset;                    /* 0x18 */
    u8 _unknown_1c[4];                  /* 0x1c */
    u16 id;                             /* 0x20 */
    u8 _unknown_22[6];                  /* 0x22 */
    u32 spu_address;                    /* 0x28; SpuMalloc result, freed by main_sound_free_waveset */
    struct suzuki_waveset* next;        /* 0x2c */
    suzuki_instrument_t instruments[1]; /* 0x30 */
} suzuki_waveset_t;

/* SMD/SED sequence header ("smds"), read by main_smd_transfer_music_data,
 * main_smd_init_channels, main_smd_init_channel_streams and
 * main_smd_get_music_filename. */
typedef struct suzuki_smd_header {
    u8 magic[4];            /* 0x00 */
    u8 _unknown_04[4];      /* 0x04 */
    u32 size;               /* 0x08 */
    u8 _unknown_0c[4];      /* 0x0c */
    u16 id;                 /* 0x10; copied to MUS id and channel sound_id */
    u8 field_12;            /* 0x12; copied to MUS field_14 */
    u8 tick_divisor;        /* 0x13 */
    u8 channel_count;       /* 0x14 */
    u8 field_15;            /* 0x15; copied to MUS field_17 */
    u16 waveset_id;         /* 0x16 */
    u16 field_18;           /* 0x18; copied to MUS field_1a */
    s8 reverb_mode;         /* 0x1a */
    u8 reverb_depth;        /* 0x1b */
    u8 reverb_delay;        /* 0x1c */
    u8 reverb_feedback;     /* 0x1d */
    u16 filename_offset;    /* 0x1e */
    u16 data_offset;        /* 0x20 */
    u16 channel_offsets[1]; /* 0x22; zero-terminated */
} suzuki_smd_header_t;

/* Suzuki sequencer modulator (0x20 bytes). Each channel has four, at
 * channel+0xe0. step is one of the g_main_smd_modulator_waveforms functions,
 * called by main_smd_update_modulators (0x8001749c), which adds the scaled
 * result to the channel output selected by target. counter_14 delays the
 * start; while counter_18 is below 0x100 the output is scaled by
 * counter_18 / 0x100 and counter_18 grows by counter_18_reset (fade-in). */
typedef struct suzuki_modulator {
    s32 (*step)(struct suzuki_modulator*); /* 0x00 */
    s32 value;                             /* 0x04 */
    s32 increment;                         /* 0x08 */
    s32 amplitude;                         /* 0x0c; from main_smd_modulator_calculate_step */
    u16 timer;                             /* 0x10 */
    u16 timer_reset;                       /* 0x12 */
    u16 counter_14;                        /* 0x14 */
    u16 counter_14_reset;                  /* 0x16 */
    s16 counter_18;                        /* 0x18 */
    u16 counter_18_reset;                  /* 0x1a */
    u8 target;                             /* 0x1c; 0 pitch, 1 volume, 2 balance */
    u8 waveform;                           /* 0x1d; index into g_main_smd_modulator_waveforms */
    s16 flags;                             /* 0x1e; bit 0: active; bits 2-3: step state */
} suzuki_modulator_t;

typedef char suzuki_modulator_size_must_be_0x20[(sizeof(suzuki_modulator_t) == 0x20) ? 1 : -1];

typedef s32 (*suzuki_modulator_step_t)(suzuki_modulator_t* modulator);

/* Repeat stack entry (0xc bytes) pushed by Repeat (0x98) and consumed by
 * Coda (0x99) and To Coda (0x9A). */
typedef struct suzuki_repeat {
    u8 count;       /* 0x00 */
    u8 _unknown_01; /* 0x01 */
    u8 octave_base; /* 0x02 */
    u8 coda_octave; /* 0x03 */
    u8* start;      /* 0x04 */
    u8* coda;       /* 0x08 */
} suzuki_repeat_t;

/* Suzuki sequencer channel record (0x160 bytes), the third argument of every
 * SMD opcode handler (note data, music, channel). Field names follow the
 * opcode that writes them; field_XX members are written by unnamed opcodes.
 *
 * active is the note flag word: 0x1 in use, 0x4 set from MUS status 0x2000,
 * 0x8 keeps the velocity, 0x20 muted, 0x100 fermata, 0x400 note end or rest,
 * 0x800 set and cleared by opcodes 0xB0/0xB1, 0x4000 stream set
 * (main_smd_init_channel_streams); main_smd_set_flags_all_channels ORs a mask
 * into every non-zero word.
 *
 * func_flags marks the SPU voice attributes main_sound_update_voice_attrs
 * pushes: 0x1 volume, 0x2 volume and volume mode, 0x4 pitch, 0x8 start and
 * loop addresses, 0x10 attack (time and mode), 0x20 decay, 0x40 sustain
 * (time and mode), 0x80 release (time and mode), 0x100 sustain level. 0x4,
 * 0x10 and 0x40 also request the pitch-LFO, noise and reverb voice-mask
 * updates. note_flags2 bit 0 queues a key-on, bit 1 a key-off, bit 8 a
 * volume/balance change and bit 9 a pitch change for main_smd_update_voices.
 * flags_06 bits 0, 3 and 4 mark the pitch-slide, volume and balance ramps
 * stepped by main_smd_update_channel_ramps.
 *
 * octave_base is written at halfword width (Octave, Raise/Lower Octave, End
 * Bar & Loop) but saved into the u8 loop_octave_base by Loop (0x91) and
 * opcode 0x8D, which read it with `lbu`: GCC narrows the u16 load itself.
 *
 * sound_id is the SMD/SED id for music and `resource id << 16 | n` for VFX
 * sounds; Play VFX SMD (0x80015c38) reads the resource id half with `lh`,
 * the SFX API compares the whole word. */
typedef struct suzuki_music_channel {
    u16 active;      /* 0x00 */
    u16 note_flags2; /* 0x02 */
    u16 func_flags;  /* 0x04 */
    u16 flags_06;    /* 0x06; bit 1 toggled by 0xD5, bit 2 by 0xD6, bit 3 ramp active */
    union {
        u32 raw;
        struct {
            u16 low;
            s16 high; /* VFX resource id */
        } halves;
    } sound_id;                /* 0x08 */
    u8 channel_number;         /* 0x0c; 0xff + channel index (SFX: the index); write-only */
    u8 priority;               /* 0x0d; SFX steal priority (channel id >> 8; stolen when <= 0x20); music: 0x10 */
    u8 _unknown_0e[2];         /* 0x0e */
    u32 start_tick;            /* 0x10; g_main_sound_tick_count when main_sound_start_sfx started the channel */
    u8* note_data_start;       /* 0x14 */
    u8* note_data;             /* 0x18; read position */
    u8* loop_note_data;        /* 0x1c; set by Loop and opcode 0x8D, jumped to by End Bar & Loop */
    u8* restart_note_data;     /* 0x20; set by main_smd_init_channel_streams */
    u32 field_24;              /* 0x24; cleared by the channel initialiser */
    u16 loop_count;            /* 0x28; End Bar & Loop */
    u8 field_2a;               /* 0x2a; cleared by the channel initialiser */
    u8 loop_octave_base;       /* 0x2b; octave_base saved with loop_note_data */
    u8 instrument;             /* 0x2c */
    u8 voice;                  /* 0x2d; SPU voice (opcode 0xAA); bit 0 allows the pitch LFO */
    u16 release_2e;            /* 0x2e; Release stores its byte here and in release_time */
    suzuki_waveset_t* waveset; /* 0x30; Select Sound Font */
    u32 voice_mask;            /* 0x34; SPU voice bit(s) owned by this channel */
    u32 field_38;              /* 0x38; initialised to 0xff9f */
    s16 spu_volume_left;       /* 0x3c; SpuSetVoiceVolume(Attr) left, from main_smd_update_voices */
    s16 spu_volume_right;      /* 0x3e */
    s16 spu_volume_mode_left;  /* 0x40; SpuSetVoiceVolumeAttr left mode; cleared by the channel initialiser */
    s16 spu_volume_mode_right; /* 0x42 */
    u8 _unknown_44[4];         /* 0x44 */
    u16 spu_pitch;             /* 0x48; SpuSetVoicePitch value, from main_smd_update_voices */
    u8 _unknown_4a[6];         /* 0x4a */
    u32 start_address;         /* 0x50; instrument sample SPU address */
    u32 loop_address;          /* 0x54; instrument loop SPU address */
    s32 attack_mode;           /* 0x58 */
    s32 sustain_mode;          /* 0x5c */
    s32 release_mode;          /* 0x60 */
    u16 attack_time;           /* 0x64 */
    u16 decay_time;            /* 0x66 */
    u16 sustain_time;          /* 0x68 */
    u16 release_time;          /* 0x6a */
    u16 sustain_level;         /* 0x6c */
    u8 _unknown_6e[6];         /* 0x6e */
    s16 rest_length;           /* 0x74; Rest or Fermata hold length, counted down per tick */
    u8 length_adjust;          /* 0x76; signed note-length adjust: opcode 0xAD adds to or clears it */
    u8 _unknown_77;            /* 0x77 */
    u16 note_ticks;            /* 0x78; ticks left in the note; 0 sets active bit 0x400 */
    u16 gate_time;             /* 0x7a; key-on time in sixteenths of the length (0xA9); initialised to 0xf */
    u8 previous_key;           /* 0x7c; previous note's key, the portamento start; cleared by the channel initialiser */
    u8 key;                    /* 0x7d; key number of the current note */
    u16 octave_base;           /* 0x7e; key number of the current octave, octave * 12 */
    s32 pitch_slide;           /* 0x80; accumulated by the 0xD4 ramp */
    s16 fine_tune;             /* 0x84; instrument fine_tune, added to key << 8 (1/256 semitone) */
    s16 pitch_offset;          /* 0x86; fine pitch; 0xD0/0xD1 scale their byte by 32, 0xD2 by 8 */
    s16 pitch_modulation;      /* 0x88; modulator target 0 output */
    s16 volume_modulation;     /* 0x8a; modulator target 1 output */
    s16 balance_modulation;    /* 0x8c; modulator target 2 output */
    u8 _unknown_8e[2];         /* 0x8e */
    u16 portamento_ticks;      /* 0x90; portamento length (opcode 0xD6), mirrored into flags_06 bit 2 */
    s16 balance;               /* 0x92; balance byte << 8 */
    s16 velocity;              /* 0x94; velocity << 8, 0x6000 by default */
    u8 _unknown_96[2];         /* 0x96 */
    s32 volume;                /* 0x98; volume byte << 24 */
    s32 pitch_slide_step;      /* 0x9c; opcode 0xD4 */
    s32 volume_step;           /* 0xa0; Fermata Ramp (0xE2) */
    s16 balance_step;          /* 0xa4; opcode 0xEA */
    u16 pitch_slide_count;     /* 0xa6 */
    u16 volume_step_count;     /* 0xa8 */
    u16 balance_step_count;    /* 0xaa */
    u16 repeat_depth;          /* 0xac; -1 when the repeat stack is empty */
    u16 modulator_index;       /* 0xae; modulator selected by opcode 0xF0 */
    suzuki_repeat_t repeats[4];       /* 0xb0 */
    suzuki_modulator_t modulators[4]; /* 0xe0 */
} suzuki_music_channel_t;

typedef char suzuki_music_channel_size_must_be_0x160[(sizeof(suzuki_music_channel_t) == 0x160) ? 1 : -1];

/* Suzuki MUS (music sequence) record: 0xb8 bytes followed by channel_count
 * channels, allocated by SuzukiPutPlaySMD. It is the second argument of every
 * SMD opcode handler. Queued records form a linked list through next, headed
 * by g_main_sound_active_music_list; snapshot chains saved copies of the
 * whole record (main_smd_save_snapshot) through their own snapshot fields.
 *
 * status bit 15 is set while the music plays (SuzukiGetMusicPlaying);
 * main_sound_calculate_reverb_voices tests that bit as a sign (`lh` and
 * `bgez`), and bit 0 there marks the reverb voices as excludable. Bit 4 marks
 * a saved snapshot, bit 1 the SFX record, 0x100 music paused by
 * main_smd_pause_music (cleared on resume), 0x4000 a pending full voice
 * update (main_sound_update_voice_attrs clears it) and 0x2000 sets channel
 * active bit 0x4 at initialisation.
 *
 * tempo is tempo byte << 16. It is a union because Scale Tempo (0x80016e14)
 * reads the tempo byte back with `lh 0x7e`, while Tempo, Accelerando and
 * opcode 0xA2 use the whole word. */
typedef struct suzuki_music {
    struct suzuki_music* next;     /* 0x00 */
    struct suzuki_music* snapshot; /* 0x04; freed by main_smd_free_snapshots */
    suzuki_smd_header_t* smd;      /* 0x08 */
    u8 _unknown_0c[4];             /* 0x0c */
    u16 status;                    /* 0x10 */
    u16 id;                        /* 0x12; SMD id */
    u8 field_14;                   /* 0x14; SMD field_12 */
    u8 tick_divisor;               /* 0x15 */
    u8 channel_count;              /* 0x16 */
    u8 field_17;                   /* 0x17; SMD field_15 */
    s16 waveset_id;                /* 0x18; sound font id */
    u16 field_1a;                  /* 0x1a; SMD field_18, 0x7f for SFX */
    u8 field_1c;                   /* 0x1c; set by opcode 0xA4, adjusted by 0xA5 */
    u8 channel_select;             /* 0x1d; opcode 0x8D acts when its byte matches */
    u16 noise_clock;               /* 0x1e; opcodes 0xB4/0xB5 */
    s32 tick_20;                   /* 0x20; incremented every tick */
    u32 tick_24;                   /* 0x24; ticks while the record owns voices */
    u32 ticks;                     /* 0x28; root-counter ticks played, in 1/256 units (tempo scale) */
    u32 tick_2c;                   /* 0x2c; restore the snapshot when tick_24 reaches it (0: never) */
    u16 min_loop_count;            /* 0x30; main_smd_get_min_loop_count */
    u16 bar;                       /* 0x32; 1-based bar counter */
    u16 beat;                      /* 0x34; beat within the bar */
    u16 beat_ticks_36;             /* 0x36 */
    u16 beats_per_bar;             /* 0x38 */
    u16 ticks_per_beat;            /* 0x3a */
    u16 beat_unit;                 /* 0x3c */
    u16 beats_remaining;           /* 0x3e */
    u8 _unknown_40[4];             /* 0x40 */
    s32 reverb_mode;               /* 0x44 */
    s16 reverb_depth;              /* 0x48; depth byte << 8 */
    u8 _unknown_4a[2];             /* 0x4a */
    s32 reverb_delay;              /* 0x4c */
    s32 reverb_feedback;           /* 0x50 */
    u16 stop_bar;                  /* 0x54; main_smd_set_stop_bar: the music stops when bar reaches it */
    u8 _unknown_56[2];             /* 0x56 */
    u32 channel_mask;              /* 0x58; bit n: channel n has note data and is still running */
    u32 mute_mask;                 /* 0x5c; muted channels */
    u32 key_on_mask;               /* 0x60 */
    u32 key_off_mask;              /* 0x64 */
    u32 lfo_voice_mask;            /* 0x68 */
    u32 noise_voice_mask;          /* 0x6c */
    u32 reverb_voice_mask;         /* 0x70 */
    s32 tempo_accumulator;         /* 0x74; one sequencer tick per 0x10000 of scaled_tempo; initialised to 0x10000 */
    s32 scaled_tempo;              /* 0x78; tempo * tempo_scale */
    union {
        s32 raw;
        struct {
            u16 low;
            s16 high; /* the tempo byte itself */
        } halves;
    } tempo;                            /* 0x7c */
    s32 tempo_step;                     /* 0x80; per-step tempo delta of opcode 0xA2 */
    u16 tempo_step_count;               /* 0x84 */
    u16 tempo_target;                   /* 0x86; tempo byte the 0xA2 ramp approaches */
    suzuki_ramp_t tempo_scale;          /* 0x88; value is scale byte << 24, its high halfword the multiplier */
    suzuki_ramp_t master_volume;        /* 0x94; value is volume byte << 24 */
    suzuki_ramp_t pitch_shift;          /* 0xa0 */
    suzuki_ramp_t balance_shift;        /* 0xac */
    suzuki_music_channel_t channels[1]; /* 0xb8; channel_count entries */
} suzuki_music_t;

typedef char suzuki_music_channels_must_be_at_0xb8[(sizeof(suzuki_music_t) == 0xb8 + 0x160) ? 1 : -1];

/* Loaded SED/SMD sound block ("feds" VFX resource), kept by the Suzuki driver
 * in a singly linked list through next (SuzukiAppendVFXSMD). Play VFX SMD
 * (0x80015c38) matches id and indexes channel_offsets. */
typedef struct main_sound_resource {
    u8 unknown_00[0xa];
    u16 id;                           /* 0x0a */
    u16 volume_offset;                /* 0x0c; offset of the per-sound u8 volume table */
    u8 unknown_0e[2];                 /* 0x0e */
    struct main_sound_resource* next; /* 0x10 */
    u16 channel_offsets[1];           /* 0x14 */
} main_sound_resource_t;

/* Driver SPU state at 0x80037020: the common attributes passed to LIBSPU,
 * the volumes they and g_main_sound_reverb_attr (0x80037008) are built from,
 * and two volume ramps stepped by the root-counter handler (0x800149dc). The
 * reverb attributes are a separate object: every user addresses them from
 * their own base, not from this record's. */
typedef struct suzuki_spu_state {
    SpuCommonAttr common;     /* 0x00 0x80037020 */
    s16 music_volume;         /* 0x28 0x80037048; applied to common.mvol */
    s16 cd_volume;            /* 0x2a 0x8003704a; applied to common.cd.volume */
    s16 reverb_depth;         /* 0x2c 0x8003704c; applied to g_main_sound_reverb_attr.depth */
    s16 sound_type;           /* 0x2e 0x8003704e; Put Sound Type argument */
    suzuki_ramp_t music_ramp; /* 0x30 0x80037050 */
    suzuki_ramp_t cd_ramp;    /* 0x3c 0x8003705c */
} suzuki_spu_state_t;

typedef char suzuki_spu_state_size_must_be_0x48[(sizeof(suzuki_spu_state_t) == 0x48) ? 1 : -1];

/* SMD opcode handler, called through g_main_smd_opcode_handlers by
 * main_smd_read_instructions with the byte after the opcode; it returns the
 * next read position. Handlers that ignore an argument may declare it
 * void*; the ABI is the same. */
typedef u8* (*suzuki_smd_handler_t)(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);

/* Driver tables (read-only data 0x80028b0c-0x8002a8d8). */
extern suzuki_smd_handler_t g_main_smd_opcode_handlers[0x80];      /* 0x80028b0c; opcodes 0x80-0xff */
extern u8 g_main_smd_opcode_lengths[0x80];                         /* 0x80028d0c; opcode plus parameters */
extern u8 g_main_smd_note_durations[228];                          /* 0x80028d8c; ticks per note byte */
extern u8 g_main_smd_note_semitones[228];                          /* 0x80028e70; note byte / 19 */
extern suzuki_modulator_step_t g_main_smd_modulator_waveforms[16]; /* 0x80028f54 */
extern u8 g_main_smd_key_octaves[120];                             /* 0x80028fe8; key / 12 */
extern u8 g_main_smd_key_semitones[120];                           /* 0x80029060; key % 12 */
/* 0x800290d8: SPU pitch for semitone s and fine step f at index (s << 8) + f
 * (octave 6 reference); main_smd_calculate_pitch indexes it flat. */
extern u16 g_main_smd_pitch_table[12 * 256];

/* Driver globals (0x800329f0-0x80032a68, gp = 0x800329bc). The heap globals
 * are only ever reached $gp-relative, by 0x8001423c-0x8001442c. */
extern s16 g_main_sound_sfx_channel_count;                /* SFX request mode; the Play Sound wrappers store 2 */
extern suzuki_heap_block_t* g_main_sound_heap_arena;      /* 0x800329f8 */
extern suzuki_heap_block_t* g_main_sound_heap_blocks;     /* 0x800329fc */
extern main_sound_resource_t* g_main_sound_resource_list; /* 0x80032a00 */
extern s16* g_main_sound_spu_transfer_status_records;     /* SPU transfer status records, 16 bytes each */
extern u32 g_main_sound_music_key_off_voices;             /* voices queued for key-off by stopped music */
extern u32 g_main_mask_exclusion;                         /* 0x80032a0c; voices reserved by SFX */
extern u32 g_main_sound_sfx_restart_channels;             /* last SFX voice search: channel mask */
extern u32 g_main_sound_sfx_restart_voices;               /* last SFX voice search: voice mask */
extern s32 g_main_smd_random_state;                       /* 0x80032a18 */
extern s16 g_main_sound_sfx_instrument;
extern u32 g_main_sound_sfx_key_off_voices;        /* SFX voices pending key-off */
extern u16 D_80032A28;                             /* only ever stored */
extern u32 g_main_sound_tick_count;                /* root-counter tick count; odd ticks step the ramps */
extern u16 g_main_sound_spu_transfer_status_index; /* index into D_80032A04 */
extern u32 D_80032A34;                             /* only ever stored */
extern u32 g_main_sound_heap_size;                 /* 0x80032a38 */
extern CdlATV g_main_sound_cd_mix;                 /* 0x80032a3c; written by Put Sound Type */
/* g_main_sound_cd_mix val1 (CD left to SPU right) and val3 (CD right to SPU
 * left) as their own symbols: main_sound_put_type stores them through their
 * own `lui`, which field stores of g_main_sound_cd_mix do not reproduce. */
extern u8 g_main_sound_cd_mix_left_to_right;
extern u8 g_main_sound_cd_mix_right_to_left;
extern s16 g_main_sound_decoded_data_result;        /* SPU transfer controller state */
extern suzuki_waveset_t* g_main_sound_waveset_list; /* 0x80032a44 */
extern s32 g_main_sound_spu_event;                  /* 0x80032a48; OpenEvent handle of main_sound_spu_event_handler */
extern u16 g_main_sound_voice_update_requests;      /* voice-mask updates: bit 0 pitch LFO, 1 noise, 2 reverb */
extern suzuki_music_t* g_main_sound_active_music_list; /* 0x80032a50 */
/* Driver status: bit 0 root-counter event enabled, 0x10-0x40 SPU transfer
 * in progress (cleared by the transfer callback), 0x700 sound type, 0x1000
 * SFX enabled, 0x2000 apply the sound type at start-up, 0x8000 initialised. */
extern u16 g_main_sound_driver_flags;
extern s16 D_80032A58;                         /* only ever stored */
extern s32 g_main_root_counter_2_event;        /* 0x80032a5c; OpenEvent handle of main_sound_root_counter_2_handler */
extern suzuki_music_t* g_main_sound_sfx_music; /* 0x80032a60; eight channels on SPU voices 16-23 */
extern u8* g_main_sound_heap_end;              /* 0x80032a64 */

extern SpuReverbAttr g_main_sound_reverb_attr;    /* 0x80037008 */
extern suzuki_spu_state_t g_main_sound_spu_state; /* 0x80037020 */
extern u8 g_main_sound_heap_memory[];             /* 0x800370bc; the Suzuki heap arena */
extern u8 g_main_sound_spu_malloc_table[];        /* 0x800408e0; SpuInitMalloc(6, ...) records */

/* Scenario-music playback state at 0x8004d95c (SCUS_942.21). current_music
 * is the Suzuki music record of the playing scenario music:
 * main_sound_stop_forced_music hands it straight to
 * SuzukiDeallocateMUSChannels, which takes a suzuki_music_t*. */
typedef struct main_sound_music_state {
    s32 scenario_track;                /* 0x00 0x8004d95c slot of the playing scenario music */
    suzuki_music_t* current_music;     /* 0x04 0x8004d960 */
    suzuki_smd_header_t* forced_music; /* 0x08 0x8004d964 SMD data of the playing music */
    s32 volume;                        /* 0x0c 0x8004d968 */
} main_sound_music_state_t;

/* Music slot table at 0x8004d96c. Slots are 1-based: slots 1-2 hold the
 * scenario music opened by main_sound_open_music_into_free_slot, and tune t
 * (1-5, MUSIC_45..49.SMD) lives in slot 2 + t, so smd[3..7] (0x8004d998) hold
 * the five permanent tunes that main_sound_open_generic_sfx loads. Both
 * arrays are indexed by slot, so slot 7's handle is the word smd[0].
 * Evidence: 0x80043de0 indexes handles at 0x8004d978 + 4t and smd at
 * 0x8004d994 + 4t from a 0x8004d96c base; 0x80043a90 reads smd at
 * 0x8004d98c + 4*track and the handle 0x1c bytes below it. */
typedef struct main_sound_music_slots {
    s32 tune;                    /* 0x00 0x8004d96c pending tune (id | 0x80) or playing tune id */
    s32 handles[7];              /* 0x04 0x8004d970 Suzuki music handles; [0] is the last opened slot */
    suzuki_smd_header_t* smd[8]; /* 0x20 0x8004d98c loaded SMD data per slot */
} main_sound_music_slots_t;

/* The music state and slot table are one object: main_sound_open_generic_sfx
 * derives the slot words from the &state.forced_music base register, and the
 * tune/slot helpers reach across tune, handles and smd from single bases. */
typedef struct main_sound_music {
    main_sound_music_state_t state; /* 0x00 0x8004d95c */
    main_sound_music_slots_t slots; /* 0x10 0x8004d96c */
} main_sound_music_t;

/* Sound-effect ids passed to main_sound_play_sfx and the module queues that
 * feed it (wldcore_sound_play_effect, world_sound_dispatch_effect and the
 * g_*_sound_*effect_id globals); every path reaches main_sound_play_sfx, so
 * this is one id space. 1/2/3/5 are the Confirm, Cancel, Cursor Move and
 * Invalid sounds; the rest are named from their users. Other ids (camera
 * rotation, landing, weather, ...) stay literal until named. */
typedef enum main_sfx {
    MAIN_SFX_CONFIRM = 1,
    MAIN_SFX_CANCEL = 2,
    MAIN_SFX_CURSOR_MOVE = 3,
    MAIN_SFX_INVALID = 5,
    MAIN_SFX_PAGE_SWITCH = 6, /* L1/R1 unit and page switches */
    MAIN_SFX_UNEQUIP = 7,     /* remove equipment/abilities */
    MAIN_SFX_DISMISS = 9,     /* dismiss unit */
    MAIN_SFX_EQUIP = 0xa,     /* equip, set ability, best fit */
    /* Paired zoom sounds: battle_camera_toggle_zoom and battle_camera_zoom_map
     * play 0x11 with zoom action 4 and 0xe with action 2; the in/out reading
     * follows the zoom-target comment in battle_camera_toggle_zoom. The
     * ATTACK deploy screen also plays 0x11 when a placed unit is moved. */
    MAIN_SFX_CAMERA_ZOOM_IN = 0xe,
    MAIN_SFX_CAMERA_ZOOM_OUT = 0x11,
    MAIN_SFX_WINDOW_OPEN = 0x12,      /* help, message or text window opens */
    MAIN_SFX_TEXT_PAGE = 0x2d,        /* dialogue/scroll window advances a page */
    MAIN_SFX_CARD_ERROR = 0x30,       /* memory-card load, save or scan failure */
    MAIN_SFX_CAMERA_TILT = 0x31,      /* battle_camera_handle_tilt_request */
    MAIN_SFX_TEXT_GLYPH = 0x73,       /* typewriter tick after a dialogue glyph */
    MAIN_SFX_SHOP_TRANSACTION = 0x97, /* shop buy/sell confirmed */
} main_sfx_e;

extern main_sound_music_t g_main_sound_music;

/* Sector/size pairs for scenario and permanently loaded music SMD files. */
typedef struct main_sound_smd_file {
    s32 sector;
    s32 size;
} main_sound_smd_file_t;

extern main_sound_smd_file_t g_main_sound_scenario_smd_files[];
extern main_sound_smd_file_t g_main_sound_permanent_smd_files[5];

/* heap */
void main_heap_clear_smd_allocator_table(void);

/* return */
s32 main_return_zero_80043708(void);

/* sound */
s32 main_sound_get_current_scenario_music(void);
s32 main_sound_get_music_handle(s32 slot);

/* Verified SCUS scenario-music and tune wrappers. Include this header in
 * their definitions as well as callers so ABI declarations cannot drift. */
s32 main_sound_open_and_play_music(s32 scenario, s32 slot);
void main_sound_open_generic_sfx(void);
s32 main_sound_open_music_into_free_slot(s32 scenario);
s32 main_sound_open_music_into_slot(s32 scenario, s32 slot);
s32 main_sound_open_scenario_smd_files(s32 scenario);
void main_sound_play_sfx(s32 sound_id);
void main_sound_play_weather_sfx(s32 sound_id);
void main_sound_play_sfx_find_channel(s32 sound_id);
s32 main_sound_play_tune(s32 tune);
s32 main_sound_poll_scenario_smd_load(void);
s32 main_sound_set_current_music_balance_shift_timed(s32 value, s32 time);
s32 main_sound_set_current_music_pitch_shift_timed(s32 value, s32 time);
s32 main_sound_set_current_music_target(s32 volume, s32 time);
s32 main_sound_set_current_music_tempo_scale_timed(s32 value, s32 time);
void main_sound_set_current_music_track(s32 track);
s32 main_sound_set_current_music_volume(s32 volume, s32 time);
s32 main_sound_set_current_music_volume_timed(s32 time);
s32 main_sound_set_tune_volume(s32 volume);
s32 main_sound_stop_forced_music(void);

/* battle_map_step_init_sequence calls this without loading $a0, but the body reads sound_id. */
void main_sound_stop_sfx(int sound_id);
s32 main_sound_switch_music_track(s32 slot, s32 volume, s32 fade);
void main_sound_unload_current_scenario_music(void);
s32 main_sound_unload_scenario_mus(s32 slot);
void main_sound_unload_scenario_music_and_tunes(void);
s32 main_sound_update_tunes(void);
s32 main_sound_wait_for_scenario_smd_load(s32 scenario);

/* Suzuki driver entry points. The music handle passed by the game wrappers
 * is the suzuki_music_t* that SuzukiPutPlaySMD returns. */
void SuzukiAppendVFXSMD(main_sound_resource_t* resource);
void main_sound_remove_vfx_resource(main_sound_resource_t* resource);
void SuzukiCalcMusVolChange(suzuki_music_t* music, s16 volume, s16 time);
void SuzukiDeallocateMUSChannels(suzuki_music_t* music);
u32 SuzukiGetActiveChannels(suzuki_music_t* music);
u32 SuzukiGetMusicPlaying(suzuki_music_t* music);
suzuki_music_t* SuzukiPutPlaySMD(suzuki_smd_header_t* smd);
void SuzukiPlaySound1(int sound_id);
void SuzukiPlaySound2(int sound_id);
void SuzukiPlaySoundFindChannel(s32 sound_id);
void SuzukiSetSfxEcho(int sound_id, s16 echo);
void SuzukiSPUInitialiser(s32 flags);
void SuzukiTurnOffAllMusic(void);
void SuzukiUnloadMUS(suzuki_music_t* music);

/* Elapsed play time filled by main_smd_get_play_time. The root counter runs
 * at 240 Hz; music->ticks counts those ticks in 1/256 units. */
typedef struct suzuki_play_time {
    u32 tick_24;  /* 0x00; music->tick_24 */
    u16 fraction; /* 0x04; 1/240 s */
    u16 seconds;  /* 0x06 */
    u16 minutes;  /* 0x08 */
} suzuki_play_time_t;

/* MUS lifecycle, playback control and queue (0x800120f4-0x80014200). */
void main_smd_reset_music(suzuki_music_t* music, s16 volume, s16 fade);
void main_smd_resume_music(suzuki_music_t* music, s16 volume, s16 fade);
void main_smd_pause_music(suzuki_music_t* music);
void main_smd_stop_marked_music(void);
void main_smd_init_channel_streams(suzuki_music_t* music, suzuki_smd_header_t* smd);
void main_smd_set_tempo_scale(suzuki_music_t* music, s16 value, s16 time);
void main_smd_set_pitch_shift(suzuki_music_t* music, s16 value, s16 time);
void main_smd_set_balance_shift(suzuki_music_t* music, s16 value, s16 time);
void main_smd_set_channel_mute_mask(suzuki_music_t* music, u32 mask);
void main_smd_set_channel_select(suzuki_music_t* music, u8 value);
void main_smd_get_play_time(suzuki_music_t* music, suzuki_play_time_t* time);
u16* main_smd_get_min_loop_count(suzuki_music_t* music);
u8* main_smd_get_music_filename(suzuki_music_t* music);
void main_smd_dispatch_snapshot(suzuki_music_t* music, s32 mode);
void main_smd_discard_snapshot(suzuki_music_t* music);
void main_smd_save_snapshot(suzuki_music_t* music);
void main_smd_restore_snapshot(suzuki_music_t* music);
void main_smd_set_stop_bar(suzuki_music_t* music, u16 value);
u32 main_smd_retrigger_held_voices(suzuki_music_t* music);
u32 main_smd_get_held_voices(suzuki_music_t* music);
void main_smd_transfer_music_data(suzuki_music_t* music);
void main_smd_init_music_header(suzuki_music_t* music);
void main_smd_init_channels(suzuki_music_t* music);
void main_smd_free_snapshots(suzuki_music_t* music);
void main_smd_copy_snapshot(suzuki_music_t* music, suzuki_music_t* snapshot);
void main_smd_insert_music(suzuki_music_t* music);
s32 main_smd_remove_music(suzuki_music_t* music);
void main_smd_get_instrument_attr(SpuVoiceAttr* attr, s16 index);
void main_smd_set_flags_all_channels(u16 mask, suzuki_music_t* music);

/* SFX (0x800124cc-0x80013f74). The SFX music record has eight channels on
 * SPU voices 16-23; the low byte of a start_sfx channel id is the first
 * channel and its high byte the steal priority. */
void main_sound_set_sfx_enabled(s32 enabled);
void main_sound_play_sfx_in_channel(s32 sound_id, s32 channel);
void main_sound_play_sfx_with_settings_find_channel(s32 sound_id, s32 volume, s32 balance);
void main_sound_play_sfx_with_settings_in_channel(s32 sound_id, s32 channel, s32 volume, s32 balance);
void main_sound_play_2_sfx(s32 first, s32 second);
void main_sound_play_4_sfx(s32 first, s32 second, s32 third, s32 fourth);
void main_sound_stop_resource_sfx(main_sound_resource_t* resource);
void main_sound_stop_sfx_channels(s32 sound_id);
void main_sound_stop_sfx_channel_pair(s32 index);
void main_sound_release_sfx_voices(suzuki_music_t* music, u32 channel_mask, u32 voice_mask);
void main_sound_set_sfx_balance(s32 sound_id, s32 balance);
u32 main_sound_get_sfx_channels(s32 sound_id);
s32 main_sound_find_sfx_voice(s32 sound_id, s32 voice_count);
suzuki_music_t* main_sound_init_sfx_music(void);
void main_smd_init_sfx_music_header(suzuki_music_t* music);
void main_sound_start_sfx(s16 channel_id, s32 sound_id, s16 volume, s16 balance);
void main_sound_stop_all(void);

/* Suzuki heap (0x8001423c-0x80014544) and SPU RAM wrappers. */
void main_sound_init_heap(void* arena, u32 size);
void* main_sound_alloc(u32 size);
void main_sound_free(void* payload);
s32 main_sound_get_largest_free_block(void);
void main_sound_copy_memory(void* destination, void* source, s32 size);
void main_sound_clear_memory(void* destination, s32 size);
void main_noop_80014544(void);
s32 main_sound_alloc_spu_ram(s32 size);
s32 main_sound_free_spu_ram(u32 addr);

/* Root-counter tick (0x80014590-0x80015324, 0x80017118-0x800178e4). The
 * per-channel passes take (music, first channel, channel count). */
s32 main_sound_root_counter_2_handler(void);
void main_sound_update_voice_attrs(void);
void main_sound_flush_key_on(void);
void main_sound_flush_key_off(void);
void main_smd_step_ramp(suzuki_ramp_t* ramp);
void main_sound_calculate_pitch_lfo_voices(void);
void main_sound_calculate_noise_voices(void);
void main_sound_calculate_reverb_voices(void);
void main_smd_update_channel_ramps(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void main_smd_read_instructions(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void main_smd_update_modulators(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void main_smd_update_voices(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
s16 main_smd_calculate_pitch(s16 key);
void main_smd_modulator_reset(suzuki_modulator_t* modulator);
void main_smd_modulator_deactivate(suzuki_modulator_t* modulator);
s32 main_smd_modulator_calculate_step(s32 depth, s16 speed, s16 waveform);
s32 main_smd_modulator_step_square(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_alternating(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_triangle(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_triangle_centered(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_sawtooth(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_random(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_random_bipolar(suzuki_modulator_t* modulator);
void main_smd_seed_random(s32 value);
s32 main_smd_random(void);

/* Reprograms an SMD channel with an instrument of its waveset. */
void main_smd_set_instrument(s32 instrument, suzuki_music_channel_t* channel);
void main_smd_set_note_flags2_all_channels(u16 mask, suzuki_music_t* music);
void main_smd_force_channel_func(suzuki_music_t* music, u16 mask);

/* Driver set-up, wavesets, volume and SPU transfers (0x80017920-0x800186c4). */
void main_sound_quit(void);
void main_system_enable_root_counter_2_evcb(void);
void main_system_disable_root_counter_2_evcb(void);
suzuki_waveset_t* main_sound_load_waveset(suzuki_waveset_t* file);
void main_sound_free_wavesets(void);
void main_sound_free_waveset(suzuki_waveset_t* waveset);
suzuki_waveset_t* main_sound_find_waveset(s16 id);
void main_sound_set_type(s32 type);
s32 main_sound_get_type(void);
void main_sound_put_type(s16 level);
void main_sound_set_reverb_mode(s32 mode, s16 depth, s32 delay, s32 feedback);
void main_sound_set_master_volume(s16 volume, s16 time);
void main_sound_set_cd_volume(s16 volume, s16 time);
void main_sound_set_cd_reverb(s32 reverb, s32 mix);
void main_sound_commit_volume_change(void);
void main_sound_set_vol_balance(s32 volume, SpuVolume* volume_out, u8 mode);
void func_800184e0(u16 value);
void main_sound_transfer_spu_data(u32 spu_address, void* data, u32 size, s32 mode);
void main_suzuki_spu_callback_func(void);
s32 main_sound_spu_event_handler(void);
s16 main_sound_wait_for_spu_transfer(s32 flags);

extern s32 g_main_sound_weather_sfx_id;
void main_sound_replay_weather_sfx(void);

#endif
