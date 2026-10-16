/* Rewards, including item and equipment overflow, post-battle changes, war trophies and bonus money, the gil ticker,
 * POLY_GT4 arrays, condition display, the bonus image burst and the help menu overlay. */

#include "fft/ability.h"
#include "fft/after_battle.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/overlay.h"
#include "fft/party.h"
#include "fft/portrait.h"
#include "fft/reward.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void help_open_after_battle(void);
static void text_build_battle_unit_names(void);

/* REQUIRE twin of unit_panel_load_party_unit: rebuild party slot `party_index` into a
 * scratch unit and fill the selected-unit status and editor display from it, at full
 * HP, MP and CT. */
void unit_panel_load_party_unit(s32 unused, s32 party_index) {
    unit_stats_t* unit;
    menu_status_panel_slot_state_t* editor;
    u16* innate_items;
    item_data_t* item;
    s32 i;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    u16 maximum_hp;
    u16 maximum_mp;
    unit_panel_summary_t* gauges;

    unit_init_from_party_slot(&g_unit_panel_editor_party_unit_stats, party_index, 0);
    editor = &g_unit_panel_selected_unit_fields.state;
    unit = &g_unit_panel_editor_party_unit_stats;
    gauges = &g_unit_panel_selected_unit_status;

    g_unit_panel_selected_unit.name_id = party_index;
    g_unit_panel_selected_unit.battle_id = party_index;
    gauges->team_kind = 0;
    g_unit_panel_selected_unit.job_id = unit->job_id;
    g_unit_panel_selected_unit.brave = unit->brave;
    g_unit_panel_selected_unit.faith = unit->faith;
    g_unit_panel_selected_unit.zodiac = unit->birthday.zodiac;
    if (unit->initial_team_flags & UNIT_TEAM_MASK) {
        gauges->team_kind = 1;
    }
    if (!(unit->initial_team_flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK)) {
        gauges->team_kind = 2;
    }

    g_unit_panel_selected_unit_status.list_index = 0;
    g_unit_panel_selected_unit_status.unit_count = 0;
    /* Offset keeps party slots apart from other unit indices; the status thread
     * starts a portrait transition whenever this index changes. */
    g_unit_panel_selected_unit_status.unit_index = party_index + 0x100;
    g_unit_panel_selected_unit_status.level = unit->level;
    g_unit_panel_selected_unit_status.experience = unit->experience;

    maximum_hp = unit->max_hp;
    g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp;
    if (maximum_hp == 0) {
        g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp + 1;
    }
    g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_HP].value
        = g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    maximum_mp = unit->max_mp;
    g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp;
    if (maximum_mp == 0) {
        g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp + 1;
    }
    g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_MP].value
        = g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    g_unit_panel_selected_unit_status.gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;

    editor->move = unit->move;
    editor->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    editor->jump = unit->jump;
    editor->right_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    editor->left_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    editor->right_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    editor->left_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];

    editor->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    editor->physical_class_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    /* Each shield evade shows the larger of the two hands' values. */
    editor->physical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE])
        editor->physical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    editor->physical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];

    editor->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    /* Units carry no class magic evade stat. */
    editor->magical_class_evade = 0;
    editor->magical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE])
        editor->magical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    editor->magical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

    for (i = 4; i >= 0; i--)
        editor->equipment[i] = 0;

    /* Monsters show no equipment; each hand shows its weapon, else its shield. */
    if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
        editor->equipment[0] = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
        editor->equipment[1] = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
        editor->equipment[2] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
        editor->equipment[3] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
        editor->equipment[4] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    }

    for (i = 0; i < 5; i++) {
        if (editor->equipment[i] == 0 || editor->equipment[i] == 0xff)
            editor->equipment[i] = -1;
    }

    editor->generic_monster = 0;
    if (unit->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && unit->primary_skillset < SKILLSET_ID_END)
        editor->generic_monster = 1;

    if (editor->generic_monster != 0) {
        innate_items = ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        i = 0;
        do {
            editor->ability_ids[i] = *innate_items++;
            i++;
        } while (i < 5);
        /* No monster skill check runs here (unlike unit_panel_load_selected),
         * so the fourth monster ability is always cleared. */
        editor->ability_ids[3] = 0;
    } else {
        field_value = unit->primary_skillset;
        editor->ability_ids[0] = field_value;
        /* A missing skillset blanks its name with a terminator; the secondary name
         * takes it at byte 1, as in every copy of this loader. */
        if (field_value == 0)
            g_unit_panel_billboard_skillset_name[0] = 0xfe;
        field_value = unit->secondary_skillset;
        editor->ability_ids[1] = field_value;
        if (field_value == 0)
            g_unit_panel_billboard_secondary_skillset_name[1] = 0xfe;
        editor->ability_ids[2] = unit->abilities.reaction;
        editor->ability_ids[3] = unit->abilities.support;
        editor->ability_ids[4] = unit->abilities.movement;
    }

    /* Two Hands applies only when exactly one hand holds an item. */
    item_id = 0;
    editor->two_hands = 0;
    if (editor->equipment[0] != -1) {
        if (editor->equipment[1] == -1)
            item_id = editor->equipment[0];
    } else if (editor->equipment[1] != -1) {
        item_id = editor->equipment[1];
    }

    if (item_id != 0) {
        item = item_get_data(item_id);
        if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
            item_flags = g_item_weapon_data[item_id].flags;
            if ((item_flags & WEAPON_FLAG_FORCED_TWO_HANDS)
                || ((unit->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TWO_HANDS)
                    && (item_flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)))
                editor->two_hands = 1;
        }
    }
}

/* Return a roster unit's seven equipped items to the inventory. Always
 * returns 0. */
static s32 party_return_equipment(s32 party_index) {
    party_unit_t* party = party_get_unit(party_index);
    s32 index;

    for (index = 0; index < UNIT_EQUIPMENT_SLOT_COUNT; index++) {
        u8 item_id = party->equipment[index];

        if ((item_id != ITEM_ID_NOTHING) && (item_id != ITEM_ID_NONE)) {
            g_item_quantities[item_id]++;
        }
    }
    return 0;
}

/* Sell each of a battle unit's seven equipped items of which 100 or more are
 * already held or equipped, for a quarter of its price, and remove it from
 * the unit. Returns 1 when it sold anything. */
s32 party_sell_equipment_over_limit(s32 unit_id) {
    unit_stats_t* unit;
    s32 sold_equipment = 0;
    s32 equipment_slot;

    unit = unit_get_stats(unit_id);
    equipment_slot = 0;
    do {
        u8 item_id = unit->equipment[equipment_slot];
        s32 sale_value;

        if (item_id != ITEM_ID_NOTHING && item_id != ITEM_ID_NONE && item_get_total_quantity(item_id, 1) >= 100) {
            unit->equipment[equipment_slot] = ITEM_ID_NOTHING;
            sale_value = item_get_data(item_id)->price;
            sale_value /= 4;
            script_set_variable(
                EVENT_SCRIPT_VAR_WAR_FUNDS, script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + sale_value);
            sold_equipment = 1;
        }
        equipment_slot++;
    } while (equipment_slot < UNIT_EQUIPMENT_SLOT_COUNT);
    return sold_equipment;
}

/* Return a departing battle unit's seven equipped items to the inventory.
 * An item of which 100 or more are already held or equipped is sold for a
 * quarter of its price instead and removed from the unit. Returns 1 when
 * it sold anything. */
s32 party_store_or_sell_equipment(s32 unit_id) {
    unit_stats_t* unit;
    s32 sold_equipment = 0;
    s32 equipment_slot;

    unit = unit_get_stats(unit_id);
    equipment_slot = 0;
    do {
        u8 item_id = unit->equipment[equipment_slot];
        s32 sale_value;

        if ((item_id != ITEM_ID_NOTHING) && (item_id != ITEM_ID_NONE)) {
            if (item_get_total_quantity(item_id, 1) >= 100) {
                unit->equipment[equipment_slot] = ITEM_ID_NOTHING;
                sale_value = item_get_data(item_id)->price;
                sale_value /= 4;
                script_set_variable(
                    EVENT_SCRIPT_VAR_WAR_FUNDS, script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + sale_value);
                sold_equipment = 1;
            } else {
                g_item_quantities[item_id]++;
            }
        }
        equipment_slot++;
    } while (equipment_slot < UNIT_EQUIPMENT_SLOT_COUNT);
    return sold_equipment;
}

/*
 * Can battle unit `unit_id` join the roster? Returns 2 for a unit with the
 * save-formation flag (a guest), 1 when one of the regular roster slots is
 * free and 0 when they are all taken.
 *
 * party_find_free_slot clears the byte it is handed for a palette. Here
 * that is the palette of the first roster record with a Ramza identity (the
 * last record when there is none).
 */
static s32 party_get_join_status(s32 unit_id) {
    party_unit_t* party;
    unit_stats_t* unit = unit_get_stats(unit_id);
    s32 index;

    if (unit->unit_flags & UNIT_FLAG_SAVE_FORMATION) {
        return 2;
    }

    for (index = 0; index < PARTY_ROSTER_SLOT_COUNT; index++) {
        party = party_get_unit(index);
        if ((party->party_id != PARTY_ID_NONE) && (party->sprite_set != CHARACTER_IDENTITY_ENTD_NONE)
            && (party->sprite_set < CHARACTER_IDENTITY_RAMZA_END)) {
            break;
        }
    }

    return party_find_free_slot(0, &party->palette) != -1;
}

#define PARTY_MOVE_PARTY_PORTRAIT(party_index, portrait_rect, portrait_destination, palette_rect)                      \
    do {                                                                                                               \
        portrait_rect->x = (party_index % 7) * 8 + 0x100;                                                              \
        portrait_rect->y = (party_index / 7) * 0x30 + 0x28;                                                            \
        MoveImage(portrait_rect, portrait_destination[0], portrait_destination[1]);                                    \
        palette_rect->x = (party_index % 3) * 0x10 + 0x100;                                                            \
        palette_rect->y = party_index / 3;                                                                             \
        MoveImage(palette_rect, g_party_palette_destination, g_party_palette_destination_y);                           \
    } while (0)

/* Let the player pick a party member to dismiss when the roster is full.

   Returns 1 when the opening prompt answers 1 or -1 (no unit removed) and 0
   after party_remove_unit.
   Matching notes: should_restore also serves as the loop's constant 1 (the
   target keeps 1 in that variable's register and materializes it before the
   pointer copies); the portrait pointers are assigned inside the loop so
   loop.c hoists them after the thread constants; the R1 search loop holds
   its whole switch-unit block (a loop that large keeps the 0x14 bound
   inside it), while the L1 search stays a do-while (with both blocks inside
   their loops should_restore outranks selected_battle_unit for s4); the
   formation search is a goto loop so the 3 bound stays in place. */
