#include "fft/main.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Build the portrait source rectangle and palette for a formation unit.
 *
 * Special jobs use fixed portrait entries; generic humans derive theirs from
 * job and gender. The third argument is retained because all known callers
 * pass it even though this function does not consume it. */
void world_formation_build_unit_graphic_entry(s16 formation_index, texture_source_t* destination, s32 unused) {
    world_unit_t* unit;
    s16 job_id;
    s32 index;
    s32 palette;
    s16 clut_x;

    unit = g_world_unit_pointers[formation_index];
    job_id = unit->identity.job_id;
    if (job_id == JOB_ID_BYBLOS) {
        index = 0x4E;
    } else if (job_id == JOB_ID_STEEL_GIANT) {
        index = 0x52;
    } else if (job_id == JOB_ID_APANDA) {
        index = 0x4E;
    } else if (job_id == JOB_ID_SERPENTARIUS) {
        index = 0x4F;
    } else if (job_id == JOB_ID_ARCHAIC_DEMON) {
        index = 0x51;
    } else if (job_id == JOB_ID_ULTIMA_DEMON) {
        index = 0x51;
    } else if (job_id == JOB_ID_HOLY_DRAGON_REIS) {
        index = 0x4C;
    } else if (unit->sprite_set == CHARACTER_IDENTITY_MONSTER) {
        index = unit->monster_graphic + 0x3D;
    } else if (unit->sprite_set < CHARACTER_IDENTITY_BOY) {
        index = g_world_formation_generic_human_graphic_index_table[unit->sprite_set];
    } else {
        world_formation_get_generic_human_graphic_entry(job_id, unit->gender_flags & UNIT_FLAG_FEMALE, destination);
        return;
    }
    bcopy(&g_world_formation_graphic_entries[index], destination, sizeof(*destination));
    palette = (s16)world_formation_get_unit_palette_index(g_world_unit_pointers[formation_index]->identity.roster_slot);
    clut_x = (palette % 4) * 0x10 + 0x140;
    job_id = palette / 4;
    destination->clut = GetClut(clut_x, job_id + 0xE0);
}
