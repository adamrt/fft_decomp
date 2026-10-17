#ifndef SUZUKI_H
#define SUZUKI_H

/* Reconstructed Suzuki sound driver, linked into the resident executable. */

#include "psx/libcd.h"
#include "psx/libspu.h"
#include "psx/types.h"

typedef struct suzuki_smd_header suzuki_smd_header_t;

/* sequence */
/* Suzuki sound driver (SCUS_942.21 0x800120f4-0x800186c4) records, tables and
 * globals. Offsets and widths are taken from the driver code named beside
 * each field; the driver writes its _unknown_XX members and never reads them. */
/* Linear ramp stepped once per tick by suzuki_smd_step_ramp (0x80014f18):
 * value moves by step until count reaches zero, then snaps to target << 16.
 * The root-counter handler tests count with `lh` before stepping MUS
 * +0x88/+0x94/+0xa0/+0xac and the volume ramps of g_suzuki_sound_spu_state. */
typedef struct suzuki_ramp {
    s32 value;  /* 0x00 */
    s32 step;   /* 0x04 */
    s16 count;  /* 0x08 */
    s16 target; /* 0x0a */
} suzuki_ramp_t;

/* WAVESET instrument entry (0x10 bytes). suzuki_smd_set_instrument
 * (0x80016fb4) and suzuki_smd_get_instrument_attr copy it into a channel or an
 * SpuVoiceAttr. The rates, the level and the modes are LIBSPU envelope
 * arguments. Modes: 1 linear increase, 3 linear decrease, 5 exponential
 * increase, 7 exponential decrease (attack: 5 or linear; sustain: 1, 5, 7 or
 * linear decrease; release: 7 or linear). */
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

/* WAVESET.WD header as loaded. suzuki_sound_load_waveset
 * (0x80017c8c) uploads wave_size bytes from wave_offset to SPU RAM and keeps
 * header_size bytes on the Suzuki heap, linked from g_suzuki_sound_waveset_list
 * through next. Select Sound Font (0x80016e48) matches id. */
typedef struct suzuki_waveset {
    u8 _unused_00[0x10];                /* 0x00; "dwdsP" magic, file size at 0x08 */
    u32 header_size;                    /* 0x10 */
    u32 wave_size;                      /* 0x14 */
    u32 wave_offset;                    /* 0x18 */
    u8 _unused_1c[4];                   /* 0x1c; index of the last instrument (0xaf in WAVESET.WD) */
    u16 id;                             /* 0x20 */
    u8 _unused_22[6];                   /* 0x22 */
    u32 spu_address;                    /* 0x28; SpuMalloc result, freed by suzuki_sound_free_waveset */
    struct suzuki_waveset* next;        /* 0x2c */
    suzuki_instrument_t instruments[0]; /* 0x30 */
} suzuki_waveset_t;

/* SMD music sequence header ("smds"), read by suzuki_smd_transfer_music_data,
 * suzuki_smd_init_channels, suzuki_smd_init_channel_streams and
 * suzuki_smd_get_music_filename. The values noted as "on the disc" hold for
 * all 100 MUSIC_NN.SMD files. */
struct suzuki_smd_header {
    u8 magic[4];            /* 0x00; "smds" */
    u8 _unused_04[4];       /* 0x04; differs in every file */
    u32 size;               /* 0x08; file size in bytes */
    u8 _unused_0c[4];       /* 0x0c */
    u16 id;                 /* 0x10; copied to MUS id and channel sound_id; 0 on the disc */
    u8 _unknown_12;         /* 0x12; copied to MUS _unknown_014; 2 on the disc */
    u8 tick_divisor;        /* 0x13; divides the 48 ticks of a beat for the bar and beat counters; 1 on the disc */
    u8 channel_count;       /* 0x14 */
    u8 _unknown_15;         /* 0x15; copied to MUS _unknown_017; 0 on the disc */
    u16 waveset_id;         /* 0x16; 0 on the disc */
    u16 _unknown_18;        /* 0x18; copied to MUS _unknown_01a; 0x28-0x7f on the disc */
    s8 reverb_mode;         /* 0x1a; 4 on the disc */
    u8 reverb_depth;        /* 0x1b */
    u8 reverb_delay;        /* 0x1c */
    u8 reverb_feedback;     /* 0x1d */
    u16 filename_offset;    /* 0x1e; offset of the zero-terminated name the sequence was built from */
    u16 data_offset;        /* 0x20 */
    u16 channel_offsets[0]; /* 0x22; one stream offset per channel (0: no stream), then a zero */
};

/* Suzuki sequencer modulator (0x20 bytes). Each channel has four, at
 * channel+0xe0. step is one of the g_suzuki_smd_modulator_waveforms functions,
 * called by suzuki_smd_update_modulators (0x8001749c), which adds the scaled
 * result to the channel output selected by target. delay delays the
 * start; while fade_level is below 0x100 the output is scaled by
 * fade_level / 0x100 and fade_level grows by fade_step (fade-in). */
typedef struct suzuki_modulator {
    s32 (*step)(struct suzuki_modulator*); /* 0x00 */
    s32 value;                             /* 0x04 */
    s32 increment;                         /* 0x08 */
    s32 amplitude;                         /* 0x0c; from suzuki_smd_modulator_calculate_step */
    u16 timer;                             /* 0x10 */
    u16 timer_reset;                       /* 0x12 */
    u16 delay;                             /* 0x14; ticks before the start */
    u16 delay_reset;                       /* 0x16; reload of delay */
    s16 fade_level;                        /* 0x18; output scale, 0x100 = full */
    u16 fade_step;                         /* 0x1a; per-tick fade-in increment */
    u8 target;                             /* 0x1c; suzuki_modulator_target_e */
    u8 waveform;                           /* 0x1d; suzuki_modulator_waveform_e */
    s16 flags;                             /* 0x1e; suzuki_modulator_flag_e */
} suzuki_modulator_t;
typedef char suzuki_modulator_size_must_be_32[(sizeof(suzuki_modulator_t) == 32) ? 1 : -1];

/* Dedicated vibrato, tremolo and pan-LFO opcodes use these slots. Modulator
 * Select can configure any slot with a different output target. */
enum {
    SUZUKI_MODULATOR_SLOT_VIBRATO = 0,
    SUZUKI_MODULATOR_SLOT_TREMOLO = 1,
    SUZUKI_MODULATOR_SLOT_PAN_LFO = 2,
};

enum { SUZUKI_MODULATOR_COUNT = 4 };

/* suzuki_modulator_t.target: the channel output suzuki_smd_update_modulators
 * adds the modulator to. */
typedef enum suzuki_modulator_target {
    SUZUKI_MODULATOR_TARGET_PITCH = 0,
    SUZUKI_MODULATOR_TARGET_VOLUME = 1,
    SUZUKI_MODULATOR_TARGET_BALANCE = 2,
} suzuki_modulator_target_e;

/* suzuki_modulator_t.waveform: index into g_suzuki_smd_modulator_waveforms,
 * the low nibble of the waveform byte of the modulator opcodes. Indices
 * 8-15 hold suzuki_smd_modulator_deactivate, which stops the modulator on its
 * first step. suzuki_smd_modulator_calculate_step scales the amplitude of the
 * triangles and of sawtooth 4 only. */
typedef enum suzuki_modulator_waveform {
    SUZUKI_MODULATOR_WAVEFORM_SQUARE = 0,            /* suzuki_smd_modulator_step_square: 0 and +amplitude */
    SUZUKI_MODULATOR_WAVEFORM_ALTERNATING = 1,       /* suzuki_smd_modulator_step_alternating: +/-amplitude */
    SUZUKI_MODULATOR_WAVEFORM_TRIANGLE = 2,          /* suzuki_smd_modulator_step_triangle */
    SUZUKI_MODULATOR_WAVEFORM_TRIANGLE_CENTERED = 3, /* suzuki_smd_modulator_step_triangle_centered */
    SUZUKI_MODULATOR_WAVEFORM_SAWTOOTH = 4,          /* suzuki_smd_modulator_step_sawtooth */
    SUZUKI_MODULATOR_WAVEFORM_SAWTOOTH_UNSCALED = 5, /* the same step function, amplitude not divided */
    SUZUKI_MODULATOR_WAVEFORM_RANDOM = 6,            /* suzuki_smd_modulator_step_random: sample and hold */
    SUZUKI_MODULATOR_WAVEFORM_RANDOM_BIPOLAR = 7,    /* suzuki_smd_modulator_step_random_bipolar */
    SUZUKI_MODULATOR_WAVEFORM_OFF = 8,               /* suzuki_smd_modulator_deactivate, as are 9-15 */
} suzuki_modulator_waveform_e;

/* Vibrato, tremolo and pan-LFO operands with a start delay. */
typedef struct suzuki_smd_modulator_parameters {
    u8 speed;
    s8 depth;
    u8 delay;
} suzuki_smd_modulator_parameters_t;
typedef char suzuki_smd_modulator_parameters_size_must_be_3[sizeof(suzuki_smd_modulator_parameters_t) == 3 ? 1 : -1];