static s32 party_select_unit_to_dismiss(void) {
    s32 party_index;
    u8* text;
    s32 selected_party_index;
    s32 result;
    s32 should_restore;
    s32 message_id;
    s32 formation_index;
    party_unit_t* party_unit;
    unit_stats_t* selected_battle_unit;
    RECT* portrait_rect;
    s16* portrait_destination;
    RECT* palette_rect;

    portrait_prepare_party_textures();
    text = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    unit_panel_set_transition(1);
    {
        s32 name_character_index;

        /* One name per roster slot, empty for units that cannot be dismissed: empty
         * slots, the slots past PARTY_MEMBER_SLOT_COUNT, units away on a proposition, eggs. */
        for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
            party_unit = party_get_unit(party_index);
            if (party_unit->party_id == PARTY_ID_NONE || party_index >= PARTY_MEMBER_SLOT_COUNT
                || party_unit->proposition_status != 0 || (party_unit->gender_flags & UNIT_FLAG_EGG) != 0) {
                g_party_removable_flags[party_index] = 0;
                *text = 0xfe;
                text++;
            } else {
                g_party_removable_flags[party_index] = 1;
                for (name_character_index = 0; name_character_index < UNIT_NAME_SIZE; name_character_index++) {
                    *text = party_unit->name[name_character_index];
                    /* Names end at 0xfe or 0xff; a full 16-character name gets a terminator added. */
                    if ((*text & 0xfe) == 0xfe) {
                        text++;
                        break;
                    }
                    text++;
                    if (name_character_index == UNIT_NAME_SIZE - 1)
                        *text++ = 0xfe;
                }
            }
        }
    }

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        if (g_party_removable_flags[party_index] == 0)
            continue;
        unit_panel_load_party_unit(0, party_index);
        selected_party_index = party_index;
        break;
    }

    unit_panel_copy_billboard_in(
        &g_unit_panel_selected_unit_status, &g_unit_panel_selected_unit, &g_unit_panel_selected_unit_fields);
    g_unit_panel_status_display_redraw_request = 1;
    g_unit_panel_numbers_redraw_request = 1;
    g_unit_panel_character_status_redraw_request = 1;
    thread_yield();

    thread_start(AFTER_BATTLE_THREAD_SLOT_TRANSITION, unit_panel_transition_thread);
    thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_TRANSITION, 0, 0, 0);
    g_gfx_screen_fade = 0xff;
    do {
        thread_yield();
        g_gfx_screen_fade -= 0x10;
    } while (g_gfx_screen_fade > 0);
    g_gfx_screen_fade = 0;

    /* Show the selected unit's portrait: copy its cell and palette from the party
     * sheets in VRAM into the panel's slots. */
    {
        s16* destination;

        destination = &g_party_portrait_destination;
        g_party_portrait_move_rect.x = (selected_party_index % 7) * 8 + 0x100;
        g_party_portrait_move_rect.y = (selected_party_index / 7) * 0x30 + 0x28;
        MoveImage(&g_party_portrait_move_rect, destination[0], destination[1]);
        g_party_palette_move_rect.x = (selected_party_index % 3) * 0x10 + 0x100;
        g_party_palette_move_rect.y = selected_party_index / 3;
        MoveImage(&g_party_palette_move_rect, g_party_palette_destination, g_party_palette_destination_y);
    }

    should_restore = 1;

    for (;;) {
        g_party_removal_result = 0xff;
        thread_start(AFTER_BATTLE_THREAD_SLOT_MESSAGE, menu_icon_linked_entry_thread);
        thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MESSAGE, (s32)&g_party_removal_menus[2], 0, 0);
        portrait_rect = &g_party_portrait_move_rect;
        portrait_destination = &g_party_portrait_destination;
        palette_rect = &g_party_palette_move_rect;
        thread_wait_until_inactive(AFTER_BATTLE_THREAD_SLOT_MESSAGE);
        thread_wait_until_inactive(6);
        /* should_restore still holds 1: an answer of 1, or a cancel, keeps the party. */
        if (g_party_removal_result == should_restore || g_party_removal_result == -1) {
            unit_panel_set_transition(0);
            should_restore = 0;
            result = 1;
            break;
        }
        /* The target keeps the story-unit message branch here, out of line
           at the loop head; written at its test it compiles in place. */
        goto selection_menu;

    show_unit_message:
        thread_start(AFTER_BATTLE_THREAD_SLOT_MESSAGE, message_dialog_thread);
        thread_set_four_parameters(
            AFTER_BATTLE_THREAD_SLOT_MESSAGE, 0x19, message_id, selected_battle_unit->unit_id, should_restore);
        g_threads[AFTER_BATTLE_THREAD_SLOT_MESSAGE].task_words[DIALOG_THREAD_WORD_Y_OFFSET] = 0x18;
        thread_wait_until_inactive(AFTER_BATTLE_THREAD_SLOT_MESSAGE);
        continue;

    selection_menu:
        thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_icon_linked_entry_thread);
        thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MENU, (s32)&g_party_removal_menus[4], 0, 0);
        thread_wait_until_inactive(AFTER_BATTLE_THREAD_SLOT_MENU);
        unit_panel_set_transition(0);
        for (;;) {
            s32 helpmenu_thread;

            thread_yield();
            g_menu_input_controller = script_get_controller_input_pointer(0);
            if (*g_menu_input_controller & PSX_PAD_SELECT) {
                helpmenu_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
                thread_start(helpmenu_thread, help_open_after_battle);
                thread_wait_until_inactive(helpmenu_thread);
                continue;
            }
            /* R1 and L1 step to the next or previous removable unit. The status redraw
             * request doubles as the portrait slide direction: 1 (should_restore) for R1, 2 for L1. */
            if (*g_menu_input_controller & PSX_PAD_R1) {
                g_unit_panel_panel_status_animation[1] = 7;
                for (;;) {
                    selected_party_index++;
                    if (selected_party_index == PARTY_ROSTER_SLOT_COUNT)
                        selected_party_index = 0;
                    if (g_party_removable_flags[selected_party_index] != 0) {
                        g_sound_pending_sfx_id = SOUND_SFX_PAGE_SWITCH;
                        g_unit_panel_status_display_redraw_request = should_restore;
                        g_unit_panel_numbers_redraw_request = should_restore;
                        g_unit_panel_character_status_redraw_request = should_restore;
                        unit_panel_load_party_unit(1, selected_party_index);
                        PARTY_MOVE_PARTY_PORTRAIT(
                            selected_party_index, portrait_rect, portrait_destination, palette_rect);
                        unit_panel_copy_billboard_in(&g_unit_panel_selected_unit_status, &g_unit_panel_selected_unit,
                            &g_unit_panel_selected_unit_fields);
                        break;
                    }
                }
            } else if (*g_menu_input_controller & PSX_PAD_L1) {
                g_unit_panel_panel_status_animation[0] = 7;
                do {
                    selected_party_index--;
                    if (selected_party_index == -1)
                        selected_party_index = PARTY_ROSTER_SLOT_COUNT - 1;
                } while (g_party_removable_flags[selected_party_index] == 0);
                g_sound_pending_sfx_id = SOUND_SFX_PAGE_SWITCH;
                g_unit_panel_status_display_redraw_request = 2;
                g_unit_panel_numbers_redraw_request = should_restore;
                g_unit_panel_character_status_redraw_request = should_restore;
                unit_panel_load_party_unit(1, selected_party_index);
                PARTY_MOVE_PARTY_PORTRAIT(selected_party_index, portrait_rect, portrait_destination, palette_rect);
                unit_panel_copy_billboard_in(&g_unit_panel_selected_unit_status, &g_unit_panel_selected_unit,
                    &g_unit_panel_selected_unit_fields);
            }
            if (*g_menu_input_controller & PSX_PAD_CIRCLE)
                break;
        }

        {
            unit_stats_t* battle_unit;

            /* Find the first battle unit still on the field (its own first ENTD match) to
             * speak the dialog; message_id holds the lookup's index until it is set below. */
            for (party_index = 0; party_index < UNIT_SLOT_COUNT; party_index++) {
                battle_unit = unit_get_stats(party_index);
                selected_battle_unit = unit_entd_find_stats(battle_unit->unit_id, &message_id);
                if (message_id >= 0 && party_index == message_id && unit_get_runtime_id_by_battle_id(party_index) != -1)
                    break;
            }
        }

        party_unit = party_get_unit(selected_party_index);
        /* Generic units get one of eight random messages for their kind; others look up
         * their sprite group, with Ramza's messages apart from the special characters'. */
        formation_index = 0;
        if (party_unit->sprite_set & CHARACTER_IDENTITY_HIGH_BIT_MASK) {
            message_id = ((party_unit->sprite_set - CHARACTER_IDENTITY_SELECTOR_FIRST) * 8)
                + (script_get_variable(EVENT_SCRIPT_VAR_RANDOM_VALUE) & 7) + 0x1800;
        } else {
            s32 sprite_column;
            u8* sprite_group;
            u32 sprite_set;

            sprite_set = party_unit->sprite_set;
            sprite_group = g_after_battle_gfx_formation_sprite_groups[0];
        next_formation:
            for (sprite_column = 0; sprite_column < 3; sprite_column++) {
                if (sprite_set == sprite_group[sprite_column])
                    break;
            }
            if (sprite_column == 3) {
                formation_index++;
                sprite_group += 3;
                if (formation_index < 73)
                    goto next_formation;
            }
            if (formation_index == 73)
                formation_index = 0;
            /* Group 71 shares group 72's message. */
            if (formation_index == 71)
                formation_index = 72;
            if (sprite_set != CHARACTER_IDENTITY_ENTD_NONE && sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
                message_id = formation_index + 0x1862;
            } else {
                message_id = formation_index + 0x1818;
            }
        }

        unit_panel_set_transition(1);
        g_text_substitution_values[0] = selected_party_index;
        /* Ramza only gets his message and the prompt repeats: he cannot be dismissed. */
        if (party_unit->sprite_set < CHARACTER_IDENTITY_RAMZA_END)
            goto show_unit_message;
        g_after_battle_menu_selection_result = 0xff;
        thread_start(AFTER_BATTLE_THREAD_SLOT_MESSAGE, message_dialog_thread);
        thread_set_four_parameters(
            AFTER_BATTLE_THREAD_SLOT_MESSAGE, 0x19, message_id, selected_battle_unit->unit_id, should_restore);
        g_threads[AFTER_BATTLE_THREAD_SLOT_MESSAGE].task_words[DIALOG_THREAD_WORD_Y_OFFSET] = 0x18;
        thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_run_icon_selection_loop);
        thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MENU, (s32)&g_party_removal_menus[0], 0, 0);
        menu_wait_selection_threads();
        if (g_after_battle_menu_selection_result != 0)
            continue;

        should_restore = party_return_equipment(selected_party_index);
        result = 0;
        party_remove_unit(selected_party_index);
        party_set_class_variable(party_unit->sprite_set, PARTY_BIO_STATE_DISMISSED);
        break;
    }

    {
        s32 name_character_index;

        /* Put the battle units' names back into the shared name text. */
        text = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
        for (party_index = 0; party_index < UNIT_SLOT_COUNT; party_index++) {
            selected_battle_unit = unit_get_stats(party_index);
            for (name_character_index = 0; name_character_index < UNIT_NAME_SIZE; name_character_index++) {
                *text = selected_battle_unit->name[name_character_index];
                if ((*text & 0xfe) == 0xfe) {
                    text++;
                    break;
                }
                text++;
                if (name_character_index == UNIT_NAME_SIZE - 1)
                    *text++ = 0xfe;
            }
        }
    }

    /* Never taken: should_restore is 0 on both exits (the item restore always returns 0). */
    if (should_restore != 0) {
        thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_icon_linked_entry_thread);
        thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MENU, (s32)&g_party_removal_menus[5], 0, 0);
        thread_wait_until_inactive(AFTER_BATTLE_THREAD_SLOT_MENU);
    }

    unit_panel_set_transition(0);
    g_gfx_screen_fade = 0;
    do {
        thread_yield();
        g_gfx_screen_fade += 0x10;
    } while (g_gfx_screen_fade < 0x100);
    g_gfx_screen_fade = 0xff;
    thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_TRANSITION, 0, 0, 1);
    return result;
}

/* Applies the post-battle party changes for each affected battle unit.
 *
 * Shows each unit's status panel, lets the player choose whether to recruit
 * it (removing a party member first when the roster is full), then saves it
 * to the party or updates its bio variable. Restores the saved thread block
 * and menu descriptor on exit.
 */
