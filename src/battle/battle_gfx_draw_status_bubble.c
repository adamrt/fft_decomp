#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "psx/types.h"

/*
 * Queue the unit's status bubble into the ordering table.
 *
 * The bubble's texture coordinates are chosen from the status bubble id: the
 * "dying" bubble (id 0) and the status-CT bubble (id 20) show a counter digit,
 * and ids 9, 10 and 21 have a second texture row selected by field_2e2.  The
 * bubble is drawn one tile above the unit's feet and offset by status_bubble_x
 * and status_bubble_y.
 */
void battle_gfx_draw_status_bubble(battle_unit_misc_data_t* unit, const u16* screen_coords) {
    SVECTOR position;
    SVECTOR zoom = g_battle_status_bubble_zoom;
    VECTOR transformed;
    SVECTOR base;
    SVECTOR raised;
    long flag;
    battle_gfx_sprite_display_data_t* display;
    s32 bubble_id;
    /* One local for both digit indices: the status-CT arm reuses the dying
     * counter's register, and two separate locals allocate differently. */
    s32 table_index;
    u8 row;
    u8 flag_row;
    u8 default_row;
    s32 depth;
    s16 y;
    u32* otag;

    /* The caller supplies its culling coordinates in a1, but this routine
     * independently projects the bubble and never reads them. */
    (void)screen_coords;

    bubble_id = unit->status_bubble_id;
    display = unit->status_bubble_display;
    if (bubble_id == 21) {
        display->parts[0].u = 0x72;
        display->parts[0].v = 0xB0;
        if (unit->status_bubble_alternate_row != 0) {
            display->parts[0].v = 0xBC;
        }
    } else if (bubble_id == 0) {
        table_index = unit->battle_data->death_counter;
        if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_RERAISE) {
            display->parts[0].u = g_battle_status_bubble_u[10];
            flag_row = g_battle_status_bubble_v[10];
            display->parts[0].v = flag_row;
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = flag_row + 12;
            }
        } else if (table_index != 0xFF) {
            display->parts[0].u = g_battle_status_bubble_digit_u[table_index];
            display->parts[0].v = g_battle_status_bubble_digit_v[table_index];
        } else {
            display->parts[0].u = g_battle_status_bubble_u[0];
            default_row = g_battle_status_bubble_v[0];
            display->parts[0].v = default_row;
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = default_row + 12;
            }
        }
    } else if (bubble_id == 20) {
        table_index = unit->battle_data->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE)] & 3;
        display->parts[0].u = g_battle_status_bubble_digit_u[table_index];
        display->parts[0].v = g_battle_status_bubble_digit_v[table_index];
    } else {
        display->parts[0].u = g_battle_status_bubble_u[bubble_id];
        row = g_battle_status_bubble_v[bubble_id];
        display->parts[0].v = row;
        if (bubble_id == 9 || bubble_id == 10) {
            if (unit->status_bubble_alternate_row != 0) {
                display->parts[0].v = row + 12;
            }
        }
    }

    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    main_util_set_svector(&base, unit->screen.vx + unit->effect_vector_2.vx,
        unit->screen.vy + unit->effect_vector_2.vy + (s8)unit->status_bubble_y,
        unit->screen.vz + unit->effect_vector_2.vz);
    RotTrans(&base, &transformed, &unit->status_bubble_gte_flag);
    position.vx = (s8)unit->status_bubble_x + (transformed.vx + unit->screen_offset.vx);
    y = transformed.vy + unit->screen_offset.vy;
    position.vy = y;
    if (unit->status_bubble_timer & 0x10) {
        position.vy = y - 1;
    }
    battle_camera_convert_screen_coords_modify_by_1(&base.vx, (s16*)&raised);
    raised.vy += 12;
    RotTrans(&raised, &transformed, &flag);
    display = unit->status_bubble_display;
    otag = main_gfx_get_otag();
    depth = transformed.vz / 4;
    battle_gfx_construct_polygon_data_for_units((battle_gfx_sprite_display_t*)display, display->part_count, 0,
        (s16*)&position, 0, 0, (s16*)&zoom, otag + depth);
}
