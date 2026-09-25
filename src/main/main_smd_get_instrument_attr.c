#include "fft/main.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Fills the sample address and envelope of an SpuVoiceAttr from an
 * instrument of the first loaded waveset (the list head). addr is the
 * instrument's offset within the waveset, not an absolute SPU address. */
void main_smd_get_instrument_attr(SpuVoiceAttr* attr, s16 index) {
    suzuki_instrument_t* instrument;

    instrument = &g_main_sound_waveset_list->instruments[index];
    attr->addr = instrument->start;
    attr->a_mode = instrument->attack_mode;
    attr->s_mode = instrument->sustain_mode;
    attr->r_mode = instrument->release_mode;
    attr->ar = instrument->attack_rate;
    attr->dr = instrument->decay_rate;
    attr->sr = instrument->sustain_rate;
    attr->rr = instrument->release_rate;
    attr->sl = instrument->sustain_level;
}