static void party_apply_post_battle_changes(void) {
    s16 text_width_px;
    s16 text_height_px;
    menu_entry_t* saved_menu_descriptor;
    s32 affected_index;
    s32 transition;
    s32* initial_unit_index_ptr;
    s32* unit_index_ptr;
    unit_panel_unit_t* initial_unit_data;
    s32* selected_unit_index_ptr;
    menu_entry_t* menu_base;
    u8* menu_fields;
    unit_panel_unit_t* unit_display_data;
    s32 one;
    unit_stats_t* unit;
    s32 input;
    s32 thread_id;
    s32 text_width;
    s16 menu_x;
    s32 half_menu_x;
    u8* text_data;
    s32* transition_c_ptr;
    s32 screen_fade_max;

    if (party_update_after_battle() == 0) {
        thread_exit_current();
    }

    g_menu_post_battle_unit_changes_active = 1;
    thread_suspend(THREAD_SLOT_EVENT);
    mem_copy_bytes_nonempty(g_after_battle_saved_thread_block, &g_threads[THREAD_SLOT_EVENT], 0x400);

    text_data = g_text_data;
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS] = text_data + g_text_section_offsets[1];
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES] = text_data + g_text_section_offsets[2];
    saved_menu_descriptor = g_menu_thread_menu_data;
    menu_base = g_after_battle_join_menu_descriptor;
    g_menu_thread_menu_data = menu_base;
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES] = text_data + g_text_section_offsets[3];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = text_data + g_text_section_offsets[4];

    affected_index = 0;
    text_build_battle_unit_names();
    initial_unit_index_ptr = g_party_affected_battle_unit_indices;
    initial_unit_data = &g_unit_panel_selected_unit;
    unit_get_stats(*initial_unit_index_ptr);
    g_unit_panel_selected_unit.battle_id = *initial_unit_index_ptr;
    initial_unit_data->name_id = *initial_unit_index_ptr;
    unit_panel_load_selected();
    g_unit_panel_selected_unit_status.unit_index = *initial_unit_index_ptr;

    /* `affected_index < count` (not `count > 0`): combine folds the known
     * zero into a `blez` and leaves the eliminated compare pseudo with a
     * flow reference count, which gives one of the target's two unreferenced
     * 8-byte frame slots. */
    if (affected_index < g_party_affected_unit_count) {
        transition_c_ptr = &g_unit_panel_character_status_origin_x;
        unit_display_data = initial_unit_data;
        one = 1;
        menu_fields = (u8*)&menu_base->inner_width;
        /* Hide the constants from local-alloc, or the s3-s5 assignment rotates. */
        __asm__("" : "=r"(one) : "0"(one));
        __asm__("" : "=r"(menu_fields) : "0"(menu_fields));
        unit_index_ptr = g_party_affected_battle_unit_indices;
        do {
            unit_panel_set_transition(0);
            g_unit_panel_status_display_origin_x = -0x100;
            g_unit_panel_numbers_origin_x = -0x100;
            *transition_c_ptr = -0x100;
            unit_panel_copy_billboard_in(
                &g_unit_panel_selected_unit_status, &g_unit_panel_selected_unit, &g_unit_panel_selected_unit_fields);

            /* This zero feeds the else-arm fade and fills the `bnez` delay
             * slot; the separate zero before the second fade is copied into
             * the if-arm's `j` slot by reorg (after jump2 could have deleted
             * an arm-local duplicate) and moved into the else arm's first
             * call slot on reorg's second pass. */
            transition = 0;
            if (affected_index == 0) {
                thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
                thread_set_parameters(
                    THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_unit_panel_status_display_thread_params, 0, 0);
                thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_polarity_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_numbers_thread_params, 0, 0);
                thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)transition_c_ptr, 0, 0);
            } else {
                u16 index_value;

                /* Slide the previous unit's panels off to the right, then load the next unit. */
                do {
                    g_unit_panel_status_display_origin_x = transition;
                    g_unit_panel_numbers_origin_x = transition;
                    g_unit_panel_character_status_origin_x = transition;
                    transition += 0x20;
                    thread_yield();
                } while (transition < 0x101);

                unit_get_stats(*unit_index_ptr);
                index_value = *unit_index_ptr;
                unit_display_data->battle_id = index_value;
                unit_display_data->name_id = index_value;
                unit_panel_load_selected();
                unit_panel_copy_billboard_in(
                    &g_unit_panel_selected_unit_status, unit_display_data, &g_unit_panel_selected_unit_fields);
                index_value = *unit_index_ptr;
                g_unit_panel_status_display_redraw_request = one;
                g_unit_panel_numbers_redraw_request = one;
                g_unit_panel_character_status_redraw_request = one;
                g_unit_panel_selected_unit_status.unit_index = index_value;
                thread_yield();
            }

            /* Slide the panels in from the left. */
            transition = 0;
            do {
                g_unit_panel_status_display_origin_x = transition - 0x100;
                g_unit_panel_numbers_origin_x = transition - 0x100;
                g_unit_panel_character_status_origin_x = transition - 0x100;
                transition += 0x20;
                thread_yield();
            } while (transition < 0x101);

            /* The removal retry re-enters the selection loop and the
             * not-joining retry re-enters the removal loop from outside, so
             * loop.c treats both loops as having multiple entries and hoists
             * nothing out of them (the g_unit_panel_selected_unit_status.unit_index address stays in the
             * remove path), while flow still weights their references by
             * depth. The not-joining retry repeats the selected-unit
             * assignment; jump2 cross-jumps it into this one, so the
             * `bne v0,s4` at +0x6a4 re-enters at +0x2d0, one instruction
             * before the two retry_selection entries at +0x2d4. */
            selected_unit_index_ptr = unit_index_ptr;
            do {
            retry_selection:
                while (1) {
                    unit_panel_set_transition(0);
                    do {
                        thread_yield();
                        g_menu_input_controller = script_get_controller_input_pointer(0);
                        input = *g_menu_input_controller;
                        if (input & PSX_PAD_CIRCLE) {
                            break;
                        }
                        if (input & PSX_PAD_SELECT) {
                            thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
                            thread_start(thread_id, help_open_after_battle);
                            thread_wait_until_inactive(thread_id);
                        }
                    } while (1);

                    unit_panel_set_transition(1);
                    /* Fit the join prompt window to text 0x5006 (with this unit's name) and center it at x 0x100. */
                    g_text_substitution_values[0] = *selected_unit_index_ptr;
                    text_measure_pixels(&text_width_px, &text_height_px, text_find_entry(0x5006));
                    /* The s16 sum reads the 16-bit value directly (a second copy of the
                     * loaded width) while the modulo uses the widened s32 value; the
                     * narrowing also leaves the other unreferenced frame slot. */
                    text_width = text_width_px;
                    half_menu_x = text_width % 4 + 0x18;
                    menu_x = text_width_px + half_menu_x;
                    half_menu_x = menu_x / 2;
                    /* A recovered struct pointer hoists the owner address across the call (two instructions differ).
                     * Derive each byte displacement from the named field instead. */
#define MENU_FIELD(field)                                                                                              \
    (*(s16*)(menu_fields + (unsigned long)&((menu_entry_t*)0)->field - (unsigned long)&((menu_entry_t*)0)->inner_width))
                    MENU_FIELD(text_image.width) = menu_x;
                    MENU_FIELD(window.w) = menu_x;
                    MENU_FIELD(inner_width) = menu_x;
                    MENU_FIELD(window.x) = 0x100 - half_menu_x + menu_x % 2;
#undef MENU_FIELD

                    /* A guest (status 2) joins without the prompt. The store goes to
                     * g_after_battle_message_menu_result, which no code reads, so the checks below see the selection
                     * result left from before. */
                    if (party_get_join_status(*selected_unit_index_ptr) == 2) {
                        g_after_battle_message_menu_result = 0;
                    } else {
                        g_after_battle_menu_selection_result = 0xff;
                        thread_start(AFTER_BATTLE_THREAD_SLOT_MESSAGE, menu_icon_strip_thread);
                        thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MESSAGE,
                            (s32)(menu_fields - (unsigned long)&((menu_entry_t*)0)->inner_width), 0, 0);
                        if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
                            thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_run_icon_selection_loop);
                            thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MENU,
                                (s32)(menu_fields + 8 * sizeof(*menu_base)
                                    - (unsigned long)&((menu_entry_t*)0)->inner_width),
                                0, 0);
                        } else {
                            thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_run_icon_selection_loop);
                            thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MENU,
                                (s32)(menu_fields + sizeof(*menu_base)
                                    - (unsigned long)&((menu_entry_t*)0)->inner_width),
                                0, 0);
                        }
                    }

                    menu_wait_selection_threads();
                    if (g_after_battle_menu_selection_result != -1) {
                        break;
                    }
                    unit_panel_set_transition(0);
                }
                /* 0 joins; a full roster (status 0) first asks for a member to dismiss. */
                if (g_after_battle_menu_selection_result != 0) {
                    goto not_joining;
                }
                if (party_get_join_status(*selected_unit_index_ptr) != 0) {
                    break;
                }
                {
                    u16 index_value;
                    u16 display_index;
                    s32 removal_result;
                    s32 removal_unit_index;

                    g_gfx_screen_fade = 0;
                    do {
                        thread_yield();
                        g_gfx_screen_fade += 0x10;
                    } while (g_gfx_screen_fade < 0x100);
                    screen_fade_max = 0xff;
                    g_gfx_screen_fade = screen_fade_max;
                    /* The status thread picks its panel cell from this flag; clear it while the removal screen runs. */
                    g_menu_post_battle_unit_changes_active = 0;
                    removal_result = party_select_unit_to_dismiss();
                    /* The target loads *selected_unit_index_ptr at 0x514,
                     * above both global stores. GCC 2.6.3 emits a
                     * straight-line block in strict source order, so the
                     * load has to be written above them. */
                    removal_unit_index = *selected_unit_index_ptr;
                    g_party_removal_result = removal_result;
                    g_menu_post_battle_unit_changes_active = one;

                    unit_get_stats(removal_unit_index);
                    /* The target reads the index into v0 here and reloads it
                     * into v1 after the call; one shared local is one allocno
                     * and takes v1 at both sites. */
                    display_index = *selected_unit_index_ptr;
                    unit_display_data->battle_id = display_index;
                    unit_display_data->name_id = display_index;
                    unit_panel_load_selected();
                    index_value = *selected_unit_index_ptr;
                    g_unit_panel_status_display_redraw_request = one;
                    g_unit_panel_numbers_redraw_request = one;
                    g_unit_panel_character_status_redraw_request = one;
                    g_unit_panel_selected_unit_status.unit_index = index_value;
                    if (g_party_removal_result == 0) {
                        unit_panel_set_transition(1);
                    }
                    unit_panel_copy_billboard_in(
                        &g_unit_panel_selected_unit_status, unit_display_data, &g_unit_panel_selected_unit_fields);
                    g_gfx_screen_fade = screen_fade_max;
                    do {
                        thread_yield();
                        g_gfx_screen_fade -= 0x10;
                    } while (g_gfx_screen_fade > 0);
                    g_gfx_screen_fade = 0;
                }
            } while (g_party_removal_result != 0);

            /* Equipment the inventory cannot hold is sold; menu entry 5 says so. */
            if (party_sell_equipment_over_limit(*selected_unit_index_ptr) != 0) {
                thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_icon_linked_entry_thread);
                thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MENU, (s32)&g_party_removal_menus[5], 0, 0);
                thread_wait_until_inactive(AFTER_BATTLE_THREAD_SLOT_MENU);
            }

            /* Show special condition 10 for 60 frames or until skipped; a third parameter of 2 fades it out. */
            thread_wait_frames(20);
            thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, conditions_special_thread);
            thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MENU, 10, 0, 0);
            input_wait_frames_or_skip(60);
            thread_set_parameters(AFTER_BATTLE_THREAD_SLOT_MENU, 0, 0, 2);
            thread_wait_until_inactive(AFTER_BATTLE_THREAD_SLOT_MENU);
            unit_panel_set_transition(0);

            unit = unit_get_stats(*selected_unit_index_ptr);
            unit_index_ptr++;
            party_save_unit(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
            thread_wait_frames(20);
            goto next_unit;
        not_joining:
            /* Answer 1 declines; any other answer asks again. */
            if (g_after_battle_menu_selection_result != one) {
                selected_unit_index_ptr = unit_index_ptr;
                goto retry_selection;
            }
            unit_panel_set_transition(0);
            thread_wait_frames(10);
            unit = unit_get_stats(*selected_unit_index_ptr);
            unit_index_ptr++;
            party_set_class_variable(unit->character_identity, PARTY_BIO_STATE_DISMISSED);
        next_unit:
            affected_index++;
        } while (affected_index < g_party_affected_unit_count);
    }

    transition = 0;
    do {
        g_unit_panel_status_display_origin_x = transition;
        g_unit_panel_numbers_origin_x = transition;
        g_unit_panel_character_status_origin_x = transition;
        transition += 0x20;
        thread_yield();
    } while (transition < 0x101);

    /* Stop the three panel threads, then give the event thread its saved block back. */
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SCREEN_SLOT_UNIT_STATUS);
    thread_wait_until_inactive(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    thread_wait_until_inactive(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
    g_menu_post_battle_unit_changes_active = 0;
    mem_copy_bytes_nonempty(&g_threads[THREAD_SLOT_EVENT], g_after_battle_saved_thread_block, 0x400);
    g_menu_thread_menu_data = saved_menu_descriptor;
    thread_resume(THREAD_SLOT_EVENT);
    thread_yield();
    thread_exit_current();
}

/*
 * Builds the war-trophy entry list and pays the trophies out.
 *
 * The bonus money (when nonzero) is the first entry, followed by one entry
 * per trophy item found by reward_find_war_trophies_and_bonus_money.
 * Money entries are added to the war funds. An item entry adds one to the
 * inventory, unless 99 are already held or equipped: the item is then sold
 * for a quarter of its price, rounded toward zero.
 */
static void war_trophy_collect(void) {
    s32 i;
    s32 item_id;
    s32 quantity;
    s32 price;
    s32 adj;
    s32 funds;
    s32 item_count;
    item_data_t* item;
    u8* qp;
    u32 pad[4];
    s32* iv;
    s32* it;

    g_war_trophy_entry_count = 0;
    reward_find_war_trophies_and_bonus_money(&g_war_trophy_search_result);

    {
        s32 j;
        s32 next;
        iv = g_war_trophy_entry_values;
        it = g_war_trophy_entry_types;
        j = g_war_trophy_entry_count;
        iv[j] = g_war_trophy_search_result.bonus_money;
        it[j] = WAR_TROPHY_ENTRY_BONUS_MONEY;
        if (iv[j] != 0) {
            next = j + 1;
            g_war_trophy_entry_count = next;
        }
    }

    i = 0;
    item_count = g_war_trophy_item_count;
    if (item_count > 0) {
        s32* tb;
        s32* vb = iv;
        u8* cp;
        s32 entry;
        s32 loop_count;
        tb = it;
        cp = &g_war_trophy_search_result.item_count;
        do {
            entry = g_war_trophy_entry_count;
            item_id = g_war_trophy_search_result.item_ids[i];
            i++;
            vb[entry] = item_id;
            tb[entry] = WAR_TROPHY_ENTRY_ITEM;
            loop_count = *cp;
            g_war_trophy_entry_count = entry + 1;
        } while (i < loop_count);
    }

    i = 0;
    if (g_war_trophy_entry_count > 0) {
        s32* vp = g_war_trophy_entry_values;
        s32* tp = g_war_trophy_entry_types;
        do {
            if (*tp == WAR_TROPHY_ENTRY_ITEM) {
                item_id = *vp;
                quantity = item_get_total_quantity(item_id, 1);
                if (quantity == 99) {
                    item = item_get_data(*vp);
                    price = item->price;
                    if (price < 0) {
                        adj = price + 3;
                    } else {
                        adj = price;
                    }
                    price = adj >> 2;
                    funds = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
                    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, funds + price);
                } else {
                    s32 id = *vp;
                    qp = &g_item_quantities[id];
                    (*qp)++;
                }
            } else {
                funds = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
                script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, funds + *vp);
            }
            vp++;
            tp++;
        } while (++i < g_war_trophy_entry_count);
    }
}

