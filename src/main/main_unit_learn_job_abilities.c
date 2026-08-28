#include "fft/battle_ability.h"
#include "fft/character_identity.h"
#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"

/* Learns abilities for one job of a unit.
 * job_id 0x4a (Squire) stands for the unit's base job.  With an ENTD primary
 * skillset of 0 every ability of the unit's own job is learned; otherwise
 * each ability is rolled against its learn chance and bought with job JP.
 * The original reuses two scratch variables: `i` doubles as the unlocked-job
 * mask and `scratch` holds both the unlocked-job bits and the learn chance;
 * separate locals change the register allocation. */
void main_unit_learn_job_abilities(battle_stats_t* unit, s32 job_id, const entd_unit_t* entd) {
    s32 job_index;
    s32 skillset;
    s32 auto_learn;
    s32 learned;
    s32 i;
    s32 ability_id;
    s32 scratch;
    u16* job_jp;
    u8* learned_bits;
    ability_data_t* ability;
    u8 character;
    s32 unit_flags;

    auto_learn = 0;
    job_index = job_id - JOB_ID_SQUIRE;
    if (job_id == JOB_ID_SQUIRE) {
        character = unit->character_identity;
        if ((u8)(character - CHARACTER_IDENTITY_SELECTOR_FIRST) >= CHARACTER_IDENTITY_GENERIC_HUMAN_COUNT) {
            if (character == CHARACTER_IDENTITY_MONSTER) {
                job_id = unit->job_id;
            } else {
                job_id = unit->character_identity;
            }
        }
    } else {
        i = 0x800000 >> job_index;
        scratch = (unit->unlocked_jobs[0] << 16) + (unit->unlocked_jobs[1] << 8) + unit->unlocked_jobs[2];
        if ((scratch & i) == 0) {
            return;
        }
    }

    unit_flags = unit->unit_flags;
    if (job_id == JOB_ID_BARD && (unit_flags & UNIT_FLAG_FEMALE)) {
        return;
    }
    if (job_id == JOB_ID_DANCER && (unit_flags & UNIT_FLAG_MALE)) {
        return;
    }

    if (job_id == unit->job_id) {
        skillset = unit->primary_skillset;
        if (entd->primary_skillset == 0) {
            auto_learn = 1;
        }
    } else {
        skillset = g_job_data_pointer[job_id].skillset;
    }
    i = 0;
    /* Views of job_points[job_index] and learned_abilities[job_index * 3]. The
     * target adds the scaled index before the unit base and folds 0xdc into
     * each JP load/store. Direct member indexing changes register allocation
     * (588 vs 584 bytes); member-base local pointers grow the frame and emit
     * 604 bytes. Keep these address expressions for the exact target. */
    job_jp = (u16*)(job_index * 2 + (u32)unit);
    learned_bits = (u8*)(job_index * 3 + (u32)unit + 0x99);
    learned = 0;
    /* A goto loop: as a do/while, GCC's loop pass hoists the 0x80 bit-mask
     * constant into a saved register, which the target does not. */
loop: {
    ability_id = (u16)main_ability_get_id_from_skillset(skillset, i);
    if (ability_id != 0) {
        ability = &g_main_ability_data[ability_id];
        scratch = ability->learn_rate;
        if (auto_learn != 0) {
            learned = 1;
        } else if (main_util_roll_pass_fail(100, scratch) == 0) {
            u16 cost = ability->jp_cost;
            if (job_jp[0xdc / 2] >= cost) {
                job_jp[0xdc / 2] = job_jp[0xdc / 2] - cost;
                learned_bits[i / 8] = learned_bits[i / 8] | (0x80 >> (i % 8));
                learned = 1;
            }
        }
        if (learned != 0) {
            learned_bits[i / 8] = learned_bits[i / 8] | (0x80 >> (i % 8));
        }
    }
    learned = 0;
    i++;
    if (i < SKILLSET_ABILITY_LIST_COUNT) {
        goto loop;
    }
}
}