/* Vibrato, tremolo and pan-LFO waveform operands. */
typedef struct suzuki_smd_waveform_parameters {
    u8 speed;
    s8 depth;
    u8 waveform_flags; /* low four bits: waveform; bit 4: no restart on key-on */
} suzuki_smd_waveform_parameters_t;
typedef char suzuki_smd_waveform_parameters_size_must_be_3[(sizeof(suzuki_smd_waveform_parameters_t) == 3) ? 1 : -1];

/* Encoded waveform byte of the D9, E5, ED and F0 SMD opcodes. */
enum {
    SUZUKI_MODULATOR_INPUT_WAVEFORM_MASK = 0x0f,
    SUZUKI_MODULATOR_INPUT_NO_KEY_ON_RESTART = 0x10,
};

/* suzuki_modulator_t.flags. */
typedef enum suzuki_modulator_flag {
    SUZUKI_MODULATOR_ACTIVE = 0x1,
    SUZUKI_MODULATOR_RESTART_ON_KEY_ON = 0x2, /* suzuki_smd_read_instructions restarts it at each key-on */
    SUZUKI_MODULATOR_HALF_PERIOD_DONE = 0x4,  /* centered triangle: later half-periods last twice as long */
    SUZUKI_MODULATOR_NEGATED = 0x8,           /* the next step or amplitude is negated */
} suzuki_modulator_flag_e;

typedef s32 (*suzuki_modulator_step_t)(suzuki_modulator_t* modulator);

/* Repeat stack entry (0xc bytes) pushed by Repeat and consumed by Coda and
 * To Coda. */
typedef struct suzuki_repeat {
    u8 count;       /* 0x00; passes left after the current one */
    u8 _unused_01;  /* 0x01 */
    u8 octave_base; /* 0x02; octave_base at the section start */
    u8 coda_octave; /* 0x03; octave_base at the Coda */
    u8* start;      /* 0x04; first byte of the section */
    u8* coda;       /* 0x08; the byte after the Coda */
} suzuki_repeat_t;

/* Suzuki sequencer channel record (0x160 bytes), the third argument of every
 * SMD opcode handler (note data, music, channel). Field names follow the
 * opcode that writes them. voice_attr at 0x34 is a SpuVoiceAttr; the driver
 * passes its members to LIBSPU one by one.
 *
 * flags holds the 16-bit channel flags (suzuki_channel_flag_e); a channel is
 * allocated while it is non-zero, and suzuki_smd_set_flags_all_channels ORs a
 * mask into every non-zero 16-bit value. voice_update_flags (suzuki_voice_update_e)
 * queues work for suzuki_smd_update_voices, voice_attr_flags
 * (suzuki_voice_attr_e) marks the SPU voice attributes
 * suzuki_sound_update_voice_attrs pushes, and ramp_flags (suzuki_ramp_flag_e)
 * marks the ramps stepped by suzuki_smd_update_channel_ramps.
 *
 * octave_base is written as a 16-bit value (Octave, Raise/Lower Octave, End
 * Bar) but saved into the u8 loop_octave_base by Loop and Loop Select, which
 * read it with `lbu`: GCC narrows the u16 load itself.
 *
 * sound_id is the SMD id for music and a SUZUKI_SOUND_ID (`resource id << 16 |
 * sound`) for sound effects; Play VFX reads the resource id from the high 16 bits with `lh`,
 * the SFX API compares the full 32-bit value. */
typedef struct suzuki_music_channel {
    u16 flags;              /* 0x00; suzuki_channel_flag_e */
    u16 voice_update_flags; /* 0x02; suzuki_voice_update_e */
    u16 voice_attr_flags;   /* 0x04; suzuki_voice_attr_e */
    u16 ramp_flags;         /* 0x06; suzuki_ramp_flag_e */
    s32 sound_id;           /* 0x08; sound resource id in the high 16 bits, sound number in the low 16 bits */
    u8 channel_number;      /* 0x0c; 0xff + channel index (SFX: the index); write-only */
    u8 priority;            /* 0x0d; SFX steal priority (channel id >> 8; stolen when <= 0x20); music: 0x10 */
    u8 _padding_00e[2];     /* 0x0e; aligns start_tick */
    u32 start_tick;         /* 0x10; g_suzuki_sound_tick_count when suzuki_sound_start_sfx started the channel */
    u8* note_data_start;    /* 0x14 */
    u8* note_data;          /* 0x18; read position */
    u8* loop_note_data;     /* 0x1c; loop point set by Loop and Loop Select; End Bar jumps to it */
    u8* restart_note_data;  /* 0x20; set by suzuki_smd_init_channel_streams */
    u32 _unknown_024;       /* 0x24; cleared by both channel initializers, never read */
    u16 loop_count;         /* 0x28; times End Bar has jumped to the loop point */
    u8 _unknown_02a;        /* 0x2a; cleared by both channel initializers, never read */
    u8 loop_octave_base;    /* 0x2b; octave_base saved with loop_note_data */
    u8 instrument;          /* 0x2c; index into waveset->instruments */
    u8 voice;               /* 0x2d; SPU voice (Set Voice); only an odd voice takes Enable LFO */
    u16 note_release_rate;  /* 0x2e; the instrument's or Release Rate's value; each note reloads release_rate from it */
    suzuki_waveset_t* waveset; /* 0x30; Select Sound Font */
    SpuVoiceAttr voice_attr;   /* 0x34; SPU voice, volume, sample addresses and envelope */
    s16 event_ticks;           /* 0x74; ticks left of the current note, Rest or Fermata; 0 reads the next event */
    s8 length_adjust;          /* 0x76; ticks added to each event's length (Length Adjust) */
    u8 _padding_077;           /* 0x77; aligns note_ticks */
    u16 note_ticks;            /* 0x78; ticks until the note is keyed off (SUZUKI_CHANNEL_KEYED_OFF) */
    u16 gate_time;             /* 0x7a; key-on time in sixteenths of the length (Gate Time); initialized to 0xf */
    u8 previous_key;           /* 0x7c; previous note's key, the portamento start; cleared by the channel initializer */
    u8 key;                    /* 0x7d; key number of the current note */
    u16 octave_base;           /* 0x7e; key number of the current octave, octave * 12 */
    s32 pitch;              /* 0x80; 1/256 semitones << 16: each note's key, moved by the pitch slide and portamento */
    s16 fine_tune;          /* 0x84; instrument fine_tune, added to key << 8 (1/256 semitone) */
    s16 pitch_offset;       /* 0x86; detune in 1/256 semitone (the Pitch Offset opcodes), added to each note */
    s16 pitch_modulation;   /* 0x88; modulator target 0 output */
    s16 volume_modulation;  /* 0x8a; modulator target 1 output */
    s16 balance_modulation; /* 0x8c; modulator target 2 output */
    u8 _unused_08e[2];      /* 0x8e */
    u16 portamento_ticks;   /* 0x90; Portamento length; non-zero sets SUZUKI_RAMP_PORTAMENTO */
    s16 balance;            /* 0x92; balance byte << 8 */
    s16 velocity;           /* 0x94; velocity << 8, 0x6000 by default */
    u8 _padding_096[2];     /* 0x96; aligns volume */
    s32 volume;             /* 0x98; volume byte << 24 */
    s32 pitch_slide_step;   /* 0x9c; per-tick pitch delta of Pitch Slide or the portamento */
    s32 volume_step;        /* 0xa0; per-tick volume delta of Volume Ramp */
    s16 balance_step;       /* 0xa4; per-tick balance delta of Balance Ramp */
    u16 pitch_slide_count;  /* 0xa6 */
    u16 volume_step_count;  /* 0xa8 */
    u16 balance_step_count; /* 0xaa */
    u16 repeat_depth;       /* 0xac; -1 when the repeat stack is empty */
    u16 modulator_index;    /* 0xae; modulator selected by Modulator Select */
    suzuki_repeat_t repeats[4];                            /* 0xb0 */
    suzuki_modulator_t modulators[SUZUKI_MODULATOR_COUNT]; /* 0xe0 */
} suzuki_music_channel_t;
typedef char suzuki_music_channel_size_must_be_352[(sizeof(suzuki_music_channel_t) == 352) ? 1 : -1];
typedef char suzuki_channel_voice_attr_must_be_at_offset_0x34
    [((unsigned long)&((suzuki_music_channel_t*)0)->voice_attr == 0x34) ? 1 : -1];
typedef char suzuki_channel_voice_attr_size_must_be_64[(sizeof(SpuVoiceAttr) == 64) ? 1 : -1];