/* Screen x of war-trophy entry `value`'s column, given the entry count
 * `index`: one column at 0xd0 for fewer than five entries, otherwise 0x9c
 * for even entries and 0x104 for odd ones. */
static s32 war_trophy_get_column_x(s32 value, s32 index) {
    s32 result = 0xd0;

    if (index >= 5) {
        s32 alternate = 0x9c;

        if (value & 1) {
            alternate = 0x104;
        }
        result = alternate;
    }
    return result;
}

/* Screen y of war-trophy row `value`, given the entry count `index`: 20
 * pixels per entry in the single column, per pair of entries in two
 * columns, from y 0x78. */
static s32 war_trophy_get_row_y(s32 value, s32 index) {
    if (index < 5) {
        return value * 20 + 0x78;
    }
    return (value / 2) * 20 + 0x78;
}

/*
 * Thread body of the post-battle "war trophies" list.
 *
 * Collects the entries (war_trophy_collect)
 * and exits at once when there are none. Otherwise it starts the banner
 * thread and reveals the entries eight to a page: a new entry appears every
 * 0x1f frames or on Circle, and the last entry of a page stays for 0x79
 * frames before the page is cleared. Fewer than five entries form one column,
 * more form two. An item entry is its 16x16 icon and name; any other entry is
 * a number followed by an 18x8 label glyph. Each new line of text is uploaded to
 * VRAM and its buffer freed on the following frame.
 */
static void war_trophy_list_thread(void) {
    SPRT sprite;
    s32 start;
    s32 last;
    s32 row;
    s32 i;
    POLY_FT4* quad;
    war_trophy_primitives_t* buffer;
    war_trophy_primitives_t* prims;
    u8* image;
    s32 frame;
    s32 loaded;
    s32 timer;
    s32 thread;

    war_trophy_collect();

    if (g_war_trophy_entry_count == 0) {
        thread_exit_current();
    }

    prims = g_war_trophy_primitives;
    thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    last = 0;
    thread_start(thread, conditions_special_thread);
    thread_set_parameters(thread, 4, 0, 0);

    for (start = 0; start < g_war_trophy_entry_count; start += 8) {
        row = 0;
        /* Clear the previous page's text from the banner texture. */
        ClearImage(&g_war_trophy_clear_rect, 0, 0, 0);
        thread_yield();

        buffer = g_war_trophy_primitives;
        gfx_set_draw_mode_for_texture_page(&buffer->draw_mode, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
        for (i = 0; i < 16; i++) {
            menu_init_default_poly_ft4(&buffer->quads[i]);
        }
        menu_init_sprt(&buffer->banner);
        prims->banner.x0 = 0x88;
        prims->banner.y0 = 0x78;
        prims->banner.u0 = 8;
        prims->banner.v0 = 0x78;
        prims->banner.h = 0x78;
        prims->banner.w = 0xf0;
        prims->banner.clut = 0x7cbc;
        mem_copy_bytes_nonempty(&prims[1], g_war_trophy_primitives, sizeof(war_trophy_primitives_t));

        frame = 0;
        while (1) {
            /* Typed buffer indexing swaps the ADDU operands. */
            buffer = (war_trophy_primitives_t*)((frame & 1) * sizeof(war_trophy_primitives_t) + (s32)prims);

            if ((*g_menu_input_controller & PSX_PAD_CIRCLE) || frame == 0
                || ((row != 7 && g_war_trophy_entry_count - 1 != last) && timer >= 0x1f)
                || ((row == 7 || g_war_trophy_entry_count - 1 == last) && timer >= 0x79)) {
                if (frame != 0) {
                    row++;
                    last++;
                }
                if (last == g_war_trophy_entry_count || (row != 0 && (row & 7) == 0)) {
                    break;
                }

                image = menu_build_and_upload_window_frame_image(
                    0x50, 0x10, &g_war_trophy_image_rect, MENU_WINDOW_FRAME_BUILD);
                menu_clear_render_buffer(image, 0x280);
                g_menu_text_state.pen.stride = 0x50;
                timer = 0;
                if (g_war_trophy_entry_types[last] == WAR_TROPHY_ENTRY_ITEM) {
                    menu_set_text_origin(0, 0);
                    menu_display_text_entry(
                        g_war_trophy_entry_values[last] + TEXT_ID_ITEM_NAME_BASE, image, &g_menu_text_state.pen);
                } else {
                    menu_set_text_origin(0, 0);
                    menu_draw_large_number_glyphs(g_war_trophy_entry_values[last],
                        text_count_decimal_digits(g_war_trophy_entry_values[last]), image, &g_menu_text_state.pen);
                }

                /* Text lines go into the texture the banner sprite draws, where texture x + 0x80
                 * is screen x, so each line lands just right of its icon. */
                if (g_war_trophy_entry_count < 5) {
                    g_war_trophy_image_rect.x = 0x60;
                    g_war_trophy_image_rect.y = row * 20 + 0x78;
                } else {
                    g_war_trophy_image_rect.x = (row & 1) ? 0x94 : 0x2c;
                    g_war_trophy_image_rect.y = (row / 2) * 20 + 0x78;
                }
                /* Four 4-bit texels per VRAM word, from VRAM x 0x1c0. */
                g_war_trophy_image_rect.x = g_war_trophy_image_rect.x / 4 + 0x1c0;
                /* No-op store, kept from the original code. */
                g_war_trophy_image_rect.y = g_war_trophy_image_rect.y;
                loaded = 1;
                LoadImage(&g_war_trophy_image_rect, (u32*)image);
            }

            /* LoadImage reads the buffer asynchronously, so it is freed a frame later. */
            thread_yield();
            if (loaded) {
                menu_free_memory(image);
                loaded = 0;
            }

            /* Quads are indexed by entry number, not by position on the page. */
            for (i = start; i <= last; i++) {
                quad = &buffer->quads[i];
                if (g_war_trophy_entry_types[i] == WAR_TROPHY_ENTRY_ITEM) {
                    quad->x0 = war_trophy_get_column_x(i, g_war_trophy_entry_count);
                    quad->y0 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count);
                    quad->x1 = war_trophy_get_column_x(i, g_war_trophy_entry_count) + 0x10;
                    quad->y1 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count);
                    quad->x2 = war_trophy_get_column_x(i, g_war_trophy_entry_count);
                    quad->y2 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 0x10;
                    quad->x3 = war_trophy_get_column_x(i, g_war_trophy_entry_count) + 0x10;
                    quad->y3 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 0x10;
                    gfx_set_item_icon_sprite(&sprite, g_war_trophy_entry_values[i]);
                    setUV4(quad, sprite.u0, sprite.v0, sprite.u0 + 0x10, sprite.v0, sprite.u0, sprite.v0 + 0x10,
                        sprite.u0 + 0x10, sprite.v0 + 0x10);
                    quad->clut = sprite.clut;
                    quad->tpage = GetTPage(0, 0, 0x380, 0x120);
                    gfx_draw_or_append_gpu_primitive(quad);
                } else {
                    quad->x0 = war_trophy_get_column_x(i, g_war_trophy_entry_count)
                        + text_count_decimal_digits(g_war_trophy_entry_values[i]) * 7 + 0x12;
                    quad->y0 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 4;
                    quad->x1 = war_trophy_get_column_x(i, g_war_trophy_entry_count)
                        + text_count_decimal_digits(g_war_trophy_entry_values[i]) * 7 + 0x24;
                    quad->y1 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 4;
                    quad->x2 = war_trophy_get_column_x(i, g_war_trophy_entry_count)
                        + text_count_decimal_digits(g_war_trophy_entry_values[i]) * 7 + 0x12;
                    quad->y2 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 0xc;
                    quad->x3 = war_trophy_get_column_x(i, g_war_trophy_entry_count)
                        + text_count_decimal_digits(g_war_trophy_entry_values[i]) * 7 + 0x24;
                    quad->y3 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 0xc;
                    setUV4(quad, 0x76, 0x10, 0x88, 0x10, 0x76, 0x18, 0x88, 0x18);
                    quad->clut = 0x7cbc;
                    quad->tpage = GetTPage(0, 0, 0x3c0, 0x100);
                    gfx_draw_or_append_gpu_primitive(quad);
                }
            }

            gfx_draw_or_append_gpu_primitive(&buffer->banner);
            gfx_draw_or_append_gpu_primitive(&buffer->draw_mode);
            frame++;
            timer++;
        }
        if (loaded) {
            menu_free_memory(image);
        }
    }

    thread_set_parameters(thread, 0, 0, 1);
    thread_wait_until_inactive(thread);
    thread_exit_current();
}

