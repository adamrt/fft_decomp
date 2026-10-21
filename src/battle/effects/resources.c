/* Initialize the resident effect pools and allocate secondary-effect records and slots. */

#include "fft/effect.h"
#include "fft/effect_model.h"
#include "fft/effect_particle.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/effect_timeline.h"
#include "fft/memory.h"
#include "fft/system.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern u8 g_effect_element_masks[EFFECT_ELEMENT_INDEX_COUNT];
extern effect_sprite_section_t* g_effect_secondary_particle_frame_data_ptr;

/* Secondary pool and slot-chain ownership. */
extern effect_secondary_data_t g_effect_secondary_data[EFFECT_SECONDARY_RECORD_COUNT];
extern u16 g_effect_secondary_free_head;
extern u16 g_effect_secondary_active_tail;
extern u8 g_effect_slot_active_head;
extern u8 g_effect_slot_free_head;

/* Reset the effect system for a battle: the effect slot chains (slot 0 the sentinel, 1-32 free),
 * the 16 secondary effect records (all on the free chain, their slot lists empty), the projectile
 * matrix translation, the render state and the effect camera. */
void effect_system_init(void) {
    s32 i;
    s32 j;

    g_effect_slots[0].prev = 0;
    g_effect_slots[0].next = 0;
    g_effect_slot_free_head = 1;
    for (i = 1; i < EFFECT_SLOT_COUNT; i++) {
        g_effect_slots[i].next = i + 1;
        g_effect_slots[i].prev = i - 1;
    }
    g_effect_slots[i].next = 0;
    g_effect_slots[i].prev = i - 1;
    g_effect_slot_active_head = 0;
    g_effect_projectile_matrix.t[2] = 0;
    g_effect_projectile_matrix.t[1] = 0;
    g_effect_projectile_matrix.t[0] = 0;
    for (i = 1; i < EFFECT_SECONDARY_SLOT_COUNT; i++) {
        g_effect_secondary_data[i].own_slot_id = i;
        g_effect_secondary_data[i].previous_slot_id = i + 1;
        g_effect_secondary_data[i].next_slot_id = i - 1;
        g_effect_secondary_data[i].function_id = 0;
        g_effect_secondary_data[i].allocation = 0;
    }
    g_effect_secondary_data[0].previous_slot_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].next_slot_id = EFFECT_SECONDARY_SLOT_COUNT - 1;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].own_slot_id = EFFECT_SECONDARY_SLOT_COUNT;
    g_effect_secondary_free_head = 1;
    g_effect_secondary_data[0].next_slot_id = 0;
    g_effect_secondary_data[0].own_slot_id = 0;
    g_effect_secondary_data[0].function_id = 0;
    g_effect_secondary_data[0].allocation = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].previous_slot_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].function_id = 0;
    g_effect_secondary_data[EFFECT_SECONDARY_SLOT_COUNT].allocation = 0;
    g_effect_secondary_active_tail = 0;
    g_effect_secondary_particle_frame_data_ptr = &g_effect_secondary_particle_frame_data;
    for (i = 0; i < EFFECT_SECONDARY_RECORD_COUNT; i++) {
        g_effect_secondary_data[i].active_count = 0;
        for (j = EFFECT_SLOT_COUNT - 1; j >= 0; j--) {
            g_effect_secondary_data[i].slot_ids[j] = 0;
        }
    }
    effect_render_state_reset();
    effect_camera_init();
}

/* Take a secondary effect record from the free chain and append it to the active
 * chain, with its timer and slot ids cleared, and return its id. When no record
 * is free it passes code 0x66 to the empty report hook and returns 0.
 *
 * The id is kept as an s32 byte load and a separate `slot = id & 0xff` indexes
 * the table: the target tests and indexes through the masked copy ($v0) while
 * storing and returning the raw id ($s0), which keeps the error path's `move
 * v0,s0` instead of a folded 0. Writing previous_slot_id before the next_slot_id
 * clear reproduces the target's early g_effect_secondary_active_tail load. */
static u8 secondary_record_alloc(void) {
    s32 id = (u8)g_effect_secondary_free_head;
    s32 slot = id & 0xff;
    s32 i;

    if (slot != 0) {
        g_effect_secondary_free_head = g_effect_secondary_data[slot].previous_slot_id;
        g_effect_secondary_data[g_effect_secondary_free_head].next_slot_id = 0;
        g_effect_secondary_data[slot].previous_slot_id = g_effect_secondary_active_tail;
        g_effect_secondary_data[slot].next_slot_id = 0;
        g_effect_secondary_data[g_effect_secondary_active_tail].next_slot_id = id;
        g_effect_secondary_active_tail = id;
        g_effect_secondary_data[slot].timer = 0;
        for (i = 31; i >= 0; i--) {
            g_effect_secondary_data[slot].slot_ids[i] = 0;
        }
        return id;
    }
    system_report_stub(1, 0x66);
    return id;
}