/* suzuki_music_channel_t.flags. */
typedef enum suzuki_channel_flag {
    SUZUKI_CHANNEL_IN_USE = 0x0001,
    /* Set at initialization from SUZUKI_MUSIC_STATUS_IMMEDIATE_INSTRUMENT:
     * suzuki_smd_set_instrument marks the voice attributes at once. */
    SUZUKI_CHANNEL_IMMEDIATE_INSTRUMENT = 0x0004,
    /* Sound-effect channel (suzuki_sound_start_sfx): notes keep the channel
     * velocity, and suzuki_smd_set_instrument marks the voice attributes at once. */
    SUZUKI_CHANNEL_SFX = 0x0008,
    SUZUKI_CHANNEL_NO_RETRIGGER = 0x0010,       /* never set by the driver itself; a held note is not keyed on again */
    SUZUKI_CHANNEL_MUTED = 0x0020,              /* by music->mute_mask; blocks key-on */
    SUZUKI_CHANNEL_NOTE_READ = 0x0080,          /* set with SOUNDING by every note; never tested */
    SUZUKI_CHANNEL_SOUNDING = 0x0100,           /* a note or Fermata holds the channel; ends the tick's opcode reads */
    SUZUKI_CHANNEL_TIED = 0x0200,               /* the gate time does not key the note off */
    SUZUKI_CHANNEL_KEYED_OFF = 0x0400,          /* resting or past the gate time; the next note keys on */
    SUZUKI_CHANNEL_SLUR = 0x0800,               /* Slur On/Off: every note is tied */
    SUZUKI_CHANNEL_NOTE_FOLLOWS = 0x1000,       /* the look-ahead found a note as the next event */
    SUZUKI_CHANNEL_STREAMS_SET = 0x4000,        /* suzuki_smd_init_channel_streams; never tested */
    SUZUKI_CHANNEL_INSTRUMENT_PENDING = 0x8000, /* the next note marks the voice attributes */
} suzuki_channel_flag_e;

/* suzuki_music_channel_t.voice_update_flags: work queued for
 * suzuki_smd_update_voices. */
typedef enum suzuki_voice_update {
    SUZUKI_VOICE_UPDATE_KEY_ON = 0x0001,
    SUZUKI_VOICE_UPDATE_KEY_OFF = 0x0002,
    SUZUKI_VOICE_UPDATE_VOLUME = 0x0100, /* volume and balance */
    SUZUKI_VOICE_UPDATE_PITCH = 0x0200,
} suzuki_voice_update_e;

/* suzuki_music_channel_t.voice_attr_flags: the SPU voice attributes
 * suzuki_sound_update_voice_attrs pushes. PITCH, ATTACK and SUSTAIN double as
 * the marks of the LFO, noise and reverb opcodes, so they also request the
 * voice-mask updates of suzuki_voice_mask_request_e. Callers also set bits
 * 0x7000, which nothing tests. */
typedef enum suzuki_voice_attr {
    SUZUKI_VOICE_ATTR_VOLUME = 0x0001,
    SUZUKI_VOICE_ATTR_VOLUME_MODE = 0x0002, /* volume and volume mode */
    SUZUKI_VOICE_ATTR_PITCH = 0x0004,
    SUZUKI_VOICE_ATTR_ADDRESS = 0x0008, /* start and loop addresses */
    SUZUKI_VOICE_ATTR_ATTACK = 0x0010,  /* time and mode */
    SUZUKI_VOICE_ATTR_DECAY = 0x0020,
    SUZUKI_VOICE_ATTR_SUSTAIN = 0x0040, /* time and mode */
    SUZUKI_VOICE_ATTR_RELEASE = 0x0080, /* time and mode */
    SUZUKI_VOICE_ATTR_SUSTAIN_LEVEL = 0x0100,
    SUZUKI_VOICE_ATTR_ALL = 0x01ff,
} suzuki_voice_attr_e;

/* suzuki_music_channel_t.ramp_flags. */
typedef enum suzuki_ramp_flag {
    SUZUKI_RAMP_PITCH_SLIDE = 0x01,
    SUZUKI_RAMP_PITCH_SLIDE_HOLD = 0x02, /* toggled by Pitch Slide Hold: the slide count does not run down */
    SUZUKI_RAMP_PORTAMENTO = 0x04,       /* Portamento: a key-on slides from the previous key */
    SUZUKI_RAMP_VOLUME = 0x08,
    SUZUKI_RAMP_BALANCE = 0x10,
} suzuki_ramp_flag_e;

/* g_suzuki_sound_voice_update_requests: the voice masks the root-counter
 * handler recalculates. */
typedef enum suzuki_voice_mask_request {
    SUZUKI_VOICE_MASK_REQUEST_PITCH_LFO = 0x1,
    SUZUKI_VOICE_MASK_REQUEST_NOISE = 0x2,
    SUZUKI_VOICE_MASK_REQUEST_REVERB = 0x4,
} suzuki_voice_mask_request_e;

/* SMD instruction bytes. A byte below SUZUKI_SMD_OPCODE_FIRST starts a note:
 * it is the velocity, the next byte indexes g_suzuki_smd_note_semitones and
 * g_suzuki_smd_note_durations, and a length byte follows when that duration
 * is 0. Every other byte is an opcode; g_suzuki_smd_opcode_handlers and
 * g_suzuki_smd_opcode_lengths are indexed by opcode - SUZUKI_SMD_OPCODE_FIRST.
 *
 * Each comment lists the parameter bytes after the opcode. The handler of
 * SUZUKI_SMD_OPCODE_X is suzuki_smd_x unless the comment names another. The 37
 * opcodes without an enumerator share suzuki_smd_no_instruction and have
 * length 0 in g_suzuki_smd_opcode_lengths, so the look-ahead of
 * suzuki_smd_read_instructions would never get past one. */