#define X0 (0x108 - j * 13 - (5 - digits) * 13 / 2)

/*
 * Post-battle gil ticker: adds the war-result money to the war funds (EVENT_SCRIPT_VAR_WAR_FUNDS),
 * starts the display-condition thread, splits the amount into up to six
 * digits and rolls each digit's odometer strip (13-pixel cells, counter << 13
 * fixed point, slowing by 0x44 per step once the target digit comes round)
 * before holding for 60 frames and fading out.
 *
 * The polygon pointers are formed as `(s32)array + offset` byte offsets, which
 * keeps the target's separate base and offset; `&g_war_trophy_money_digit_polys[buffer][j]` is reduced
 * to a single pointer induction variable. `pass` doubles as the fade level,
 * which the target's register allocation reflects.
 */
static void war_trophy_run_gil_ticker(void) {
    DR_AREA areas[4];
    s32 thread_id;
    s32 money;
    s32 digits;
    s32 i;
    s32 j;
    s32 buffer;
    s32 tick;
    s32 pass;
    s32 x;
    s32 off;
    POLY_FT4* commas;
    POLY_FT4* comma;
    POLY_FT4* sign;

    g_war_trophy_entry_count = 0;
    reward_find_war_trophies_and_bonus_money(&g_war_trophy_search_result);
    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS,
        script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + g_war_trophy_search_result.level_sum_money);
    money = g_war_trophy_search_result.level_sum_money;

    thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_id, conditions_special_thread);
    thread_set_parameters(thread_id, 2, 0, 0);
    g_menu_input_controller = script_get_controller_input_pointer(0);

    /* `i = 0` here (not before the script calls) lets sched1 keep
     * `li a0,0x2c` above the money load. */
    for (i = 0; i < 6; i++) {
        g_war_trophy_money_digit_speeds[i] = g_war_trophy_money_digit_initial_speeds[i];
        g_war_trophy_money_digit_settling[i] = 0;
    }

    digits = text_count_decimal_digits(money);
    /* There are six digit strips; a longer amount is reported and shows as 0. */
    if (digits >= 7) {
        system_report_error(7, 0);
        money = 0;
        digits = 1;
    }

    /* Each strip starts on its target digit; one digit is 1 << 13 position units. */
    j = money;
    for (i = 0; i < digits; i++) {
        g_war_trophy_money_digit_targets[i] = j % 10;
        j /= 10;
        g_war_trophy_money_digit_positions[i] = g_war_trophy_money_digit_targets[i] << 13;
    }

    g_sound_pending_sfx_id = SOUND_SFX_GIL_TICKER;
    for (buffer = 0; buffer < 2; buffer++) {
        for (j = 0; j < digits; j++) {
            POLY_FT4* pd;
            POLY_FT4* pn;

            menu_init_default_poly_ft4(&g_war_trophy_money_digit_polys[buffer][j]);
            menu_init_default_poly_ft4(&g_war_trophy_money_next_digit_polys[buffer][j]);
            g_war_trophy_money_digit_polys[buffer][j].tpage = getTPage(0, 0, 384, 0);
            g_war_trophy_money_digit_polys[buffer][j].clut
                = getClut(g_conditions_banner_palette_rect.x, g_conditions_banner_palette_rect.y);
            g_war_trophy_money_next_digit_polys[buffer][j].tpage = getTPage(0, 0, 384, 0);
            g_war_trophy_money_next_digit_polys[buffer][j].clut
                = getClut(g_conditions_banner_palette_rect.x, g_conditions_banner_palette_rect.y);
            pd = (POLY_FT4*)((s32)g_war_trophy_money_digit_polys + (j * 0x28 + buffer * 0xf0));
            setXY4(pd, X0, 0x80, X0 + 13, 0x80, X0, 0x97, X0 + 13, 0x97);
            pn = (POLY_FT4*)((s32)g_war_trophy_money_next_digit_polys + (j * 0x28 + buffer * 0xf0));
            setXY4(pn, X0, 0x69, X0 + 13, 0x69, X0, 0x80, X0 + 13, 0x80);
            /* Digits from the thousands up move 8 pixels left to make room for the comma.
             * Only x0 moves here; the roll loop rebuilds the other vertices from it. */
            if (j >= 3) {
                g_war_trophy_money_digit_polys[buffer][j].x0 -= 8;
                g_war_trophy_money_next_digit_polys[buffer][j].x0 -= 8;
            }
        }
    }

    for (i = 0; i < 2; i++) {
        /* Block-local pointers: local-alloc then builds each address in the
         * pointer's own register, as the target does. */
        POLY_FT4* comma_poly;
        POLY_FT4* sign_poly;

        comma_poly = &g_war_trophy_money_comma_polys[i];
        menu_init_default_poly_ft4(comma_poly);
        setXY4(comma_poly, g_war_trophy_money_digit_polys[0][2].x0 - 10, 0x8f,
            g_war_trophy_money_digit_polys[0][2].x0 - 4, 0x8f, g_war_trophy_money_digit_polys[0][2].x0 - 10, 0x99,
            g_war_trophy_money_digit_polys[0][2].x0 - 4, 0x99);
        setUV4(comma_poly, 0x82, 0x40, 0x88, 0x40, 0x82, 0x4a, 0x88, 0x4a);
        g_war_trophy_money_comma_polys[i].tpage = getTPage(0, 0, 384, 0);
        g_war_trophy_money_comma_polys[i].clut
            = getClut(g_conditions_banner_palette_rect.x, g_conditions_banner_palette_rect.y);
        sign_poly = &g_war_trophy_money_sign_polys[i];
        menu_init_default_poly_ft4(sign_poly);
        setXY4(sign_poly, g_war_trophy_money_digit_polys[0][0].x0 + 0x11, 0x80,
            g_war_trophy_money_digit_polys[0][0].x0 + 0x31, 0x80, g_war_trophy_money_digit_polys[0][0].x0 + 0x11, 0x97,
            g_war_trophy_money_digit_polys[0][0].x0 + 0x31, 0x97);
        setUV4(sign_poly, 0x88, 0x32, 0xa8, 0x32, 0x88, 0x49, 0xa8, 0x49);
        g_war_trophy_money_sign_polys[i].tpage = getTPage(0, 0, 384, 0);
        g_war_trophy_money_sign_polys[i].clut
            = getClut(g_conditions_banner_palette_rect.x, g_conditions_banner_palette_rect.y);
    }

    /* Per display buffer, areas 0-1 clip the rolling digits to their window and
     * areas 2-3 restore the full screen afterwards. */
    if (g_gfx_screen_polarity == 0) {
        SetDrawArea(&areas[0], &g_war_trophy_money_clip_rect_0);
        SetDrawArea(&areas[1], &g_war_trophy_money_clip_rect_1);
        SetDrawArea(&areas[2], &g_war_trophy_screen_clip_rect_0);
        SetDrawArea(&areas[3], &g_war_trophy_screen_clip_rect_1);
    } else {
        SetDrawArea(&areas[0], &g_war_trophy_money_clip_rect_1);
        SetDrawArea(&areas[1], &g_war_trophy_money_clip_rect_0);
        SetDrawArea(&areas[2], &g_war_trophy_screen_clip_rect_1);
        SetDrawArea(&areas[3], &g_war_trophy_screen_clip_rect_0);
    }

    tick = 0;
    i = 0;
    for (;;) {
        buffer = i & 1;
        thread_yield();
        gfx_draw_or_append_gpu_primitive(&areas[buffer + 2]);
        for (pass = 0; pass < 2; pass++, tick++) {
            for (j = 0; j < digits; j++) {
                POLY_FT4* pd;
                POLY_FT4* pn;

                g_war_trophy_money_digit_positions[j] += g_war_trophy_money_digit_speeds[j];
                off = j * 0x28 + buffer * 0xf0;
                x = g_war_trophy_money_digit_polys[buffer][j].x0;
                pd = (POLY_FT4*)((s32)g_war_trophy_money_digit_polys + off);
                setXY4(pd, x, ((g_war_trophy_money_digit_positions[j] >> 8) & 0x1f) + 0x80, x + 13,
                    ((g_war_trophy_money_digit_positions[j] >> 8) & 0x1f) + 0x80, x,
                    ((g_war_trophy_money_digit_positions[j] >> 8) & 0x1f) + 0x97, x + 13,
                    ((g_war_trophy_money_digit_positions[j] >> 8) & 0x1f) + 0x97);
                setUV4(pd, (g_war_trophy_money_digit_positions[j] >> 13) % 10 * 13, 0x32,
                    (g_war_trophy_money_digit_positions[j] >> 13) % 10 * 13 + 13, 0x32,
                    (g_war_trophy_money_digit_positions[j] >> 13) % 10 * 13, 0x49,
                    (g_war_trophy_money_digit_positions[j] >> 13) % 10 * 13 + 13, 0x49);
                x = g_war_trophy_money_next_digit_polys[buffer][j].x0;
                off = j * 0x28 + buffer * 0xf0;
                pn = (POLY_FT4*)((s32)g_war_trophy_money_next_digit_polys + off);
                setXY4(pn, x, g_war_trophy_money_digit_polys[buffer][j].y0 - 0x20, x + 13,
                    g_war_trophy_money_digit_polys[buffer][j].y0 - 0x20, x,
                    g_war_trophy_money_digit_polys[buffer][j].y0 - 9, x + 13,
                    g_war_trophy_money_digit_polys[buffer][j].y0 - 9);
                setUV4(pn, ((g_war_trophy_money_digit_positions[j] >> 13) + 1) % 10 * 13, 0x32,
                    ((g_war_trophy_money_digit_positions[j] >> 13) + 1) % 10 * 13 + 13, 0x32,
                    ((g_war_trophy_money_digit_positions[j] >> 13) + 1) % 10 * 13, 0x49,
                    ((g_war_trophy_money_digit_positions[j] >> 13) + 1) % 10 * 13 + 13, 0x49);
                /* Once its turn comes (higher digits later), a digit slows from six short of its
                 * target and stops on it when its cell is exactly aligned. */
                if (j * 32 + (digits - 1) * 16 < tick) {
                    if (((g_war_trophy_money_digit_positions[j] >> 13) + 6) % 10
                        == g_war_trophy_money_digit_targets[j]) {
                        g_war_trophy_money_digit_settling[j] = 1;
                    }
                    if (g_war_trophy_money_digit_settling[j] == 1) {
                        if (g_war_trophy_money_digit_speeds[j] > 0x100) {
                            g_war_trophy_money_digit_speeds[j] -= 0x44;
                        } else {
                            g_war_trophy_money_digit_speeds[j] = 0x100;
                            if ((g_war_trophy_money_digit_positions[j] >> 13) % 10
                                    == g_war_trophy_money_digit_targets[j]
                                && ((g_war_trophy_money_digit_positions[j] >> 8) & 0x1f) == 0) {
                                g_war_trophy_money_digit_speeds[j] = 0;
                                if (j == digits - 1) {
                                    /* The last digit settled: leave all three loops. */
                                    goto finished;
                                }
                            }
                        }
                    }
                }
            }
        }

        /* Digits appear one every 16 frames, ones digit first. */
        for (j = 0; j < digits; j++) {
            if (j * 16 < i) {
                gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_digit_polys[buffer][j]);
                gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_next_digit_polys[buffer][j]);
            }
        }
        gfx_draw_or_append_gpu_primitive(&areas[buffer]);
        /* The comma appears with the thousands digit. */
        if (digits >= 4 && i >= 0x31) {
            gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_comma_polys[buffer]);
        }
        gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_sign_polys[buffer]);
        if (*g_menu_input_controller & PSX_PAD_CIRCLE) {
            /* Confirm skips the settled-digit frame drawn at `finished`. */
            goto skipped;
        }
        i++;
    }

