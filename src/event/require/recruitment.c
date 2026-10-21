/* Show recruited survivors in the party panels, offer room by dismissing
 * a roster member, and return equipment to inventory or sell its overflow. */

#include "fft/ability.h"
#include "fft/after_battle.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/message.h"
#include "fft/party.h"
#include "fft/portrait.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Scalar x aliases of the three panels' origins; they preserve the retail
 * transition-store scheduling. */
extern s32 g_unit_panel_status_display_origin_x;
extern s32 g_unit_panel_character_status_origin_x;
extern s32 g_unit_panel_numbers_origin_x;
extern u8 g_party_removable_flags[];
extern RECT g_party_portrait_move_rect;
extern s16 g_party_portrait_destination;
extern RECT g_party_palette_move_rect;

/* x and y of the palette MoveImage destination RECT. Read as two scalar symbols: a RECT view lets GCC keep the
 * struct address live in a saved register and changes the allocation. */
extern s16 g_party_palette_destination;
extern s16 g_party_palette_destination_y;
extern s16 g_party_removal_result;

/* Entries in the post-battle recruitment and dismissal menu table. */
typedef enum recruitment_entry {
    AFTER_BATTLE_MENU_JOIN_PROMPT = 0,
    AFTER_BATTLE_MENU_JOIN_CHOICES = 1,
    AFTER_BATTLE_MENU_DISMISS_CONFIRMATION = 2,
    AFTER_BATTLE_MENU_STATUS_SCREEN = 3,
    AFTER_BATTLE_MENU_ROSTER_FULL_PROMPT = 4,
    AFTER_BATTLE_MENU_ROSTER_FULL_CHOICES = 5,
    AFTER_BATTLE_MENU_DISMISS_UNIT_PROMPT = 6,
    AFTER_BATTLE_MENU_EQUIPMENT_SOLD_MESSAGE = 7,
    AFTER_BATTLE_MENU_MONSTER_JOIN_CHOICES = 8,
    AFTER_BATTLE_MENU_ENTRY_COUNT = 9
} recruitment_entry_e;

/* Post-battle recruitment prompts, dismissal menus and equipment-overflow messages. */
extern menu_entry_t g_after_battle_menu_entries[AFTER_BATTLE_MENU_ENTRY_COUNT];

/* The join prompt's unused message result. */
static s16 g_after_battle_message_menu_result;

/* Member access reschedules a summary-field store in party_apply_post_battle_changes. */
extern s32 g_unit_panel_character_status_redraw_request;
static thread_t g_after_battle_saved_thread = { 0 };
/* REQUIRE twin of unit_panel_load_party_unit: rebuild party slot `party_index` into a
 * scratch unit and fill the selected-unit status and editor display from it, at full
 * HP, MP and CT. */