typedef enum suzuki_smd_opcode {
    SUZUKI_SMD_OPCODE_FIRST = 0x80,
    SUZUKI_SMD_OPCODE_REST = 0x80,    /* ticks */
    SUZUKI_SMD_OPCODE_FERMATA = 0x81, /* ticks: holds the sounding note that much longer */
    SUZUKI_SMD_OPCODE_NOP_8A = 0x8a,  /* nothing; suzuki_smd_opcode_8a */
    SUZUKI_SMD_OPCODE_LOOP_SELECT
    = 0x8d, /* selector: a Loop when it equals music->loop_select; suzuki_smd_loop_point_if_selected */
    SUZUKI_SMD_OPCODE_SKIP_3 = 0x8e,  /* three ignored bytes */
    SUZUKI_SMD_OPCODE_NOP_8F = 0x8f,  /* nothing; suzuki_smd_opcode_8f */
    SUZUKI_SMD_OPCODE_END_BAR = 0x90, /* jumps to the loop point; ends the channel when none is set */
    SUZUKI_SMD_OPCODE_LOOP = 0x91,    /* sets the loop point */
    SUZUKI_SMD_OPCODE_OCTAVE = 0x94,  /* octave */
    SUZUKI_SMD_OPCODE_RAISE_OCTAVE = 0x95,
    SUZUKI_SMD_OPCODE_LOWER_OCTAVE = 0x96,
    SUZUKI_SMD_OPCODE_TIME_SIGNATURE = 0x97, /* beats per bar, beat unit */
    SUZUKI_SMD_OPCODE_REPEAT = 0x98,         /* pass count: opens a repeated section */
    SUZUKI_SMD_OPCODE_CODA = 0x99,           /* closes the section: jumps back while passes remain */
    SUZUKI_SMD_OPCODE_TO_CODA = 0x9a,        /* on the last pass, jumps past the section's Coda */
    SUZUKI_SMD_OPCODE_PLAY_SFX = 0x9c,       /* sound id low, high, volume */
    /* Sound id low, high. g_suzuki_smd_opcode_lengths counts three parameter
     * bytes, one more than the handler consumes. */
    SUZUKI_SMD_OPCODE_STOP_SFX = 0x9d,
    SUZUKI_SMD_OPCODE_PLAY_VFX = 0x9e,           /* sound low, high, stream: continues in a loaded sound resource */
    SUZUKI_SMD_OPCODE_TEMPO = 0xa0,              /* tempo: 75/64 quarter notes a minute per unit */
    SUZUKI_SMD_OPCODE_ACCELERANDO = 0xa1,        /* signed tempo delta */
    SUZUKI_SMD_OPCODE_TEMPO_RAMP = 0xa2,         /* ticks, target tempo */
    SUZUKI_SMD_OPCODE_UNKNOWN_A4 = 0xa4,         /* value: sets music->_unknown_01c; suzuki_smd_opcode_a4 */
    SUZUKI_SMD_OPCODE_UNKNOWN_A5 = 0xa5,         /* delta: adds to music->_unknown_01c; suzuki_smd_opcode_a5 */
    SUZUKI_SMD_OPCODE_MASTER_VOLUME = 0xa6,      /* volume of the whole music */
    SUZUKI_SMD_OPCODE_MASTER_VOLUME_RAMP = 0xa7, /* time in 32/240 s units, target volume */
    SUZUKI_SMD_OPCODE_GATE_TIME = 0xa9,  /* sixteenths of each note's length that stay keyed on; suzuki_smd_gate_time */
    SUZUKI_SMD_OPCODE_SET_VOICE = 0xaa,  /* SPU voice number */
    SUZUKI_SMD_OPCODE_INSTRUMENT = 0xac, /* instrument of the channel's waveset */
    SUZUKI_SMD_OPCODE_LENGTH_ADJUST
    = 0xad,                            /* signed ticks added to length_adjust, 0 clears it; suzuki_smd_length_adjust */
    SUZUKI_SMD_OPCODE_NOP_AE = 0xae,   /* nothing; suzuki_smd_opcode_ae */
    SUZUKI_SMD_OPCODE_NOP_AF = 0xaf,   /* nothing; suzuki_smd_opcode_af */
    SUZUKI_SMD_OPCODE_SLUR_ON = 0xb0,  /* every note is tied to the next; suzuki_smd_slur_on */
    SUZUKI_SMD_OPCODE_SLUR_OFF = 0xb1, /* suzuki_smd_slur_off */
    SUZUKI_SMD_OPCODE_ENABLE_LFO = 0xb2, /* SPU pitch LFO on the channel's voice (odd voices only) */
    SUZUKI_SMD_OPCODE_DISABLE_LFO = 0xb3,
    SUZUKI_SMD_OPCODE_ENABLE_NOISE = 0xb4,   /* noise clock */
    SUZUKI_SMD_OPCODE_INCREASE_NOISE = 0xb5, /* noise clock delta, modulo 64 */
    SUZUKI_SMD_OPCODE_TURN_ON_NOISE = 0xb6,  /* noise with the clock left as it is */
    SUZUKI_SMD_OPCODE_DISABLE_NOISE = 0xb7,
    SUZUKI_SMD_OPCODE_ACOUSTICS = 0xb8, /* reverb depth, delay, feedback */
    SUZUKI_SMD_OPCODE_ENABLE_REVERB = 0xba,
    SUZUKI_SMD_OPCODE_DISABLE_REVERB = 0xbb,
    SUZUKI_SMD_OPCODE_NATURALE = 0xc0,           /* reloads the channel's instrument */
    SUZUKI_SMD_OPCODE_ENVELOPE_SHAPE = 0xc1,     /* attack, sustain and release modes */
    SUZUKI_SMD_OPCODE_ATTACK_RATE = 0xc2,        /* rate; suzuki_smd_attack_time */
    SUZUKI_SMD_OPCODE_DECAY_RATE = 0xc3,         /* rate; suzuki_smd_decay_time */
    SUZUKI_SMD_OPCODE_SUSTAIN_RATE = 0xc4,       /* rate; suzuki_smd_sustain_time */
    SUZUKI_SMD_OPCODE_RELEASE_RATE = 0xc5,       /* rate; suzuki_smd_release */
    SUZUKI_SMD_OPCODE_SUSTAIN_LEVEL = 0xc6,      /* level */
    SUZUKI_SMD_OPCODE_ENVELOPE_TAIL = 0xc7,      /* decay rate, sustain level */
    SUZUKI_SMD_OPCODE_ATTACK_MODE = 0xc8,        /* SPU envelope mode */
    SUZUKI_SMD_OPCODE_SUSTAIN_MODE = 0xc9,       /* SPU envelope mode */
    SUZUKI_SMD_OPCODE_RELEASE_MODE = 0xca,       /* SPU envelope mode */
    SUZUKI_SMD_OPCODE_PITCH_OFFSET = 0xd0,       /* signed, 1/8 semitone; suzuki_smd_pitch_offset */
    SUZUKI_SMD_OPCODE_SHIFT_PITCH_OFFSET = 0xd1, /* signed delta, 1/8 semitone; suzuki_smd_add_pitch_offset_coarse */
    SUZUKI_SMD_OPCODE_SHIFT_PITCH_OFFSET_FINE
    = 0xd2, /* signed delta, 1/32 semitone; suzuki_smd_add_pitch_offset_fine */
    /* Signed 16-bit delta in 1/256 semitone, high byte first; suzuki_smd_add_pitch_offset_wide. */
    SUZUKI_SMD_OPCODE_SHIFT_PITCH_OFFSET_WORD = 0xd3,
    SUZUKI_SMD_OPCODE_PITCH_SLIDE = 0xd4, /* ticks, signed semitones */
    SUZUKI_SMD_OPCODE_PITCH_SLIDE_HOLD
    = 0xd5,                              /* toggles SUZUKI_RAMP_PITCH_SLIDE_HOLD; suzuki_smd_toggle_pitch_slide_hold */
    SUZUKI_SMD_OPCODE_PORTAMENTO = 0xd6, /* ticks a key-on slides from the previous key, 0 off; suzuki_smd_portamento */
    /* The vibrato opcodes program modulator 0 on the pitch; their handlers
     * are suzuki_smd_pitch_shift_fade_in, suzuki_smd_pitch_shift,
     * suzuki_smd_pitch_shift_waveform, suzuki_smd_pitch_shift_on and
     * suzuki_smd_pitch_shift_off. */
    SUZUKI_SMD_OPCODE_VIBRATO_FADE_IN = 0xd7,  /* fade-in length in ticks, less one */
    SUZUKI_SMD_OPCODE_VIBRATO = 0xd8,          /* speed, signed depth, delay: a centered triangle */
    SUZUKI_SMD_OPCODE_VIBRATO_WAVEFORM = 0xd9, /* speed, signed depth, waveform (bit 4: no restart on key-on) */
    SUZUKI_SMD_OPCODE_VIBRATO_ON = 0xda,
    SUZUKI_SMD_OPCODE_VIBRATO_OFF = 0xdb,
    SUZUKI_SMD_OPCODE_PITCH_SLIDE_OFF = 0xdc, /* suzuki_smd_pitch_slide_off */
    SUZUKI_SMD_OPCODE_DYNAMIC = 0xe0,         /* channel volume */
    SUZUKI_SMD_OPCODE_CRESCENDO = 0xe1,       /* signed volume delta */
    SUZUKI_SMD_OPCODE_VOLUME_RAMP = 0xe2,     /* ticks, target volume; suzuki_smd_fermata_ramp */
    /* The tremolo opcodes program modulator 1 on the volume. */
    SUZUKI_SMD_OPCODE_TREMOLO_FADE_IN = 0xe3,  /* fade-in length in ticks, less one */
    SUZUKI_SMD_OPCODE_TREMOLO = 0xe4,          /* speed, signed depth, delay: a triangle */
    SUZUKI_SMD_OPCODE_TREMOLO_WAVEFORM = 0xe5, /* speed, signed depth, waveform (bit 4: no restart on key-on) */
    SUZUKI_SMD_OPCODE_TREMOLO_ON = 0xe6,
    SUZUKI_SMD_OPCODE_TREMOLO_OFF = 0xe7,
    SUZUKI_SMD_OPCODE_BALANCE = 0xe8,       /* balance: 0 left, 0x40 center, 0x7f right */
    SUZUKI_SMD_OPCODE_SHIFT_BALANCE = 0xe9, /* signed balance delta */
    SUZUKI_SMD_OPCODE_BALANCE_RAMP = 0xea,  /* ticks, target balance */
    /* The pan LFO opcodes program modulator 2 on the balance. */
    SUZUKI_SMD_OPCODE_PAN_LFO_FADE_IN = 0xeb,  /* fade-in length in ticks, less one */
    SUZUKI_SMD_OPCODE_PAN_LFO = 0xec,          /* speed, signed depth, delay: a centered triangle */
    SUZUKI_SMD_OPCODE_PAN_LFO_WAVEFORM = 0xed, /* speed, signed depth, waveform (bit 4: no restart on key-on) */
    SUZUKI_SMD_OPCODE_PAN_LFO_ON = 0xee,
    SUZUKI_SMD_OPCODE_PAN_LFO_OFF = 0xef,
    /* Modulator index, waveform (bit 4: no restart on key-on), target: selects
     * the modulator the next two opcodes program. */
    SUZUKI_SMD_OPCODE_MODULATOR_SELECT = 0xf0,
    SUZUKI_SMD_OPCODE_MODULATOR_SET_DEPTH = 0xf1,   /* speed, signed depth high, depth low */
    SUZUKI_SMD_OPCODE_MODULATOR_SET_FADE_IN = 0xf2, /* delay, fade-in length in ticks less one */
    SUZUKI_SMD_OPCODE_MODULATOR_ENABLE_MASK = 0xf5, /* bit n: modulator n restarts and runs, clear stops it */
    SUZUKI_SMD_OPCODE_MODULATOR_ENABLE = 0xf6,      /* modulator index */
    SUZUKI_SMD_OPCODE_MODULATOR_DISABLE = 0xf7,     /* modulator index */
    SUZUKI_SMD_OPCODE_SCALE_TEMPO = 0xfd,           /* tempo multiplier, 0 ignored */
    SUZUKI_SMD_OPCODE_SELECT_SOUND_FONT = 0xfe,     /* waveset id */
    SUZUKI_SMD_OPCODE_END_TRACK = 0xff,             /* ends the channel once its voice's envelope has run out */
} suzuki_smd_opcode_e;