finished:
    for (j = 0; j < digits; j++) {
        gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_digit_polys[buffer][j]);
        gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_next_digit_polys[buffer][j]);
    }
    gfx_draw_or_append_gpu_primitive(&areas[buffer]);
    if (digits >= 4) {
        gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_comma_polys[buffer]);
    }
    gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_sign_polys[buffer]);

skipped:
    g_sound_pending_sfx_id = SOUND_SFX_NONE;
    i++;
    tick = 0;
    for (;;) {
        buffer = i & 1;
        thread_yield();
        gfx_draw_or_append_gpu_primitive(&areas[buffer + 2]);
        for (j = 0; j < digits; j++) {
            POLY_FT4* digit_quad;

            x = g_war_trophy_money_digit_polys[buffer][j].x0;
            digit_quad = (POLY_FT4*)((s32)g_war_trophy_money_digit_polys + (j * 0x28 + buffer * 0xf0));
            setXY4(digit_quad, x, 0x80, x + 13, 0x80, x, 0x97, x + 13, 0x97);
            setUV4(digit_quad, g_war_trophy_money_digit_targets[j] * 13, 0x32,
                g_war_trophy_money_digit_targets[j] * 13 + 13, 0x32, g_war_trophy_money_digit_targets[j] * 13, 0x49,
                g_war_trophy_money_digit_targets[j] * 13 + 13, 0x49);
            gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_digit_polys[buffer][j]);
        }
        gfx_draw_or_append_gpu_primitive(&areas[buffer]);
        tick++;
        if (digits >= 4) {
            gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_comma_polys[buffer]);
        }
        gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_sign_polys[buffer]);
        if (tick == 60 || (*g_menu_input_controller & PSX_PAD_CIRCLE)) {
            break;
        }
        i++;
    }

    /* Stop the banner thread and fade the amount out; `pass` now holds the color level. */
    thread_set_parameters(thread_id, 0, 0, 1);
    i++;
    pass = 0x80;
    do {
        buffer = i & 1;
        thread_yield();
        gfx_draw_or_append_gpu_primitive(&areas[buffer + 2]);
        for (j = 0; j < digits; j++) {
            POLY_FT4* digit_quad;

            x = g_war_trophy_money_digit_polys[buffer][j].x0;
            digit_quad = (POLY_FT4*)((s32)g_war_trophy_money_digit_polys + (j * 0x28 + buffer * 0xf0));
            setXY4(digit_quad, x, 0x80, x + 13, 0x80, x, 0x97, x + 13, 0x97);
            setUV4(digit_quad, g_war_trophy_money_digit_targets[j] * 13, 0x32,
                g_war_trophy_money_digit_targets[j] * 13 + 13, 0x32, g_war_trophy_money_digit_targets[j] * 13, 0x49,
                g_war_trophy_money_digit_targets[j] * 13 + 13, 0x49);
            setRGB0(digit_quad, pass, pass, pass);
            gfx_draw_or_append_gpu_primitive(&g_war_trophy_money_digit_polys[buffer][j]);
        }
        gfx_draw_or_append_gpu_primitive(&areas[buffer]);
        /* Loading the base first keeps `la g_war_trophy_money_comma_polys` above buffer * 40. */
        commas = g_war_trophy_money_comma_polys;
        comma = commas + buffer;
        sign = &g_war_trophy_money_sign_polys[buffer];
        setRGB0(comma, pass, pass, pass);
        setRGB0(sign, pass, pass, pass);
        if (digits >= 4) {
            gfx_draw_or_append_gpu_primitive(comma);
        }
        gfx_draw_or_append_gpu_primitive(sign);
        pass -= 12;
        i++;
    } while (pass >= 0x30);
    thread_wait_until_inactive(thread_id);
    thread_exit_current();
}

/* Scale each vertex color by scale / 128. */
static void gfx_scale_poly_gt4_colors(const conditions_colors_t* colors, s32 scale, POLY_GT4* poly) {
    s32 r;
    s32 g;
    s32 b;

    r = colors->vertices[0].r * scale / 128;
    g = colors->vertices[0].g * scale / 128;
    b = colors->vertices[0].b * scale / 128;
    setRGB0(poly, r, g, b);
    r = colors->vertices[1].r * scale / 128;
    g = colors->vertices[1].g * scale / 128;
    b = colors->vertices[1].b * scale / 128;
    setRGB1(poly, r, g, b);
    r = colors->vertices[2].r * scale / 128;
    g = colors->vertices[2].g * scale / 128;
    b = colors->vertices[2].b * scale / 128;
    setRGB2(poly, r, g, b);
    r = colors->vertices[3].r * scale / 128;
    g = colors->vertices[3].g * scale / 128;
    b = colors->vertices[3].b * scale / 128;
    setRGB3(poly, r, g, b);
}

/* Initialize the 32 semi-transparent POLY_GT4s of g_gfx_poly_gt4_array_32
 * with zeroed vertices, a one-texel texture cell, texture page 6 and the CLUT
 * at the bonus palette rectangle's origin offset by (x, y). */
static void gfx_init_poly_gt4_array_32(s32 x, s32 y) {
    s32 count = 0;
    s32 one = 1;
    s16* palette_origin;

    /* The y-centered pointer preserves the retail base address; RECT fields change it. */
    palette_origin = &g_conditions_banner_palette_rect.y;
    do {
        POLY_GT4* poly = &g_gfx_poly_gt4_array_32[count];
        SetPolyGT4(poly);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        setUV4(poly, 0, 0, one, 0, 0, one, one, one);
        setXY4(poly, 0, 0, 0, 0, 0, 0, 0, 0);
        /* The graph type is read, and read again when it is not 1, but never used. */
        if (GetGraphType() != one) {
            GetGraphType();
        }
        count++;
        poly->tpage = 6;
        poly->clut = getClut(palette_origin[-1] + x, palette_origin[0] + y);
    } while (count < 32);
}

/* Initialize the 8 semi-transparent POLY_GT4s of g_gfx_poly_gt4_array_8
 * with zeroed vertices, a one-texel texture cell, texture page 6 and the CLUT
 * at the bonus palette rectangle's origin offset by (x, y). */
static void gfx_init_poly_gt4_array_8(s32 x, s32 y) {
    s32 count = 0;
    s32 one = 1;
    s16* palette_origin;

    /* The y-centered pointer preserves the retail base address; RECT fields change it. */
    palette_origin = &g_conditions_banner_palette_rect.y;
    do {
        POLY_GT4* poly = &g_gfx_poly_gt4_array_8[count];
        SetPolyGT4(poly);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        setUV4(poly, 0, 0, one, 0, 0, one, one, one);
        setXY4(poly, 0, 0, 0, 0, 0, 0, 0, 0);
        /* The graph type is read, and read again when it is not 1, but never used. */
        if (GetGraphType() != one) {
            GetGraphType();
        }
        count++;
        poly->tpage = 6;
        poly->clut = getClut(palette_origin[-1] + x, palette_origin[0] + y);
    } while (count < 8);
}

/* 0x801c8f80-0x801c9fa4, 4132 bytes.
 *
 * General-case display-condition banner: builds one POLY_GT4 per quad into
 * buffer 0, mirrors it into buffer 1, then plays a 20-frame reveal (the lower
 * three quads from frame 0, the upper three from frame 9), an optional
 * `hold_frames` hold and a 10-frame hide. Circle aborts reveal and hold.
 *
 * Load-bearing spellings: the tables are function-lifetime locals and the
 * range row is `&range[condition]` with `condition = 0` (a spilled
 * constant-equivalent pseudo), which reproduces the target's constant-address
 * count read and its `addiu sN,t0,0` range rematerialization per phase;
 * `tpage` is a local copy so the page load keeps its +2 offset; `x + 0x100 +
 * width` / `y + (height + 0x78) - offset` / `y + 0x78 + offset` fix operand
 * order; declaring `frame` before `quad_count` fixes their stack slots.
 *
 * `buffer` is read before it is first written (the target's s7 is live out of
 * the prologue), so it is left uninitialized on purpose.
 */

#define QUAD(index) (quads + parameters[0])[index]

#define VERTEX_COLORS(index) (&colors[parameters[0] + index])

/* Full, unanimated quad: the reveal offset is not yet applied. */
#define STORE_QUAD_FULL(poly, index)                                                                                   \
    poly->u0 = QUAD(index).source.x;                                                                                   \
    poly->v0 = QUAD(index).source.y - 0x80;                                                                            \
    poly->u1 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v1 = QUAD(index).source.y - 0x80;                                                                            \
    poly->u2 = QUAD(index).source.x;                                                                                   \
    poly->v2 = QUAD(index).source.y - 0x80 + QUAD(index).source.h;                                                     \
    poly->u3 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v3 = QUAD(index).source.y - 0x80 + QUAD(index).source.h;                                                     \
    poly->x0 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y0 = QUAD(index).offset.y + 0x78;                                                                            \
    poly->x1 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y1 = QUAD(index).offset.y + 0x78;                                                                            \
    poly->x2 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y2 = QUAD(index).offset.y + 0x78 + QUAD(index).source.h;                                                     \
    poly->x3 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y3 = QUAD(index).offset.y + 0x78 + QUAD(index).source.h

/* Reveal: keep the bottom edge fixed while raising the top edge. */
#define STORE_QUAD_REVEAL(poly, index, reveal_pixels)                                                                  \
    poly->u0 = QUAD(index).source.x;                                                                                   \
    poly->v0 = (QUAD(index).source.y + (QUAD(index).source.h - 0x80)) - reveal_pixels;                                 \
    poly->u1 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v1 = (QUAD(index).source.y + (QUAD(index).source.h - 0x80)) - reveal_pixels;                                 \
    poly->u2 = QUAD(index).source.x;                                                                                   \
    poly->v2 = QUAD(index).source.y + (QUAD(index).source.h - 0x80);                                                   \
    poly->u3 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v3 = QUAD(index).source.y + (QUAD(index).source.h - 0x80);                                                   \
    poly->x0 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y0 = QUAD(index).offset.y + (QUAD(index).source.h + 0x78) - reveal_pixels;                                   \
    poly->x1 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y1 = QUAD(index).offset.y + (QUAD(index).source.h + 0x78) - reveal_pixels;                                   \
    poly->x2 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y2 = QUAD(index).offset.y + 0x78 + QUAD(index).source.h;                                                     \
    poly->x3 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y3 = QUAD(index).offset.y + 0x78 + QUAD(index).source.h

/* Hide: keep the bottom edge fixed while lowering the top edge. */
#define STORE_QUAD_HIDE(poly, index, hide_pixels)                                                                      \
    poly->u0 = QUAD(index).source.x;                                                                                   \
    poly->v0 = QUAD(index).source.y - 0x80 + hide_pixels;                                                              \
    poly->u1 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v1 = QUAD(index).source.y - 0x80 + hide_pixels;                                                              \
    poly->u2 = QUAD(index).source.x;                                                                                   \
    poly->v2 = QUAD(index).source.y + ((hide_pixels) - 0x80) + (QUAD(index).source.h - hide_pixels);                   \
    poly->u3 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v3 = QUAD(index).source.y + ((hide_pixels) - 0x80) + (QUAD(index).source.h - hide_pixels);                   \
    poly->x0 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y0 = QUAD(index).offset.y + 0x78 + hide_pixels;                                                              \
    poly->x1 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y1 = QUAD(index).offset.y + 0x78 + hide_pixels;                                                              \
    poly->x2 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y2 = QUAD(index).offset.y + ((hide_pixels) + 0x78) + (QUAD(index).source.h - hide_pixels);                   \
    poly->x3 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y3 = QUAD(index).offset.y + ((hide_pixels) + 0x78) + (QUAD(index).source.h - hide_pixels)