void unit_panel_load_party_unit(s32 unused, s32 party_index) {
    unit_stats_t* unit;
    menu_status_panel_slot_state_t* editor;
    u16* abilities;
    const item_data_t* item;
    s32 i;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    u16 maximum_hp;
    u16 maximum_mp;
    unit_panel_summary_t* gauges;

    unit_party_slot_init(&g_unit_panel_editor_party_unit_stats, party_index, 0);
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
        abilities = ability_skillset_list_build(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        i = 0;
        do {
            editor->ability_ids[i] = *abilities++;
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
        item = item_data_get(item_id);
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
static s32 equipment_return(s32 party_index) {
    party_unit_t* party = party_unit_get(party_index);
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
s32 party_sell_equipment_over_limit(s32 battle_index) {
    unit_stats_t* unit;
    s32 sold_equipment = 0;
    s32 equipment_slot;

    unit = unit_stats_get(battle_index);
    equipment_slot = 0;
    do {
        u8 item_id = unit->equipment[equipment_slot];
        s32 sale_value;

        if (item_id != ITEM_ID_NOTHING && item_id != ITEM_ID_NONE && item_total_quantity_get(item_id, 1) >= 100) {
            unit->equipment[equipment_slot] = ITEM_ID_NOTHING;
            sale_value = item_data_get(item_id)->price;
            sale_value /= 4;
            script_variable_set(
                EVENT_SCRIPT_VAR_WAR_FUNDS, script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS) + sale_value);
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
s32 party_store_or_sell_equipment(s32 battle_index) {
    unit_stats_t* unit;
    s32 sold_equipment = 0;
    s32 equipment_slot;

    unit = unit_stats_get(battle_index);
    equipment_slot = 0;
    do {
        u8 item_id = unit->equipment[equipment_slot];
        s32 sale_value;

        if ((item_id != ITEM_ID_NOTHING) && (item_id != ITEM_ID_NONE)) {
            if (item_total_quantity_get(item_id, 1) >= 100) {
                unit->equipment[equipment_slot] = ITEM_ID_NOTHING;
                sale_value = item_data_get(item_id)->price;
                sale_value /= 4;
                script_variable_set(
                    EVENT_SCRIPT_VAR_WAR_FUNDS, script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS) + sale_value);
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
 * party_free_slot_find clears the byte it is handed for a palette. Here
 * that is the palette of the first roster record with a Ramza identity (the
 * last record when there is none).
 */
static s32 join_status_get(s32 battle_index) {
    party_unit_t* party;
    unit_stats_t* unit = unit_stats_get(battle_index);
    s32 index;

    if (unit->unit_flags & UNIT_FLAG_SAVE_FORMATION) {
        return 2;
    }

    for (index = 0; index < PARTY_ROSTER_SLOT_COUNT; index++) {
        party = party_unit_get(index);
        if ((party->party_id != PARTY_ID_NONE) && (party->sprite_set != CHARACTER_IDENTITY_ENTD_NONE)
            && (party->sprite_set < CHARACTER_IDENTITY_RAMZA_END)) {
            break;
        }
    }

    return party_free_slot_find(0, &party->palette) != -1;
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
   after party_unit_remove.
   Matching notes: should_restore also serves as the loop's constant 1 (the
   target keeps 1 in that variable's register and materializes it before the
   pointer copies); the portrait pointers are assigned inside the loop so
   loop.c hoists them after the thread constants; the R1 search loop holds
   its whole switch-unit block (a loop that large keeps the 0x14 bound
   inside it), while the L1 search stays a do-while (with both blocks inside
   their loops should_restore outranks selected_battle_unit for s4); the
   formation search is a goto loop so the 3 bound stays in place. */
static s32 dismissal_selection_run(void) {
    s32 party_index;
    u8* text;
    s32 selected_party_index;
    s32 result;
    s32 should_restore;
    s32 message_id;
    s32 message_group_index;
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
            party_unit = party_unit_get(party_index);
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
    g_unit_panel_status_display_thread_params.redraw_request = 1;
    g_unit_panel_numbers_thread_params.redraw_request = 1;
    g_unit_panel_character_status_redraw_request = 1;
    thread_yield();

    thread_start(AFTER_BATTLE_THREAD_SLOT_TRANSITION, unit_panel_transition_thread);
    thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_TRANSITION, 0, 0, 0);
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
        thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MESSAGE,
            (s32)&g_after_battle_menu_entries[AFTER_BATTLE_MENU_ROSTER_FULL_PROMPT], 0, 0);
        portrait_rect = &g_party_portrait_move_rect;
        portrait_destination = &g_party_portrait_destination;
        palette_rect = &g_party_palette_move_rect;
        thread_inactive_wait(AFTER_BATTLE_THREAD_SLOT_MESSAGE);
        thread_inactive_wait(6);
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
        thread_four_parameters_set(
            AFTER_BATTLE_THREAD_SLOT_MESSAGE, 0x19, message_id, selected_battle_unit->unit_id, should_restore);
        g_threads[AFTER_BATTLE_THREAD_SLOT_MESSAGE].task_words[DIALOG_THREAD_WORD_Y_OFFSET] = 0x18;
        thread_inactive_wait(AFTER_BATTLE_THREAD_SLOT_MESSAGE);
        continue;

    selection_menu:
        thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_icon_linked_entry_thread);
        thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MENU,
            (s32)&g_after_battle_menu_entries[AFTER_BATTLE_MENU_DISMISS_UNIT_PROMPT], 0, 0);
        thread_inactive_wait(AFTER_BATTLE_THREAD_SLOT_MENU);
        unit_panel_set_transition(0);
        for (;;) {
            s32 helpmenu_thread;

            thread_yield();
            g_menu_input_controller = script_get_controller_input_pointer(0);
            if (*g_menu_input_controller & PSX_PAD_SELECT) {
                helpmenu_thread = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
                thread_start(helpmenu_thread, help_open_after_battle);
                thread_inactive_wait(helpmenu_thread);
                continue;
            }
            /* R1 and L1 step to the next or previous removable unit. The status redraw
             * request doubles as the portrait slide direction: 1 (should_restore) for R1, 2 for L1. */
            if (*g_menu_input_controller & PSX_PAD_R1) {
                g_unit_panel_status_animation[1] = 7;
                for (;;) {
                    selected_party_index++;
                    if (selected_party_index == PARTY_ROSTER_SLOT_COUNT)
                        selected_party_index = 0;
                    if (g_party_removable_flags[selected_party_index] != 0) {
                        g_sound_pending_sfx_id = SOUND_SFX_PAGE_SWITCH;
                        g_unit_panel_status_display_thread_params.redraw_request = should_restore;
                        g_unit_panel_numbers_thread_params.redraw_request = should_restore;
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
                g_unit_panel_status_animation[0] = 7;
                do {
                    selected_party_index--;
                    if (selected_party_index == -1)
                        selected_party_index = PARTY_ROSTER_SLOT_COUNT - 1;
                } while (g_party_removable_flags[selected_party_index] == 0);
                g_sound_pending_sfx_id = SOUND_SFX_PAGE_SWITCH;
                g_unit_panel_status_display_thread_params.redraw_request = 2;
                g_unit_panel_numbers_thread_params.redraw_request = should_restore;
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
                battle_unit = unit_stats_get(party_index);
                selected_battle_unit = unit_entd_find_stats(battle_unit->unit_id, &message_id);
                if (message_id >= 0 && party_index == message_id && unit_battle_runtime_id_get(party_index) != -1)
                    break;
            }
        }

        party_unit = party_unit_get(selected_party_index);
        /* Generic units get one of eight random messages for their kind; others look up
         * their message group, with Ramza's messages apart from the special characters'. */
        message_group_index = 0;
        if (party_unit->sprite_set & CHARACTER_IDENTITY_HIGH_BIT_MASK) {
            message_id = ((party_unit->sprite_set - CHARACTER_IDENTITY_SELECTOR_FIRST) * 8)
                + (script_variable_get(EVENT_SCRIPT_VAR_RANDOM_VALUE) & 7) + 0x1800;
        } else {
            s32 identity_column;
            u8(*identity_group)[PARTY_MESSAGE_IDENTITIES_PER_GROUP];
            u32 identity;

            identity = party_unit->sprite_set;
            identity_group = g_party_message_identity_groups;
        next_identity_group:
            for (identity_column = 0; identity_column < PARTY_MESSAGE_IDENTITIES_PER_GROUP; identity_column++) {
                if (identity == (*identity_group)[identity_column])
                    break;
            }
            if (identity_column == PARTY_MESSAGE_IDENTITIES_PER_GROUP) {
                message_group_index++;
                identity_group++;
                if (message_group_index < PARTY_MESSAGE_IDENTITY_GROUP_COUNT)
                    goto next_identity_group;
            }
            if (message_group_index == PARTY_MESSAGE_IDENTITY_GROUP_COUNT)
                message_group_index = 0;
            /* Reis's dragon form uses the final message group. */
            if (message_group_index == 71)
                message_group_index = 72;
            if (identity != CHARACTER_IDENTITY_ENTD_NONE && identity < CHARACTER_IDENTITY_RAMZA_END) {
                message_id = message_group_index + 0x1862;
            } else {
                message_id = message_group_index + 0x1818;
            }
        }

        unit_panel_set_transition(1);
        g_text_substitution_values[0] = selected_party_index;
        /* Ramza only gets his message and the prompt repeats: he cannot be dismissed. */
        if (party_unit->sprite_set < CHARACTER_IDENTITY_RAMZA_END)
            goto show_unit_message;
        g_after_battle_menu_selection_result = 0xff;
        thread_start(AFTER_BATTLE_THREAD_SLOT_MESSAGE, message_dialog_thread);
        thread_four_parameters_set(
            AFTER_BATTLE_THREAD_SLOT_MESSAGE, 0x19, message_id, selected_battle_unit->unit_id, should_restore);
        g_threads[AFTER_BATTLE_THREAD_SLOT_MESSAGE].task_words[DIALOG_THREAD_WORD_Y_OFFSET] = 0x18;
        thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_run_icon_selection_loop);
        thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MENU,
            (s32)&g_after_battle_menu_entries[AFTER_BATTLE_MENU_DISMISS_CONFIRMATION], 0, 0);
        menu_wait_selection_threads();
        if (g_after_battle_menu_selection_result != 0)
            continue;

        should_restore = equipment_return(selected_party_index);
        result = 0;
        party_unit_remove(selected_party_index);
        party_set_biography_state(party_unit->sprite_set, PARTY_BIO_STATE_DISMISSED);
        break;
    }

    {
        s32 name_character_index;

        /* Put the battle units' names back into the shared name text. */
        text = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
        for (party_index = 0; party_index < UNIT_SLOT_COUNT; party_index++) {
            selected_battle_unit = unit_stats_get(party_index);
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
        thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MENU,
            (s32)&g_after_battle_menu_entries[AFTER_BATTLE_MENU_EQUIPMENT_SOLD_MESSAGE], 0, 0);
        thread_inactive_wait(AFTER_BATTLE_THREAD_SLOT_MENU);
    }

    unit_panel_set_transition(0);
    g_gfx_screen_fade = 0;
    do {
        thread_yield();
        g_gfx_screen_fade += 0x10;
    } while (g_gfx_screen_fade < 0x100);
    g_gfx_screen_fade = 0xff;
    thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_TRANSITION, 0, 0, 1);
    return result;
}

/* Applies the post-battle party changes for each affected battle unit.
 *
 * Shows each unit's status panel, lets the player choose whether to recruit
 * it (removing a party member first when the roster is full), then saves it
 * to the party or updates its bio variable. Restores the saved thread block
 * and menu descriptor on exit.
 */
void party_apply_post_battle_changes(void) {
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
        thread_current_exit();
    }

    g_menu_post_battle_unit_changes_active = 1;
    thread_suspend(THREAD_SLOT_EVENT);
    mem_copy_bytes_nonempty(
        &g_after_battle_saved_thread, &g_threads[THREAD_SLOT_EVENT], sizeof(g_after_battle_saved_thread));

    text_data = g_text_data;
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS]
        = text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS];
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES]
        = text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES];
    saved_menu_descriptor = g_menu_thread_menu_data;
    menu_base = g_after_battle_menu_entries;
    g_menu_thread_menu_data = menu_base;
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES]
        = text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES]
        = text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_JOB_NAMES];

    affected_index = 0;
    text_build_battle_unit_names();
    initial_unit_index_ptr = g_party_affected_battle_unit_indices;
    initial_unit_data = &g_unit_panel_selected_unit;
    unit_stats_get(*initial_unit_index_ptr);
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
                thread_parameters_set(
                    THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_unit_panel_status_display_thread_params, 0, 0);
                thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
                thread_parameters_set(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_numbers_thread_params, 0, 0);
                thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
                thread_parameters_set(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)transition_c_ptr, 0, 0);
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

                unit_stats_get(*unit_index_ptr);
                index_value = *unit_index_ptr;
                unit_display_data->battle_id = index_value;
                unit_display_data->name_id = index_value;
                unit_panel_load_selected();
                unit_panel_copy_billboard_in(
                    &g_unit_panel_selected_unit_status, unit_display_data, &g_unit_panel_selected_unit_fields);
                index_value = *unit_index_ptr;
                g_unit_panel_status_display_thread_params.redraw_request = one;
                g_unit_panel_numbers_thread_params.redraw_request = one;
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
                            thread_id = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
                            thread_start(thread_id, help_open_after_battle);
                            thread_inactive_wait(thread_id);
                        }
                    } while (1);

                    unit_panel_set_transition(1);
                    /* Fit the join prompt window to text 0x5006 (with this unit's name) and center it at x 0x100. */
                    g_text_substitution_values[0] = *selected_unit_index_ptr;
                    text_pixels_measure(&text_width_px, &text_height_px, text_entry_find(0x5006));
                    /* The s16 sum reads the 16-bit value directly (a second copy of the
                     * loaded width) while the modulo uses the widened s32 value; the
                     * narrowing also leaves the other unreferenced frame slot. */
                    text_width = text_width_px;
                    half_menu_x = text_width % 4 + 0x18;
                    menu_x = text_width_px + half_menu_x;
                    half_menu_x = menu_x / 2;
                    /* A recovered struct pointer hoists the owner address across the call (two instructions differ).
                     * Derive each byte displacement from the named field instead. */