/* Suzuki MUS (music sequence) record: 0xb8 bytes followed by channel_count
 * channels, allocated by suzuki_smd_create_music. It is the second argument of every
 * SMD opcode handler. Queued records form a linked list through next, headed
 * by g_suzuki_sound_active_music_list; snapshot chains saved copies of the
 * whole record (suzuki_smd_save_snapshot) through their own snapshot fields.
 *
 * status (suzuki_music_status_e) has bit 15 set while the music plays
 * (suzuki_smd_is_music_playing); suzuki_sound_calculate_reverb_voices tests that bit
 * as a sign (`lh` and `bgez`), and bit 0 there marks the reverb voices as
 * excludable.
 *
 * tempo is a signed 16.16 value. Scale Tempo reads its integer part; Tempo,
 * Accelerando and Tempo Ramp update the full 32-bit value. */
typedef struct suzuki_music {
    struct suzuki_music* next;     /* 0x00 */
    struct suzuki_music* snapshot; /* 0x04; freed by suzuki_smd_free_snapshots */
    suzuki_smd_header_t* smd;      /* 0x08 */
    u8 _unused_00c[4];             /* 0x0c */
    s16 status;                    /* 0x10; suzuki_music_status_e */
    u16 id;                        /* 0x12; SMD id */
    u8 _unknown_014;               /* 0x14; SMD _unknown_12, 1 for the SFX record */
    u8 tick_divisor;               /* 0x15 */
    u8 channel_count;              /* 0x16 */
    u8 _unknown_017;               /* 0x17; SMD _unknown_15, 0 for the SFX record */
    s16 waveset_id;                /* 0x18; sound font id */
    u16 _unknown_01a;              /* 0x1a; SMD _unknown_18, 0x7f for SFX */
    u8 _unknown_01c;               /* 0x1c; set by opcode 0xA4, added to by 0xA5, cleared by the header reset */
    u8 loop_select;                /* 0x1d; the Loop Select opcode whose byte equals it sets the loop point */
    u16 noise_clock;               /* 0x1e; SpuSetNoiseClock value (Enable Noise, Increase Noise) */
    s32 timer_ticks;               /* 0x20; incremented every tick */
    u32 sequence_ticks;            /* 0x24; ticks while the record owns voices */
    u32 ticks;                     /* 0x28; root-counter ticks played, in 1/256 units (tempo scale) */
    u32 snapshot_restore_tick;     /* 0x2c; restore the snapshot when sequence_ticks reaches it (0: never) */
    u16 min_loop_count;            /* 0x30; suzuki_smd_get_min_loop_count */
    u16 bar;                       /* 0x32; 1-based bar counter */
    u16 beat;                      /* 0x34; beat within the bar */
    u16 beat_ticks_remaining;      /* 0x36; ticks left in the beat */
    u16 beats_per_bar;             /* 0x38 */
    u16 ticks_per_beat;            /* 0x3a */
    u16 beat_unit;                 /* 0x3c */
    u16 beats_per_bar_copy;        /* 0x3e; write-only */
    u8 _unused_040[4];             /* 0x40 */
    s32 reverb_mode;               /* 0x44 */
    s16 reverb_depth;              /* 0x48; depth byte << 8 */
    u8 _padding_04a[2];            /* 0x4a; aligns reverb_delay */
    s32 reverb_delay;              /* 0x4c */
    s32 reverb_feedback;           /* 0x50 */
    u16 stop_bar;                  /* 0x54; suzuki_smd_set_stop_bar: the music stops when bar reaches it */
    u8 _padding_056[2];            /* 0x56; aligns channel_mask */
    u32 channel_mask;              /* 0x58; bit n: channel n has note data and is still running */
    u32 mute_mask;                 /* 0x5c; muted channels */
    u32 key_on_mask;               /* 0x60 */
    u32 key_off_mask;              /* 0x64 */
    u32 lfo_voice_mask;            /* 0x68 */
    u32 noise_voice_mask;          /* 0x6c */
    u32 reverb_voice_mask;         /* 0x70 */
    s32 tempo_accumulator;         /* 0x74; one sequencer tick per 0x10000 of scaled_tempo; initialized to 0x10000 */
    s32 scaled_tempo;              /* 0x78; tempo * tempo_scale */
    s32 tempo;                     /* 0x7c; 16.16 tempo advanced by Tempo Ramp */
    s32 tempo_step;                /* 0x80; per-tick tempo delta of Tempo Ramp */
    u16 tempo_step_count;          /* 0x84 */
    u16 tempo_target;              /* 0x86; tempo byte Tempo Ramp ends on */
    suzuki_ramp_t tempo_scale;     /* 0x88; value is scale byte << 24, its high 16 bits the multiplier */
    suzuki_ramp_t master_volume;   /* 0x94; value is volume byte << 24 */
    suzuki_ramp_t pitch_shift;     /* 0xa0 */
    suzuki_ramp_t balance_shift;   /* 0xac */
    suzuki_music_channel_t channels[0]; /* 0xb8; channel_count entries */
} suzuki_music_t;
typedef char suzuki_music_header_size_must_be_184[(sizeof(suzuki_music_t) == 184) ? 1 : -1];

/* suzuki_music_t.status. */
typedef enum suzuki_music_status {
    SUZUKI_MUSIC_STATUS_SMD = 0x0001,            /* the record plays SMD music */
    SUZUKI_MUSIC_STATUS_SFX = 0x0002,            /* the sound-effect record */
    SUZUKI_MUSIC_STATUS_SNAPSHOT = 0x0010,       /* a saved copy exists */
    SUZUKI_MUSIC_STATUS_NO_VOICE_ATTRS = 0x0020, /* never set; suzuki_sound_update_voice_attrs skips the record */
    SUZUKI_MUSIC_STATUS_PAUSED = 0x0100,         /* suzuki_smd_pause_music; cleared on resume */
    /* Never set; suzuki_smd_init_channels gives every channel
     * SUZUKI_CHANNEL_IMMEDIATE_INSTRUMENT. */
    SUZUKI_MUSIC_STATUS_IMMEDIATE_INSTRUMENT = 0x2000,
    SUZUKI_MUSIC_STATUS_FULL_VOICE_UPDATE = 0x4000, /* push every voice attribute once */
    SUZUKI_MUSIC_STATUS_PLAYING = 0x8000,
} suzuki_music_status_e;

/* Loaded sound resource (a "feds" block: SYSTEM.SED, ENV.SED, or the block
 * inside an effect file or WLDCORE.BIN), kept by the Suzuki driver in a
 * singly linked list through next (suzuki_sound_append_resource). suzuki_sound_start_sfx
 * and Play VFX (0x80015c38) match id and index channel_offsets. Each sound
 * has two streams, so one sound plays on a pair of SFX channels. */
typedef struct suzuki_sound_resource {
    u8 _unused_00[0xa];                 /* 0x00; "feds", the size at 0x04, the sound count at 0x08 */
    u16 id;                             /* 0x0a; the high half of a sound id */
    u16 volume_offset;                  /* 0x0c; offset of the per-sound u8 volume table (0x80 = full) */
    u8 _padding_0e[2];                  /* 0x0e; aligns next */
    struct suzuki_sound_resource* next; /* 0x10 */
    u16 channel_offsets[0];             /* 0x14; two stream offsets per sound, 0 for no stream */
} suzuki_sound_resource_t;

/* Driver SPU state at 0x80037020: the common attributes passed to LIBSPU,
 * the volumes they and g_suzuki_sound_reverb_attr (0x80037008) are built from,
 * and two volume ramps stepped by the root-counter handler (0x800149dc). The
 * reverb attributes are a separate object: every user addresses them from
 * their own base, not from this record's. */
typedef struct suzuki_spu_state {
    SpuCommonAttr common;     /* 0x00 0x80037020 */
    s16 music_volume;         /* 0x28 0x80037048; applied to common.mvol */
    s16 cd_volume;            /* 0x2a 0x8003704a; applied to common.cd.volume */
    s16 reverb_depth;         /* 0x2c 0x8003704c; applied to g_suzuki_sound_reverb_attr.depth */
    s16 cd_mix_level;         /* 0x2e 0x8003704e; the CdMix level last given to suzuki_sound_put_type */
    suzuki_ramp_t music_ramp; /* 0x30 0x80037050 */
    suzuki_ramp_t cd_ramp;    /* 0x3c 0x8003705c */
} suzuki_spu_state_t;
typedef char suzuki_spu_state_size_must_be_72[(sizeof(suzuki_spu_state_t) == 72) ? 1 : -1];