/* Show a general condition banner (DisplayConditions 8 and up) from its BONUS.BIN image: wipe in
 * the lower three pieces and then the upper three over 20 frames, hold for hold_frames, then wipe
 * it out over 10. Circle ends it at once. */
static void conditions_show_general(s32 hold_frames) {
    sprite_sheet_frame_t* quads;
    u8* range = g_conditions_general_quad_ranges;
    conditions_colors_t* colors = g_conditions_general_vertex_colors;
    conditions_tpage_t* tpage = g_conditions_general_tpages;
    s32 condition = 0;
    s32 frame;
    s32 quad_count;
    s32 index;
    s32 buffer;
    s32 reveal_offset_bottom;
    s32 reveal_offset_top;
    s32 hide_offset;
    u32* input;

    conditions_load_banner_image(thread_get_current_parameter_1() - EVENT_DISPLAY_CONDITION_GENERAL_FIRST);
    {
        u8* parameters = &range[condition];

        quad_count = parameters[1] - parameters[0];
    }
    gfx_init_poly_gt4_array_32(0, 1);
    quads = g_conditions_general_quads;

    for (index = 0; index < quad_count; index++) {
        POLY_GT4* poly = &g_gfx_poly_gt4_buffers[0][index];
        u8* parameters = &range[condition];
        s32 page;

        STORE_QUAD_FULL(poly, index);
        /* Keep only the semi-transparency bits (0x60) on texture page 6; GPU types 1
         * and 2 take those bits two places higher. */
        if (GetGraphType() == 1 || GetGraphType() == 2)
            page = (tpage[index].page & 0x60) * 4;
        else
            page = tpage[index].page & 0x60;
        poly->tpage = page | 6;
    }

    /* Seed buffer 1 with all 40 quads of buffer 0. */
    {
        POLY_GT4* destination = &g_gfx_poly_gt4_buffers[1][0];
        POLY_GT4* source = destination - 40;

        mem_copy_bytes_nonempty(destination, source, 0x820);
    }

    reveal_offset_top = 0;
    reveal_offset_bottom = 0;
    frame = 0;
    for (;;) {
        u8* parameters = &range[condition];

        thread_yield();
        input = script_get_controller_input_pointer(0);
        g_menu_input_controller = input;
        /* Circle skips the remaining phases and ends the thread. */
        if (*input & PSX_PAD_CIRCLE) {
            thread_yield();
            thread_exit_current();
            return;
        }

        /* `buffer` is never initialized; only its parity matters, so either buffer may come first. */
        buffer = (buffer + 1) & 1;
        /* The upper three pieces start their wipe nine frames after the lower three. */
        if (frame >= 9) {
            for (index = 5; index >= 3; index--) {
                POLY_GT4* poly = &g_gfx_poly_gt4_buffers[buffer][index];

                /* The clamp lowers the shared offset itself, so a shorter piece also caps the
                 * pieces drawn after it. */
                if (QUAD(index).source.h < reveal_offset_top)
                    reveal_offset_top = QUAD(index).source.h;
                STORE_QUAD_REVEAL(poly, index, reveal_offset_top);
                gfx_scale_poly_gt4_colors(VERTEX_COLORS(index), 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        for (index = 2; index >= 0; index--) {
            POLY_GT4* poly = &g_gfx_poly_gt4_buffers[buffer][index];

            if (QUAD(index).source.h < reveal_offset_bottom)
                reveal_offset_bottom = QUAD(index).source.h;
            STORE_QUAD_REVEAL(poly, index, reveal_offset_bottom);
            gfx_scale_poly_gt4_colors(VERTEX_COLORS(index), 0x80, poly);
            gfx_draw_or_append_gpu_primitive(poly);
        }

        reveal_offset_bottom += 8;
        /* The one-trip loop emits nothing; its loop note weights these two
         * references of reveal_offset_top one level deeper, which keeps the
         * top offset ahead of the bottom one in global allocation (s3/s6)
         * while both are initialized top first like the target. */
        if (frame >= 9)
            do {
                reveal_offset_top += 8;
            } while (0);
        frame++;
        if (frame >= 0x14)
            break;
    }

    frame = 0;
    if (hold_frames > 0) {
        do {
            thread_yield();
            input = script_get_controller_input_pointer(0);
            g_menu_input_controller = input;
            if (*input & PSX_PAD_CIRCLE) {
                thread_yield();
                thread_exit_current();
                return;
            }
            buffer = (buffer + 1) & 1;
            for (index = quad_count - 1; index >= 0; index--) {
                POLY_GT4* poly = &g_gfx_poly_gt4_buffers[buffer][index];
                u8* parameters = &range[condition];
                gfx_scale_poly_gt4_colors(VERTEX_COLORS(index), 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
            frame++;
        } while (frame < hold_frames);
    }

    hide_offset = 0;
    frame = 0;
    do {
        u8* parameters = &range[condition];

        thread_yield();
        buffer = (buffer + 1) & 1;
        for (index = quad_count - 1; index >= 0; index--) {
            POLY_GT4* poly = &g_gfx_poly_gt4_buffers[buffer][index];

            if (QUAD(index).source.h < hide_offset)
                hide_offset = QUAD(index).source.h;
            STORE_QUAD_HIDE(poly, index, hide_offset);
            gfx_scale_poly_gt4_colors(VERTEX_COLORS(index), 0x80, poly);
            gfx_draw_or_append_gpu_primitive(poly);
        }
        hide_offset += 8;
        frame++;
    } while (frame < 0xa);
    thread_yield();
    thread_exit_current();
}

/* Empty and unreferenced. */
void conditions_noop_1(void) {
}

/* The target re-reads the first-quad index for every field. */
#undef QUAD
#undef VERTEX_COLORS
#define QUAD (quads + parameters[0])[quad_index]

#define VERTEX_COLORS (&color_table[parameters[0] + quad_index])

/* Fade a condition's sprites in over the two primitive buffers, hold them until
 * thread parameter 3 becomes 2, then fade them out.
 *
 * `table` and `color_table` are function-lifetime copies of the table
 * addresses; the target rematerializes them in each loop instead of folding
 * them into the loads. The buffer copy and the small-array address use
 * g_gfx_poly_gt4_buffers, the second symbol at the address of
 * g_gfx_poly_gt4_array_32: sharing that base lets the hoisted
 * small-array address derive from the copy source, which keeps it live in s0. */
void conditions_special_thread(void) {
    s32 parameter_index;
    s32 use_small_array;
    s32 quad_count;
    s32 buffer;
    s32 scale;
    s32 quad_index;
    sprite_sheet_frame_t* quads;
    u8* table = g_conditions_special_quad_ranges;
    conditions_colors_t* color_table = g_conditions_special_vertex_colors;

    parameter_index = thread_get_current_parameter_1();
    use_small_array = thread_get_current_parameter_2();
    {
        u8* parameters = &g_conditions_special_quad_ranges[parameter_index];

        quads = g_conditions_special_quads;
        quad_count = parameters[1] - parameters[0];
    }
    if (use_small_array == 0) {
        gfx_init_poly_gt4_array_32(0x20, 0);
    } else {
        gfx_init_poly_gt4_array_8(0x20, 0);
    }

    for (quad_index = 0; quad_index < quad_count; quad_index++) {
        u8* parameters = &table[parameter_index];
        POLY_GT4* poly = &(use_small_array == 0 ? g_gfx_poly_gt4_array_32 : g_gfx_poly_gt4_array_8)[quad_index];

        setUV4(poly, QUAD.source.x, QUAD.source.y, QUAD.source.x + QUAD.source.w, QUAD.source.y, QUAD.source.x,
            QUAD.source.y + QUAD.source.h, QUAD.source.x + QUAD.source.w, QUAD.source.y + QUAD.source.h);
        setXY4(poly, QUAD.offset.x + 0x100, QUAD.offset.y + 0x78, QUAD.offset.x + 0x100 + QUAD.source.w,
            QUAD.offset.y + 0x78, QUAD.offset.x + 0x100, QUAD.offset.y + 0x78 + QUAD.source.h,
            QUAD.offset.x + 0x100 + QUAD.source.w, QUAD.offset.y + 0x78 + QUAD.source.h);
    }
    mem_copy_bytes_nonempty(g_gfx_poly_gt4_buffers[1], g_gfx_poly_gt4_buffers, 40 * sizeof(POLY_GT4));

    buffer = 0;
    scale = 0x30;
    /* Parameter 3 selects the next phase at any frame: 2 jumps from the
       fade-in past the hold loop to the fade-out, 1 ends the thread. */
    while (scale < 0x80) {
        if (thread_get_current_parameter_3() == 2) {
            goto fade_out;
        }
        if (thread_get_current_parameter_3() == 1) {
            thread_yield();
            thread_exit_current();
            return;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[parameter_index];
            const conditions_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
        scale += 8;
    }
    for (;;) {
        if (thread_get_current_parameter_3() == 2) {
            break;
        }
        if (thread_get_current_parameter_3() == 1) {
            thread_yield();
            thread_exit_current();
            return;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[parameter_index];
            const conditions_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
    }
fade_out:
    scale = 0x80;
    do {
        if (thread_get_current_parameter_3() == 1) {
            break;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[parameter_index];
            const conditions_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
        scale -= 0xc;
    } while (scale >= 0x30);
    thread_yield();
    thread_exit_current();
}

/* Call thread_exit_current. */
void thread_exit_current_wrapper(void) {
    thread_exit_current();
}

/* Animate a BONUS.BIN banner image as a burst of tiles.
 *
 * Profile gcc-2.6.3_O2_aspsx-2.34_divcheck: the per-tile delay is an unsigned
 * division whose divisor carries ASPSX's zero check (`bnez`/`break 7`).
 *
 * The bonus image is cut into a 12x24 grid of flat quads. Each tile starts
 * after a delay derived from its distance (script mode 0: from the center,
 * 1: from the origin, 2: horizontal only, other: rotated x), then two
 * rotations of its radius place the quad corners while angle and radius grow
 * (phase 0) or shrink (phase 1). Between the phases a full-screen flash quad is
 * drawn until the controlling task switches to 0x36.
 *
 * Tile coordinates are written as expressions of the loop counters rather
 * than running variables: loop.c then derives the same induction values the
 * target keeps, and the extra insns keep it from hoisting the row offset and
 * the RotMatrix/vector addresses out of the column loop.
 */
void conditions_animate_banner_burst(void) {
    s32 phase;
    s32 max_delay;
    u32 divisor;
    s32 angle_step;
    s32 radius_step;
    s32 done;
    s32 mode;
    s32 frame;
    s32 any;
    s32 row;
    s32 col;
    s32 dx;
    s32 dy;
    s32 dist;
    s32 ox;
    s32 oy;
    u8* params;
    conditions_burst_tile_t* tile;
    POLY_F4* poly;

    g_after_battle_thread_suspended_id = g_thread_current_id;
    g_text_section_pointers[TEXT_SECTION_UNIT_MESSAGE] = g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES_COPY];
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON) {
        conditions_load_banner_image(0);
    } else {
        conditions_load_banner_image(1);
    }

    /* Named operand loads fold the three native cursor advances. */
    params = thread_get_current_parameter_2();
    /* Script parameter bytes: 1 mode, 2 delay divisor, 3-4 angle step (little-endian), 5 radius step. */
    params++;
    mode = *params;
    params++;
    divisor = *params;
    params++;
    max_delay = 0;
    radius_step = params[2];
    angle_step = (params[1] << 8) + params[0];

    /* Lay out the tiles; odd columns sit half a row higher. */
    for (row = 0; row < 12; row++) {
        for (col = 0; col < 24; col++) {
            tile = &g_conditions_burst_tiles[row][col];
            tile->x = col * 12 - 12;
            tile->y = row * 24 - 12;
            if (col & 1) {
                tile->y = row * 24 - 24;
            }
            if (mode == CONDITIONS_BURST_FROM_CENTER) {
                dx = tile->x - 0x80;
                dy = tile->y - 0x80;
                dist = SquareRoot0(dx * dx + dy * dy);
            } else if (mode == CONDITIONS_BURST_FROM_ORIGIN) {
                dx = tile->x;
                dy = tile->y;
                dist = SquareRoot0(dx * dx + dy * dy);
            } else if (mode == CONDITIONS_BURST_HORIZONTAL) {
                dx = tile->x;
                dist = SquareRoot0(dx * dx);
            } else {
                dx = tile->x - 0x80;
                dy = tile->y - 0x80;
                g_conditions_burst_rotation.vx = 0;
                g_conditions_burst_rotation.vy = 0;
                g_conditions_burst_rotation.vz = 0x200;
                RotMatrix(&g_conditions_burst_rotation, &g_conditions_burst_matrix);
                g_conditions_burst_offset.vx = dx * ONE;
                g_conditions_burst_offset.vy = dy * ONE;
                g_conditions_burst_offset.vz = 0;
                ApplyMatrixLV(
                    &g_conditions_burst_matrix, &g_conditions_burst_offset, &g_conditions_burst_rotated_offset);
                dist = abs(g_conditions_burst_rotated_offset.vx / ONE);
            }
            /* The delay grows with the square of the distance. */
            dist = (dist * dist) / (divisor << 8);
            tile->delay = dist;
            tile->angle = 0;
            tile->radius = 0;
            tile->initial_delay = dist;
            if (max_delay < tile->delay) {
                max_delay = tile->delay;
            }
            SetPolyF4(&tile->poly[0]);
            SetSemiTrans(&tile->poly[0], 1);
            mem_copy_bytes_nonempty(&tile->poly[1], &tile->poly[0], sizeof(POLY_F4));
        }
    }

    SetDrawMode(&g_conditions_burst_draw_modes[0], 0, 0, GetTPage(0, 0, 0x180, 0), &g_menu_draw_area_template);
    SetDrawMode(&g_conditions_burst_draw_modes[1], 0, 0, GetTPage(0, 0, 0x180, 0), &g_menu_draw_area_template);
    frame = 0;
    for (phase = 0; phase < 2; phase++) {
        done = 0;
        for (;;) {
            any = 0;
            thread_yield();
            for (row = 0; row < 12; row++) {
                for (col = 0; col < 24; col++) {
                    tile = &g_conditions_burst_tiles[row][col];
                    poly = &tile->poly[frame & 1];
                    /* A started tile is a square turned by its angle: corners on its radius at
                     * angle + 45 and + 135 degrees (0x200, 0x600) and their mirror images. */
                    if (tile->delay == 0) {
                        g_conditions_burst_rotation.vx = 0;
                        g_conditions_burst_rotation.vy = 0;
                        g_conditions_burst_rotation.vz = tile->angle + 0x200;
                        RotMatrix(&g_conditions_burst_rotation, &g_conditions_burst_matrix);
                        g_conditions_burst_offset.vx = tile->radius * ONE;
                        g_conditions_burst_offset.vy = 0;
                        g_conditions_burst_offset.vz = 0;
                        ApplyMatrixLV(
                            &g_conditions_burst_matrix, &g_conditions_burst_offset, &g_conditions_burst_rotated_offset);
                        ox = g_conditions_burst_rotated_offset.vx / 0x4000;
                        oy = g_conditions_burst_rotated_offset.vy / 0x4000;
                        poly->x0 = ox + tile->x + 0x80;
                        poly->y0 = oy + tile->y;
                        poly->x3 = tile->x - ox + 0x80;
                        poly->y3 = tile->y - oy;
                        g_conditions_burst_rotation.vx = 0;
                        g_conditions_burst_rotation.vy = 0;
                        g_conditions_burst_rotation.vz = tile->angle + 0x600;
                        RotMatrix(&g_conditions_burst_rotation, &g_conditions_burst_matrix);
                        g_conditions_burst_offset.vx = tile->radius * ONE;
                        g_conditions_burst_offset.vy = 0;
                        g_conditions_burst_offset.vz = 0;
                        ApplyMatrixLV(
                            &g_conditions_burst_matrix, &g_conditions_burst_offset, &g_conditions_burst_rotated_offset);
                        ox = g_conditions_burst_rotated_offset.vx / 0x4000;
                        oy = g_conditions_burst_rotated_offset.vy / 0x4000;
                        poly->x1 = ox + tile->x + 0x80;
                        poly->y1 = oy + tile->y;
                        poly->x2 = tile->x - ox + 0x80;
                        poly->y2 = tile->y - oy;
                        if (phase == 0) {
                            tile->angle += angle_step;
                            tile->radius += radius_step;
                            if (tile->angle < 0x201 || tile->radius < 0x31) {
                                any = 1;
                            }
                            if (tile->angle > 0x200) {
                                tile->angle = 0x200;
                            }
                            if (tile->radius > 0x30) {
                                tile->radius = 0x30;
                            }
                        } else {
                            tile->angle -= angle_step;
                            tile->radius -= radius_step;
                            if (tile->angle >= 0 || tile->radius >= 0) {
                                any = 1;
                            }
                            if (tile->angle < 0) {
                                tile->angle = 0;
                            }
                            if (tile->radius < 0) {
                                tile->radius = 0;
                            }
                        }
                        poly->r0 = g_after_battle_gfx_fade_rgb[0];
                        poly->g0 = g_after_battle_gfx_fade_rgb[1];
                        poly->b0 = g_after_battle_gfx_fade_rgb[2];
                        gfx_append_gpu_primitive_to_secondary_otag(poly);
                    } else {
                        poly->r0 = g_after_battle_gfx_fade_rgb[0];
                        poly->g0 = g_after_battle_gfx_fade_rgb[1];
                        poly->b0 = g_after_battle_gfx_fade_rgb[2];
                        /* Waiting tiles stay hidden in phase 0 and keep their full shape in phase 1. */
                        if (phase == 1) {
                            gfx_append_gpu_primitive_to_secondary_otag(poly);
                        }
                        any = 1;
                        tile->delay--;
                    }
                }
            }

            gfx_append_gpu_primitive_to_secondary_otag(&g_conditions_burst_draw_modes[frame & 1]);
            /* One more frame is drawn after every tile has finished. */
            if (done == 1) {
                break;
            }
            frame++;
            if (any == 0) {
                done = 1;
            }
        }

        if (phase == 1) {
            break;
        }

        SetPolyF4(&g_conditions_burst_flash_polys[0]);
        setXY4(&g_conditions_burst_flash_polys[0], 0x80, 0, 0x180, 0, 0x80, 0x100, 0x180, 0x100);
        g_conditions_burst_flash_polys[0].r0 = 0x30;
        g_conditions_burst_flash_polys[0].g0 = 0x28;
        g_conditions_burst_flash_polys[0].b0 = 0x10;
        SetSemiTrans(&g_conditions_burst_flash_polys[0], 1);
        mem_copy_bytes_nonempty(
            &g_conditions_burst_flash_polys[1], &g_conditions_burst_flash_polys[0], sizeof(POLY_F4));
        frame++;
        /* The controlling thread ends the hold by switching this task to THREAD_TASK_DARK_SCREEN. */
        thread_set_current_task_id(THREAD_TASK_DARK_SCREEN_HOLD);
        for (;;) {
            thread_yield();
            g_conditions_burst_flash_polys[frame & 1].r0 = g_after_battle_gfx_fade_rgb[0];
            g_conditions_burst_flash_polys[frame & 1].g0 = g_after_battle_gfx_fade_rgb[1];
            g_conditions_burst_flash_polys[frame & 1].b0 = g_after_battle_gfx_fade_rgb[2];
            gfx_append_gpu_primitive_to_secondary_otag(&g_conditions_burst_flash_polys[frame & 1]);
            gfx_append_gpu_primitive_to_secondary_otag(&g_conditions_burst_draw_modes[frame & 1]);
            if (g_threads[g_thread_current_id].task_id == THREAD_TASK_DARK_SCREEN) {
                break;
            }
            frame++;
        }

        frame++;
        /* Restart the delays so phase 1 collapses the tiles in the same order. */
        for (row = 0; row < 12; row++) {
            for (col = 0; col < 24; col++) {
                tile = &g_conditions_burst_tiles[row][col];
                tile->delay = tile->initial_delay;
            }
        }
    }

    thread_wait_frames(1);
    thread_exit_current();
}

/*
 * Thread body of the REQUIRE overlay: runs one display condition.
 *
 * Parameter 1 is the condition id and parameter 2 its argument. The general
 * conditions draw their banner inline (the argument is the hold time). The
 * others each run their routine in the first idle thread slot and wait for
 * it: the special-case banner (held for the argument's frames or until
 * skipped), "ready", the battle congratulations, the gil reward, the war
 * trophies, the permanent Brave/Faith changes and the low-Brave/high-Faith
 * desertions. The post-battle unit changes run inline.
 */
void conditions_dispatch(void) {
    s32 condition;
    s32 parameter;
    s32 thread;

    thread_set_current_task_id(THREAD_TASK_CONDITION_DISPATCH);
    condition = thread_get_current_parameter_1();
    parameter = thread_get_current_parameter_2();
    if (condition >= EVENT_DISPLAY_CONDITION_GENERAL_FIRST)
        conditions_show_general(parameter);
    thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);

    if (condition == EVENT_DISPLAY_CONDITION_SPECIAL_CASES) {
        thread_start(thread, conditions_special_thread);
        thread_set_parameters(thread, 0, 0, 0);
        input_wait_frames_or_skip(parameter);
        thread_set_parameters(thread, 0, 0, 2);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_SHOW_READY_BUGGED) {
        thread_start(thread, conditions_show_ready_bugged);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_BATTLE_CONGRATULATIONS) {
        thread_start(thread, victory_congratulations_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_GIL_REWARD) {
        thread_start(thread, war_trophy_run_gil_ticker);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_WAR_TROPHIES) {
        thread_start(thread, war_trophy_list_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_APPLY_PERMANENT_BRAVE_FAITH_CHANGES) {
        thread_start(thread, party_brave_faith_report_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_REMOVE_LOW_BRAVE_HIGH_FAITH_UNITS) {
        thread_start(thread, party_departures_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_APPLY_POST_BATTLE_UNIT_CHANGES) {
        party_apply_post_battle_changes();
    }
    thread_exit_current();
}

/* Thread body that runs the HELPMENU overlay from the REQUIRE screen: waits
 * until 0x10000 bytes can be reserved at the high overlay address, loads the
 * overlay and runs help_run_after_battle in the first idle slot,
 * with the panel's two animation flags set while it runs. */
static void help_open_after_battle(void) {
    s32 thread_id;

    thread_set_current_task_id(THREAD_TASK_HELP_MENU);
    g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
    g_unit_panel_panel_status_animation[0] = 1;
    g_unit_panel_panel_status_animation[1] = 1;
    while (heap_reserve_at(g_heap_high_overlay_load_address, 0x10000) != g_heap_high_overlay_load_address) {
        thread_yield();
    }
    overlay_request_companion(COMPANION_OVERLAY_HELPMENU);
    thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_id, help_run_after_battle);
    thread_wait_until_inactive(thread_id);
    g_unit_panel_panel_status_animation[0] = 0;
    g_unit_panel_panel_status_animation[1] = 0;
    thread_exit_current();
}

/* Rebuild the roster name text section from the battle units: one 0xFE
 * terminated name per battle unit slot, with a terminator appended to a name
 * that fills all 16 bytes. */
static void text_build_battle_unit_names(void) {
    u8* output = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    s32 unit_id;

    for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
        unit_stats_t* unit = unit_get_stats(unit_id);
        s32 length = 0;

        do {
            u8 value = unit->name[length];
            *output = value;
            if ((value & 0xfe) != 0xfe) {
                output++;
                if (length == UNIT_NAME_SIZE - 1) {
                    *output++ = 0xfe;
                }
            } else {
                output++;
                break;
            }
            length++;
        } while (length < UNIT_NAME_SIZE);
    }
}