#define MENU_FIELD(field) (*(s16*)(menu_fields + offsetof(menu_entry_t, field) - offsetof(menu_entry_t, inner_width)))
                    MENU_FIELD(text_image.source.w) = menu_x;
                    MENU_FIELD(window.w) = menu_x;
                    MENU_FIELD(inner_width) = menu_x;
                    MENU_FIELD(window.x) = 0x100 - half_menu_x + menu_x % 2;
#undef MENU_FIELD

                    /* A guest (status 2) joins without the prompt. The store goes to
                     * g_after_battle_message_menu_result, which no code reads, so the checks below see the selection
                     * result left from before. */
                    if (join_status_get(*selected_unit_index_ptr) == 2) {
                        g_after_battle_message_menu_result = 0;
                    } else {
                        g_after_battle_menu_selection_result = 0xff;
                        thread_start(AFTER_BATTLE_THREAD_SLOT_MESSAGE, menu_icon_strip_thread);
                        thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MESSAGE,
                            (s32)(menu_fields - offsetof(menu_entry_t, inner_width)), 0, 0);
                        if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
                            thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_run_icon_selection_loop);
                            thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MENU,
                                (s32)(menu_fields + AFTER_BATTLE_MENU_MONSTER_JOIN_CHOICES * sizeof(*menu_base)
                                    - offsetof(menu_entry_t, inner_width)),
                                0, 0);
                        } else {
                            thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_run_icon_selection_loop);
                            thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MENU,
                                (s32)(menu_fields + AFTER_BATTLE_MENU_JOIN_CHOICES * sizeof(*menu_base)
                                    - offsetof(menu_entry_t, inner_width)),
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
                if (join_status_get(*selected_unit_index_ptr) != 0) {
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
                    removal_result = dismissal_selection_run();
                    /* The target loads *selected_unit_index_ptr at 0x514,
                     * above both global stores. GCC 2.6.3 emits a
                     * straight-line block in strict source order, so the
                     * load has to be written above them. */
                    removal_unit_index = *selected_unit_index_ptr;
                    g_party_removal_result = removal_result;
                    g_menu_post_battle_unit_changes_active = one;

                    unit_stats_get(removal_unit_index);
                    /* The target reads the index into v0 here and reloads it
                     * into v1 after the call; one shared local is one allocno
                     * and takes v1 at both sites. */
                    display_index = *selected_unit_index_ptr;
                    unit_display_data->battle_id = display_index;
                    unit_display_data->name_id = display_index;
                    unit_panel_load_selected();
                    index_value = *selected_unit_index_ptr;
                    g_unit_panel_status_display_thread_params.redraw_request = one;
                    g_unit_panel_numbers_thread_params.redraw_request = one;
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

            /* Equipment the inventory cannot hold is sold; explain the sale before the unit joins. */
            if (party_sell_equipment_over_limit(*selected_unit_index_ptr) != 0) {
                thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, menu_icon_linked_entry_thread);
                thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MENU,
                    (s32)&g_after_battle_menu_entries[AFTER_BATTLE_MENU_EQUIPMENT_SOLD_MESSAGE], 0, 0);
                thread_inactive_wait(AFTER_BATTLE_THREAD_SLOT_MENU);
            }

            /* Show Join Up for 60 frames or until skipped; a third parameter of 2 fades it out. */
            thread_frames_wait(20);
            thread_start(AFTER_BATTLE_THREAD_SLOT_MENU, conditions_special_thread);
            thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MENU, CONDITIONS_BANNER_JOIN_UP, 0, 0);
            input_wait_frames_or_skip(60);
            thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MENU, 0, 0, 2);
            thread_inactive_wait(AFTER_BATTLE_THREAD_SLOT_MENU);
            unit_panel_set_transition(0);

            unit = unit_stats_get(*selected_unit_index_ptr);
            unit_index_ptr++;
            party_unit_save(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
            thread_frames_wait(20);
            goto next_unit;
        not_joining:
            /* Answer 1 declines; any other answer asks again. */
            if (g_after_battle_menu_selection_result != one) {
                selected_unit_index_ptr = unit_index_ptr;
                goto retry_selection;
            }
            unit_panel_set_transition(0);
            thread_frames_wait(10);
            unit = unit_stats_get(*selected_unit_index_ptr);
            unit_index_ptr++;
            party_set_biography_state(unit->character_identity, PARTY_BIO_STATE_DISMISSED);
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
    thread_parameters_set(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
    thread_parameters_set(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
    thread_parameters_set(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
    thread_inactive_wait(THREAD_SCREEN_SLOT_UNIT_STATUS);
    thread_inactive_wait(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    thread_inactive_wait(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
    g_menu_post_battle_unit_changes_active = 0;
    mem_copy_bytes_nonempty(
        &g_threads[THREAD_SLOT_EVENT], &g_after_battle_saved_thread, sizeof(g_after_battle_saved_thread));
    g_menu_thread_menu_data = saved_menu_descriptor;
    thread_resume(THREAD_SLOT_EVENT);
    thread_yield();
    thread_current_exit();
}