/* g_suzuki_sound_driver_flags. */
typedef enum suzuki_sound_driver_flag {
    SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT = 0x0001, /* the root-counter-2 event is enabled */
    /* An SPU RAM transfer is in progress; the transfer callback clears it
     * together with SPU_WRITE and SPU_READ. */
    SUZUKI_SOUND_DRIVER_SPU_TRANSFER = 0x0010,
    SUZUKI_SOUND_DRIVER_SPU_WRITE = 0x0020,
    SUZUKI_SOUND_DRIVER_SPU_READ = 0x0040,
    SUZUKI_SOUND_DRIVER_STEREO = 0x0100,        /* every sound type but mono */
    SUZUKI_SOUND_DRIVER_WIDE = 0x0200,          /* SUZUKI_SOUND_TYPE_WIDE */
    SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE = 0x0400, /* SUZUKI_SOUND_TYPE_WIDE_OPPOSITE */
    SUZUKI_SOUND_DRIVER_SFX_ENABLED = 0x1000,   /* suzuki_sound_set_sfx_enabled */
    /* Only the flags argument of suzuki_sound_init can set it (the game
     * passes 0): start-up and suzuki_sound_set_type also call suzuki_sound_put_type. */
    SUZUKI_SOUND_DRIVER_PUT_TYPE = 0x2000,
    SUZUKI_SOUND_DRIVER_INITIALIZED = 0x8000,
} suzuki_sound_driver_flag_e;

/* Output sound type of suzuki_sound_set_type and suzuki_sound_get_type (which
 * reports both wide types as WIDE). */
typedef enum suzuki_sound_type {
    SUZUKI_SOUND_TYPE_MONO = 0,
    SUZUKI_SOUND_TYPE_STEREO = 1,
    SUZUKI_SOUND_TYPE_WIDE = 2,          /* one side phase-inverted */
    SUZUKI_SOUND_TYPE_WIDE_OPPOSITE = 3, /* the other side inverted; no menu offers it */
} suzuki_sound_type_e;

/* Low nibble of the mode argument of suzuki_sound_transfer_spu_data. */
typedef enum suzuki_sound_spu_transfer {
    SUZUKI_SOUND_SPU_TRANSFER_WRITE = 1,
    SUZUKI_SOUND_SPU_TRANSFER_READ = 2,
    SUZUKI_SOUND_SPU_TRANSFER_READ_DECODED = 3,    /* SpuReadDecodedData, every buffer */
    SUZUKI_SOUND_SPU_TRANSFER_READ_DECODED_CD = 4, /* SpuReadDecodedData, CD buffers only */
    SUZUKI_SOUND_SPU_TRANSFER_NONE = 5,
} suzuki_sound_spu_transfer_e;

enum {
    SUZUKI_SOUND_SPU_TRANSFER_MODE_MASK = 0x0f,
    /* Flag of suzuki_sound_wait_for_spu_transfer. It is also accepted in a
     * transfer mode, whose low nibble alone selects the operation. */
    SUZUKI_SOUND_SPU_TRANSFER_WAIT = 0x10,
};

/* Mode argument of suzuki_smd_dispatch_snapshot. */
typedef enum suzuki_snapshot_mode {
    SUZUKI_SNAPSHOT_DISCARD = 0,
    SUZUKI_SNAPSHOT_SAVE = 1,
    SUZUKI_SNAPSHOT_RESTORE = 2,
} suzuki_snapshot_mode_e;

/* SMD opcode handler, called through g_suzuki_smd_opcode_handlers with the
 * byte after the opcode; it returns the next read position. */
typedef u8* (*suzuki_smd_handler_t)(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);

/* SMD and WD resources store their total byte size at offset eight. */
typedef struct suzuki_resource_prefix {
    u8 _unused_00[8];
    u32 size_bytes; /* 0x08 */
} suzuki_resource_prefix_t;

/* The transfer controller indexes these records in 16-byte steps. */
typedef struct suzuki_spu_transfer_status {
    s16 status; /* 0x00 */
    u8 _unused_02[14];
} suzuki_spu_transfer_status_t;

/* Elapsed play time filled by suzuki_smd_get_play_time. The root counter runs
 * at 240 Hz; music->ticks counts those ticks in 1/256 units. */
typedef struct suzuki_play_time {
    u32 sequence_ticks; /* 0x00; music->sequence_ticks */
    u16 fraction;       /* 0x04; 1/240 s */
    u16 seconds;        /* 0x06 */
    u16 minutes;        /* 0x08 */
} suzuki_play_time_t;

/* sfx */
/* Steal priority of an SFX channel, the high byte of a suzuki_sound_start_sfx
 * channel id. When no channel pair is free, suzuki_sound_find_sfx_channel takes
 * the oldest busy one whose priority is at most STEALABLE. */
typedef enum suzuki_sound_sfx_priority {
    SUZUKI_SOUND_SFX_PRIORITY_STEALABLE = 0x20, /* a sound on a searched or caller-named channel pair */
    SUZUKI_SOUND_SFX_PRIORITY_BACKGROUND
    = 0x60, /* suzuki_sound_play_background_sfx (sound_play_background), channels 4-5 */
    SUZUKI_SOUND_SFX_PRIORITY_DIRECT = 0x80, /* suzuki_sound_play_direct_sfx (sound_play_sfx), channels 6-7 */
} suzuki_sound_sfx_priority_e;

#define SUZUKI_SOUND_ID(resource, sound) (((resource) << 16) | (sound))

#define SUZUKI_SOUND_SFX_CHANNEL_ID(priority, channel) ((s16)(((priority) << 8) | (channel)))

/* heap */
enum { SUZUKI_SOUND_HEAP_BYTE_COUNT = 32768 };

/* Suzuki CPU arena allocator; shared with the sound driver. */
/* Suzuki heap block header (0x10 bytes; payload follows). The $gp-relative
 * allocator (0x8001423c-0x8001442c) rounds requests to 16 bytes and chains
 * blocks in address order from g_suzuki_sound_heap_blocks. */
typedef struct suzuki_heap_block {
    u16 flags;                      /* 0x00; suzuki_heap_block_flag_e; written, never tested */
    u16 _unknown_02;                /* 0x02; cleared on allocation, never read */
    u32 _unknown_04;                /* 0x04; cleared on allocation, never read */
    u8* end;                        /* 0x08; end of the payload */
    struct suzuki_heap_block* next; /* 0x0c */
} suzuki_heap_block_t;

/* suzuki_heap_block_t.flags. */
typedef enum suzuki_heap_block_flag {
    SUZUKI_HEAP_BLOCK_ALLOCATED = 0x0002,
    SUZUKI_HEAP_BLOCK_HEAD = 0x8000, /* the list head suzuki_sound_init_heap places at the arena start */
} suzuki_heap_block_flag_e;

extern suzuki_heap_block_t* g_suzuki_sound_heap_arena;              /* 0x800329f8 */
extern suzuki_heap_block_t* g_suzuki_sound_heap_blocks;             /* 0x800329fc */
extern u32 g_suzuki_sound_heap_size;                                /* 0x80032a38 */
extern u8* g_suzuki_sound_heap_end;                                 /* 0x80032a64 */
extern u8 g_suzuki_sound_heap_memory[SUZUKI_SOUND_HEAP_BYTE_COUNT]; /* the CPU heap arena */

void suzuki_sound_init_heap(void* arena, u32 size);
void* suzuki_sound_alloc(u32 size);
void suzuki_sound_free(void* payload);
s32 suzuki_sound_get_largest_free_block(void);
void suzuki_sound_clear_memory(void* destination, s32 size);

/* driver */
enum {
    SUZUKI_SOUND_SPU_ALLOC_BLOCK_CAPACITY = 6,
    SUZUKI_SOUND_SPU_ALLOC_RECORD_COUNT = SUZUKI_SOUND_SPU_ALLOC_BLOCK_CAPACITY + 1,
};

/* Driver tables (read-only data 0x80028b0c-0x8002a8d8). */
extern suzuki_smd_handler_t g_suzuki_smd_opcode_handlers[0x80]; /* 0x80028b0c; opcodes 0x80-0xff */
extern u8 g_suzuki_smd_opcode_lengths[0x80];                    /* 0x80028d0c; opcode plus parameters */
/* 0x80028d8c; ticks per note byte. Each of the twelve semitones has the same
 * 19 lengths: 0 (a length byte follows), then 192 (a whole note), 144, 96,
 * 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3 and 2. */
extern u8 g_suzuki_smd_note_durations[228];
extern u8 g_suzuki_smd_note_semitones[228];                          /* 0x80028e70; note byte / 19 */
extern suzuki_modulator_step_t g_suzuki_smd_modulator_waveforms[16]; /* 0x80028f54 */
extern u8 g_suzuki_smd_key_octaves[120];                             /* 0x80028fe8; key / 12 */
extern u8 g_suzuki_smd_key_semitones[120];                           /* 0x80029060; key % 12 */