/* Take secondary effect record id (1-16) off the active chain, free its work
 * block and append it to the end of the free chain headed by
 * g_effect_secondary_free_head: the inverse of
 * secondary_record_alloc. An id outside 1-16 only reaches the
 * empty report hook with code 0x67.
 *
 * Both chains are walked through previous_slot_id. A record removed from the
 * middle of the active chain passes its previous link to the record after it, but
 * the record before it keeps next_slot_id pointing at the freed record; only the
 * tail's next link is cleared.
 *
 * The free-chain walk index is an s32: a u16 adds an `andi 0xffff` per load. */
void effect_secondary_remove(u8 id) {
    effect_secondary_data_t* slot = &g_effect_secondary_data[id];
    s32 tail;

    if (id != 0 && id < EFFECT_SECONDARY_RECORD_COUNT) {
        if (slot->allocation != 0) {
            heap_free(slot->allocation);
            slot->allocation = 0;
        }
        if (slot->next_slot_id != 0) {
            g_effect_secondary_data[slot->next_slot_id].previous_slot_id = slot->previous_slot_id;
        } else {
            g_effect_secondary_active_tail = slot->previous_slot_id;
        }
        g_effect_secondary_data[g_effect_secondary_active_tail].next_slot_id = 0;
        tail = g_effect_secondary_free_head;
        if (tail != 0) {
            while (g_effect_secondary_data[tail].previous_slot_id != 0) {
                tail = g_effect_secondary_data[tail].previous_slot_id;
            }
            g_effect_secondary_data[tail].previous_slot_id = id;
            slot->next_slot_id = tail;
        } else {
            g_effect_secondary_free_head = id;
            slot->next_slot_id = 0;
        }
        slot->previous_slot_id = 0;
        slot->phase = EFFECT_SECONDARY_PHASE_ENDED;
        return;
    }
    system_report_stub(1, 0x67);
}

/* Return the phase of secondary effect record secondary_id; ENDED once the record has been
 * removed. */
effect_secondary_phase_e effect_secondary_phase_get(u8 secondary_id) {
    return g_effect_secondary_data[secondary_id].phase;
}

/* Ask secondary effect secondary_id to wind down: its handler runs the FINALIZING phase next. */
void effect_secondary_finalize(u8 secondary_id) {
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_FINALIZING;
}

/* Take an effect slot from the free chain and push it on the head of the active
 * chain. Returns its id, or 0 (the null sentinel) when no slot is free; the
 * slot's life is cleared either way. */
s32 effect_slot_alloc(void) {
    u8 head = g_effect_slot_free_head;
    u8 tail;
    s32 id = head;

    if (id != 0) {
        g_effect_slot_free_head = g_effect_slots[id].next;
        g_effect_slots[g_effect_slots[id].next].prev = 0;
        g_effect_slots[g_effect_slot_active_head].prev = head;
        tail = g_effect_slot_active_head;
        g_effect_slots[id].prev = 0;
        g_effect_slots[id].next = tail;
        g_effect_slot_active_head = head;
    }
    g_effect_slots[id].life = 0;
    return id;
}

/* Unlinks slot id from the active chain and pushes it onto the free chain;
 * slot 0 is the null sentinel and is ignored. */
void effect_slot_free(u8 id) {
    u8 free_head;

    if (id == 0) {
        return;
    }
    g_effect_slots[g_effect_slot_free_head].prev = id;
    if (g_effect_slots[id].prev != 0) {
        g_effect_slots[g_effect_slots[id].prev].next = g_effect_slots[id].next;
    } else {
        g_effect_slot_active_head = g_effect_slots[id].next;
    }
    g_effect_slots[g_effect_slots[id].next].prev = g_effect_slots[id].prev;
    free_head = g_effect_slot_free_head;
    g_effect_slots[id].prev = 0;
    g_effect_slots[id].next = free_head;
    g_effect_slot_free_head = id;
}

/* Start secondary effect animation_id for the caster and first target of the
 * init record and return its id. parameter is the animation's own: an
 * element mask, a sprite palette or a duration. */
s32 effect_secondary_init(s32 animation_id, s32 parameter, const effect_context_t* source) {
    s32 secondary_id = secondary_record_alloc();

    g_effect_secondary_data[secondary_id].animation_id = animation_id;
    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].parameter = parameter;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.record = source->targets[0];
    g_effect_secondary_data[secondary_id].used_weapon_id = source->used_weapon_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    return secondary_id;
}

/* Start the current effect target's secondary effect: the low half of
 * animation selects the animation id (and its function id), the high half is
 * stored alongside it. Returns its id; phase 1 is
 * EFFECT_SECONDARY_PHASE_INITIALIZING. */