/* 0x800290d8: SPU pitch for semitone s and fine step f at index (s << 8) + f
 * (octave 6 reference); suzuki_smd_calculate_pitch indexes it flat. */
extern u16 g_suzuki_smd_pitch_table[12 * 256];

/* Driver globals (0x800329f0-0x80032a68, gp = 0x800329bc). The heap globals
 * are only ever reached $gp-relative, by 0x8001423c-0x8001442c. */
extern s16
    g_suzuki_sound_sfx_channel_count; /* channels suzuki_sound_start_sfx fills, one per stream of a sound: always 2 */
extern suzuki_sound_resource_t* g_suzuki_sound_resource_list; /* 0x80032a00 */
extern suzuki_spu_transfer_status_t* g_suzuki_sound_spu_transfer_status_records;
extern u32 g_suzuki_sound_music_key_off_voices;      /* voices queued for key-off by stopped music */
extern u32 g_suzuki_sound_sfx_reserved_voices;       /* 0x80032a0c; voices reserved by SFX */
extern u32 g_suzuki_sound_sfx_restart_channels;      /* last SFX voice search: channel mask */
extern u32 g_suzuki_sound_sfx_restart_voices;        /* last SFX voice search: voice mask */
extern s32 g_suzuki_smd_random_state;                /* 0x80032a18 */
extern s16 g_suzuki_sound_sfx_instrument;            /* instrument every started SFX channel begins with; always 0 */
extern u32 g_suzuki_sound_sfx_key_off_voices;        /* SFX voices pending key-off */
extern u32 g_suzuki_sound_tick_count;                /* root-counter tick count; odd ticks step the ramps */
extern u16 g_suzuki_sound_spu_transfer_status_index; /* index into g_suzuki_sound_spu_transfer_status_records */
extern CdlATV g_suzuki_sound_cd_mix;                 /* 0x80032a3c; written by Put Sound Type */

extern s16 g_suzuki_sound_decoded_data_result;        /* SPU transfer controller state */
extern suzuki_waveset_t* g_suzuki_sound_waveset_list; /* 0x80032a44 */
extern s32 g_suzuki_sound_spu_event;             /* 0x80032a48; OpenEvent handle of suzuki_sound_spu_event_handler */
extern u16 g_suzuki_sound_voice_update_requests; /* suzuki_voice_mask_request_e */
extern suzuki_music_t* g_suzuki_sound_active_music_list; /* 0x80032a50 */

/* Driver status (suzuki_sound_driver_flag_e). */
extern s16 g_suzuki_sound_driver_flags;
extern s32
    g_suzuki_sound_root_counter_2_event; /* 0x80032a5c; OpenEvent handle of suzuki_sound_root_counter_2_handler */
extern suzuki_music_t* g_suzuki_sound_sfx_music;    /* 0x80032a60; eight channels on SPU voices 16-23 */
extern SpuReverbAttr g_suzuki_sound_reverb_attr;    /* 0x80037008 */
extern suzuki_spu_state_t g_suzuki_sound_spu_state; /* 0x80037020 */
/* Allocation records and one trailing free-range record. */
extern psyq_spu_heap_record_t g_suzuki_sound_spu_malloc_table[SUZUKI_SOUND_SPU_ALLOC_RECORD_COUNT];
typedef char suzuki_spu_allocator_storage_size_must_be_56[sizeof(g_suzuki_sound_spu_malloc_table) == 56 ? 1 : -1];
extern u16 g_suzuki_sound_unread_value; /* only ever stored */
extern u32 g_suzuki_unknown_80032a34;   /* only ever stored */
extern s16 g_suzuki_unknown_80032a58;   /* only ever stored */

/* Suzuki driver entry points. The music handle passed by the game wrappers
 * is the suzuki_music_t* that suzuki_smd_create_music returns. */
void suzuki_sound_append_resource(suzuki_sound_resource_t* resource);
void suzuki_sound_remove_resource(suzuki_sound_resource_t* resource);
void suzuki_smd_set_music_volume(suzuki_music_t* music, s16 volume, s16 time);
void suzuki_smd_stop_music(suzuki_music_t* music);
u32 suzuki_smd_get_active_voice_mask(suzuki_music_t* music);
u32 suzuki_smd_is_music_playing(suzuki_music_t* music);
suzuki_music_t* suzuki_smd_create_music(suzuki_smd_header_t* smd);
void suzuki_sound_play_direct_sfx(int sound_id);
void suzuki_sound_play_background_sfx(int sound_id);
void suzuki_sound_play_sfx(s32 sound_id);
void suzuki_sound_set_sfx_volume(int sound_id, s16 volume);
void suzuki_sound_init(s32 flags);
void suzuki_sound_stop_all_sfx(void);
void suzuki_smd_unload_music(suzuki_music_t* music);

/* MUS lifecycle, playback control and queue (0x800120f4-0x80014200). */
void suzuki_smd_reset_music(suzuki_music_t* music, s16 volume, s16 fade);
void suzuki_smd_resume_music(suzuki_music_t* music, s16 volume, s16 fade);
void suzuki_smd_pause_music(suzuki_music_t* music);
void suzuki_smd_stop_marked_music(void);
void suzuki_smd_init_channel_streams(suzuki_music_t* music, suzuki_smd_header_t* smd);
void suzuki_smd_set_tempo_scale(suzuki_music_t* music, s16 value, s16 time);
void suzuki_smd_set_pitch_shift(suzuki_music_t* music, s16 value, s16 time);
void suzuki_smd_set_balance_shift(suzuki_music_t* music, s16 value, s16 time);
void suzuki_smd_set_channel_mute_mask(suzuki_music_t* music, u32 mask);
void suzuki_smd_set_channel_select(suzuki_music_t* music, u8 value);
void suzuki_smd_get_play_time(suzuki_music_t* music, suzuki_play_time_t* time);
u16* suzuki_smd_get_min_loop_count(suzuki_music_t* music);
char* suzuki_smd_get_music_filename(suzuki_music_t* music);
void suzuki_smd_dispatch_snapshot(suzuki_music_t* music, s32 mode);
void suzuki_smd_discard_snapshot(suzuki_music_t* music);
void suzuki_smd_save_snapshot(suzuki_music_t* music);
void suzuki_smd_restore_snapshot(suzuki_music_t* music);
void suzuki_smd_set_stop_bar(suzuki_music_t* music, u16 value);
u32 suzuki_smd_retrigger_held_voices(suzuki_music_t* music);
u32 suzuki_smd_get_held_voices(suzuki_music_t* music);
void suzuki_smd_transfer_music_data(suzuki_music_t* music);
void suzuki_smd_init_music_header(suzuki_music_t* music);
void suzuki_smd_init_channels(suzuki_music_t* music);
void suzuki_smd_free_snapshots(suzuki_music_t* music);
void suzuki_smd_copy_snapshot(suzuki_music_t* music, suzuki_music_t* snapshot);
void suzuki_smd_insert_music(suzuki_music_t* music);
s32 suzuki_smd_remove_music(suzuki_music_t* music);
void suzuki_smd_get_instrument_attr(SpuVoiceAttr* attr, s16 index);
void suzuki_smd_set_flags_all_channels(u16 mask, suzuki_music_t* music);

/* SFX (0x800124cc-0x80013f74). The SFX music record has eight channels on
 * SPU voices 16-23; the low byte of a start_sfx channel id is the first
 * channel and its high byte the steal priority
 * (SUZUKI_SOUND_SFX_CHANNEL_ID). */
enum {
    SUZUKI_SFX_CHANNEL_COUNT = 8,
    SUZUKI_SFX_VOICE_FIRST = 16,
};

void suzuki_sound_set_sfx_enabled(s32 enabled);
void suzuki_sound_play_sfx_in_channel(s32 sound_id, s32 channel);
void suzuki_sound_play_sfx_with_settings_find_channel(s32 sound_id, s32 volume, s32 balance);
void suzuki_sound_play_sfx_with_settings_in_channel(s32 sound_id, s32 channel, s32 volume, s32 balance);
void suzuki_sound_play_2_sfx(s32 first, s32 second);
void suzuki_sound_play_4_sfx(s32 first, s32 second, s32 third, s32 fourth);
void suzuki_sound_stop_resource_sfx(suzuki_sound_resource_t* resource);
void suzuki_sound_stop_sfx_channels(s32 sound_id);
void suzuki_sound_stop_sfx_channel_pair(s32 index);
void suzuki_sound_release_sfx_voices(suzuki_music_t* music, u32 channel_mask, u32 voice_mask);
void suzuki_sound_set_sfx_balance(s32 sound_id, s32 balance);
u32 suzuki_sound_get_sfx_channels(s32 sound_id);
s32 suzuki_sound_find_sfx_channel(s32 sound_id, s32 voice_count);
suzuki_music_t* suzuki_sound_init_sfx_music(void);
void suzuki_smd_init_sfx_music_header(suzuki_music_t* music);
void suzuki_sound_start_sfx(s16 channel_id, s32 sound_id, s16 volume, s16 balance);
void suzuki_sound_stop_all(void);

/* Suzuki heap (0x8001423c-0x80014544) and SPU RAM wrappers. */
void suzuki_sound_copy_memory(void* destination, const void* source, s32 size);
s32 suzuki_sound_alloc_spu_ram(s32 size);
s32 suzuki_sound_free_spu_ram(u32 addr);

/* Root-counter tick (0x80014590-0x80015324, 0x80017118-0x800178e4). The
 * per-channel passes take (music, first channel, channel count). */
s32 suzuki_sound_root_counter_2_handler(void);
void suzuki_sound_update_voice_attrs(void);
void suzuki_sound_flush_key_on(void);
void suzuki_sound_flush_key_off(void);
void suzuki_smd_step_ramp(suzuki_ramp_t* ramp);
void suzuki_sound_calculate_pitch_lfo_voices(void);
void suzuki_sound_calculate_noise_voices(void);
void suzuki_sound_calculate_reverb_voices(void);
void suzuki_smd_update_channel_ramps(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void suzuki_smd_read_instructions(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void suzuki_smd_update_modulators(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void suzuki_smd_update_voices(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
s16 suzuki_smd_calculate_pitch(s16 key);
void suzuki_smd_modulator_reset(suzuki_modulator_t* modulator);
void suzuki_smd_modulator_deactivate(suzuki_modulator_t* modulator);
s32 suzuki_smd_modulator_calculate_step(s32 depth, s16 speed, s16 waveform);
s32 suzuki_smd_modulator_step_square(suzuki_modulator_t* modulator);
s32 suzuki_smd_modulator_step_alternating(suzuki_modulator_t* modulator);
s32 suzuki_smd_modulator_step_triangle(suzuki_modulator_t* modulator);
s32 suzuki_smd_modulator_step_triangle_centered(suzuki_modulator_t* modulator);
s32 suzuki_smd_modulator_step_sawtooth(suzuki_modulator_t* modulator);
s32 suzuki_smd_modulator_step_random(suzuki_modulator_t* modulator);
s32 suzuki_smd_modulator_step_random_bipolar(suzuki_modulator_t* modulator);
void suzuki_smd_seed_random(s32 value);
s32 suzuki_smd_random(void);

/* Reprograms an SMD channel with an instrument of its waveset. */
void suzuki_smd_set_instrument(s16 instrument, suzuki_music_channel_t* channel);
void suzuki_smd_set_voice_update_flags_all_channels(u16 mask, suzuki_music_t* music);
void suzuki_smd_set_voice_attr_flags_all_channels(suzuki_music_t* music, u16 mask);

/* SMD opcode handlers; return the next position in the byte stream. */
u8* suzuki_smd_no_instruction(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_rest(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_fermata(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_opcode_8a(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_loop_point_if_selected(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_opcode_8e(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_opcode_8f(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_end_bar(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_loop(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_octave(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_raise_octave(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_lower_octave(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_time_signature(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_opcode_a4(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_opcode_a5(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_repeat(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_coda(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_to_coda(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_play_sfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_stop_sfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_play_vfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_tempo(u8* data, suzuki_music_t* music, suzuki_music_channel_t* unused_channel);
u8* suzuki_smd_accelerando(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* unused_channel);
u8* suzuki_smd_tempo_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_master_volume(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_master_volume_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_gate_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_set_voice(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_instrument(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_length_adjust(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_opcode_ae(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_opcode_af(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_slur_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_slur_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_enable_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_disable_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_enable_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_increase_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_turn_on_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_disable_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_acoustics(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_enable_reverb(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_disable_reverb(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_naturale(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_envelope_shape(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_attack_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_decay_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_sustain_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_release(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_sustain_level(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_envelope_tail(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_attack_mode(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_sustain_mode(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_release_mode(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pitch_offset(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_add_pitch_offset_coarse(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_add_pitch_offset_fine(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_add_pitch_offset_wide(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pitch_slide(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_toggle_pitch_slide_hold(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pitch_slide_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_portamento(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pitch_shift(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pitch_shift_waveform(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pitch_shift_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pitch_shift_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pitch_shift_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_dynamic(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_crescendo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_fermata_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_tremolo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_tremolo_waveform(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_tremolo_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_tremolo_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_tremolo_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_balance(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_shift_balance(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_balance_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pan_lfo_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pan_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pan_lfo_waveform(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pan_lfo_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_pan_lfo_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_modulator_select(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_modulator_set_depth(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_modulator_set_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_modulator_enable_mask(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_modulator_enable(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_modulator_disable(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_scale_tempo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_select_sound_font(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);
u8* suzuki_smd_end_track(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);

/* Driver set-up, wavesets, volume and SPU transfers (0x80017920-0x800186c4). */
void suzuki_sound_quit(void);
suzuki_waveset_t* suzuki_sound_load_waveset(suzuki_waveset_t* file);
void suzuki_sound_free_wavesets(void);
void suzuki_sound_free_waveset(suzuki_waveset_t* waveset);
suzuki_waveset_t* suzuki_sound_find_waveset(s16 id);
void suzuki_sound_set_type(s32 type);
suzuki_sound_type_e suzuki_sound_get_type(void);
void suzuki_sound_put_type(s16 level);
void suzuki_sound_set_reverb_mode(s32 mode, s16 depth, s32 delay, s32 feedback);
void suzuki_sound_set_master_volume(s16 volume, s16 time);
void suzuki_sound_set_cd_volume(s16 volume, s16 time);
void suzuki_sound_set_cd_reverb(s32 reverb, s32 mix);
void suzuki_sound_commit_volume_change(void);
void suzuki_sound_set_vol_balance(s32 volume, SpuVolume* volume_out, u8 mode);
void suzuki_sound_transfer_spu_data(u32 spu_address, void* data, u32 size, s32 mode);
void suzuki_sound_spu_transfer_callback(void);
s32 suzuki_sound_spu_event_handler(void);
s32 suzuki_sound_sum_resource_words(suzuki_resource_prefix_t* resource);
s16 suzuki_sound_wait_for_spu_transfer(s32 flags);
void suzuki_sound_init_stub(void);
void suzuki_sound_enable_root_counter_2_event(void);
void suzuki_sound_disable_root_counter_2_event(void);
/* Provisional: store a driver value nothing reads; nothing calls it. */
void suzuki_sound_set_unread_value(u16 value);

/* Suzuki key-on/key-off flushes. The driver compiled them both as functions
 * and inline in its root-counter handler, so their bodies live here once. */
/* Keys on the voices queued in each playing MUS record's key_on_mask; voices
 * reserved or pending release by SFX (g_suzuki_sound_sfx_reserved_voices, g_suzuki_sound_sfx_key_off_voices) only
 * for the SFX record (status bit 1). suzuki_sound_flush_key_on (0x80014818) is
 * this body alone; the root-counter handler (0x800149dc) contains it inline. */
static inline void suzuki_sound_flush_key_on_inline(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;

    voices = 0;
    allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                voices |= music->key_on_mask;
            else
                voices |= allowed & music->key_on_mask;
            music->key_on_mask = 0;
        }
        music = music->next;
    }

    if (voices != 0)
        SpuSetKey(SPU_ON, voices);
}

/* Keys off the voices queued in each playing MUS record's key_off_mask (same
 * SFX filter as the key-on flush) plus the voices of stopped music
 * (g_suzuki_sound_music_key_off_voices) and the SFX voices pending release (g_suzuki_sound_sfx_key_off_voices), which
 * first get a fast release (rate 6, mode 3). suzuki_sound_flush_key_off (0x800148b4) is this body alone; the
 * root-counter handler contains it inline. */
static inline void suzuki_sound_flush_key_off_inline(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;
    u32 pending;
    s32 voice;

    allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
    voices = (allowed & g_suzuki_sound_music_key_off_voices) | g_suzuki_sound_sfx_key_off_voices;
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                voices |= music->key_off_mask;
            else
                voices |= allowed & music->key_off_mask;
            music->key_off_mask = 0;
        }
        music = music->next;
    }

    pending = (allowed & g_suzuki_sound_music_key_off_voices) | g_suzuki_sound_sfx_key_off_voices;
    if (pending != 0) {
        for (voice = 23; voice >= 0; voice--) {
            if ((pending & SPU_VOICECH(voice)) != 0)
                SpuSetVoiceRRAttr(voice, 6, SPU_VOICE_LINEARDecN);
        }
        g_suzuki_sound_sfx_key_off_voices = 0;
        g_suzuki_sound_music_key_off_voices = 0;
    }

    if (voices != 0)
        SpuSetKey(SPU_OFF, voices);
}

#endif