s32 effect_secondary_projectile_init(s32 animation, const effect_context_t* source) {
    u8 secondary_id = secondary_record_alloc();

    g_effect_secondary_data[secondary_id].animation_id = animation;
    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].parameter = animation >> 16;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.record = source->targets[0];
    g_effect_secondary_data[secondary_id].used_weapon_id = source->used_weapon_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    g_effect_secondary_data[secondary_id].target_count = source->target_count;
    return secondary_id;
}

/* Returns the effect_element_index_e of an element mask: 0 for no
 * element, otherwise the position of the first table mask it meets (9 when it
 * meets none). */
s32 effect_element_index_get(s32 elements) {
    s32 i;

    if (elements != 0) {
        for (i = 0; i < EFFECT_ELEMENT_INDEX_COUNT; i++) {
            if ((elements & g_effect_element_masks[i]) != 0) {
                break;
            }
        }
        elements = i;
    }
    return elements;
}

/* Start the current effect target's weapon-hit secondary effect.
 *
 * The target's hit result selects the WEAPON_HIT animation (hit, critical,
 * missed, no effect), EQUIP_BREAK (both BREAK results) or GOLEM; REFLECTED
 * keeps the slot's animation. The parameter
 * is the element effect index for the weapon's element mask. Returns its id,
 * or 0 when the init record has no targets. */
s32 effect_secondary_weapon_hit_init(s32 elements, const effect_context_t* source, s32 unused_is_bow) {
    u8 secondary_id;

    if (source->target_count != 0) {
        elements = effect_element_index_get(elements);
        secondary_id = secondary_record_alloc();
        switch (source->targets[0].data.unit.result_animation) {
        case EFFECT_HIT_NORMAL:
        case EFFECT_HIT_CRITICAL:
        case EFFECT_HIT_MISSED:
        case EFFECT_HIT_NO_EFFECT:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_WEAPON_HIT;
            break;
        case EFFECT_HIT_BREAK:
        case EFFECT_HIT_BREAK_ALT:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK;
            break;
        case EFFECT_HIT_GOLEM:
            g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_GOLEM;
            break;
        }
        g_effect_secondary_data[secondary_id].parameter = elements;
        g_effect_secondary_data[secondary_id].function_id
            = g_effect_secondary_animations[g_effect_secondary_data[secondary_id].animation_id].function_id;
        g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
        g_effect_secondary_data[secondary_id].target.record = source->targets[0];
        g_effect_secondary_data[secondary_id].timer = 0;
        g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    } else {
        secondary_id = 0;
    }
    return secondary_id;
}

/* Start the current effect target's fall-dust secondary effect at the
 * given position and return its id. Phase 1 is
 * EFFECT_SECONDARY_PHASE_INITIALIZING. */
s32 effect_secondary_fall_dust_init(const effect_context_t* source, const VECTOR* position) {
    s32 secondary_id = secondary_record_alloc();

    g_effect_secondary_data[secondary_id].timer = 0;
    g_effect_secondary_data[secondary_id].animation_id = EFFECT_SECONDARY_ANIMATION_FALL_DUST;
    g_effect_secondary_data[secondary_id].function_id
        = g_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_FALL_DUST]
              .function_id; /* array access keeps the load after the animation store */
    g_effect_secondary_data[secondary_id].caster = source->targets[EFFECT_CASTER_ENTRY];
    g_effect_secondary_data[secondary_id].target.position.x = position->vx >> 4;
    g_effect_secondary_data[secondary_id].target.position.z = position->vy >> 4;
    g_effect_secondary_data[secondary_id].target.position.y = position->vz >> 4;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    return secondary_id;
}

/* Start EFFECT_SECONDARY_ANIMATION_ENDING in a fresh secondary effect
 * record and return its id. Its null handler only steps through the phases, so
 * the record lasts two more frames. The arrow handler starts it when the arrow
 * lands without a hit. */
s32 effect_secondary_end_on_current_unit(void) {
    s32 secondary_id = secondary_record_alloc();
    s32 record_offset = secondary_id * sizeof(effect_secondary_data_t);
    u8 function_id;

    /* One byte offset serves both stores; indexing the record twice changes the code. */
    *(s16*)((u8*)&g_effect_secondary_data + record_offset + offsetof(effect_secondary_data_t, timer)) = 0;
    *(u16*)((u8*)&g_effect_secondary_data + record_offset + offsetof(effect_secondary_data_t, animation_id))
        = EFFECT_SECONDARY_ANIMATION_ENDING;
    function_id = g_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_ENDING].function_id;
    g_effect_secondary_data[secondary_id].phase = EFFECT_SECONDARY_PHASE_INITIALIZING;
    g_effect_secondary_data[secondary_id].function_id = function_id;
    return secondary_id;
}
