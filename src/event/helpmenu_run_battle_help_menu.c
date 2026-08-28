#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_runtime.h"
#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/helpmenu.h"
#include "fft/main_heap.h"
#include "fft/main_sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_thread_start(s32 thread_id, void (*function)(void));

/* Run the battle help menu thread.
 *
 * The s16 fields read at 0x38/0x3a belong to help_request_t, reached through
 * the thread's function_parameter_1 word, not to battle_thread_t. The dispatch
 * cases share their wait-and-close tail through cross-jumping; the default case
 * passes its u16 text id directly so the shared tail starts where the target's
 * does. One `help_id` carries the id through every case, the confirm handler and
 * the direction index, which places it in $s1 and the primitive base in $s8.
 */
void helpmenu_run_battle_help_menu(void) {
    POLY_FT4 cursor_polys[2];
    POLY_FT4 shadow_polys[2];
    /* The target holds four values in the 8-byte-strided slots at sp+0xb0,
       0xb8, 0xc0 and 0xc8 and reloads them at each use. A local aggregate is
       what keeps GCC 2.6.3 from register-allocating them; the trailing pad
       reproduces the 0x100 frame: declared vars run to sp+0xd8, where the
       saved registers begin. The cursor x is reloaded with lw for arithmetic
       and lhu for the direct x0/x2 stores. */
    struct {
        help_navigation_record_t* navigation;
        s32 pad_b4;
        s32 menu_index;
        s32 pad_bc;
        union {
            s32 w;
            u16 h;
        } cursor_x;
        s32 pad_c4;
        battle_thread_t* text_thread;
        s32 pad_cc;
        s32 pad_d0;
        s32 pad_d4;
    } local;
    /* Each register pin is load-bearing: unpinning it alone, or with its
       same-register partners, changes the allocation. */
    s32 help_id;
    register s32 reload_neighbor __asm__("$3");
    s32 thread_id;
    s32 thread_id2;
    s16* value_ptr;
    s16 request_kind;
    s32 help_base;
    s32 reloaded_base;
    s32 cursor_bob;
    register s32 cursor_y __asm__("$22");
    s32* input_ptr;
    s32 buffer_index;
    s32 text_base;
    s32 help_mode;
    s32 input;
    s32 entry_id;
    register s32 wait_slot_offset __asm__("$16");
    s32 poly_offset;
    s32 poly_index;
    /* frame first walks the thread slots looking for a help request. */
    register s32 frame __asm__("$19");
    register s32 text_thread_id __asm__("$21");
    s32 id_offset;
    s32 id_table;
    u8* scan_slot;
    POLY_FT4* init_poly;
    POLY_FT4* init_shadow_poly;
    register help_request_t* request __asm__("$18");
    battle_thread_t* self_thread;
    help_request_table_t* request_table;
    help_navigation_record_t* navigation_entry;
    void* banner;
    void* unit_data;
    void* billboard;
    void* cursor_tile;
    s32* controller_state;
    s32 missing_value;
    register s32 scratch __asm__("$9");
    register battle_thread_t* current_thread __asm__("$9");
    /* One s16 carries the right x and then the shadow y. */
    s16 horiz_coord;
    s16 vert_right_x;
    register s16 vert_bottom_y __asm__("$2");
    register s16 vert_shadow_y __asm__("$2");
    s32 vert_y;
    register s16 horiz_edge __asm__("$3");
    s32 horiz_addr;
    register s32 horiz_shadow_x __asm__("$3");
    /* The 0xA8 and then the 0xC8 texture u share one variable. */
    s32 tex_u_alt;
    register s16 horiz_shadow_bottom_y __asm__("$3");
    register s16 shadow_right_x __asm__("$5");
    s16 shadow_clut;
    s32 submit_offset;
    s32 vert_offset;
    register s32 active_offset __asm__("$3");
    register s32 idle_offset __asm__("$3");
    register s32 tex_u __asm__("$5");
    register POLY_FT4* vert_poly __asm__("$4");
    register POLY_FT4* vert_shadow_poly __asm__("$2");
    POLY_FT4* horiz_poly;
    POLY_FT4* clut_poly;
    s32 cursor_height;
    u8* cursor_base;
    s32 navigation_offset;

    if (g_option_menu_open != 0) {
        battle_thread_exit_current();
    }
    battle_copy_bytes(g_helpmenu_text_saved_pointers, g_battle_text_section_pointers, 0x80);
    banner = g_helpmenu_active_banner;
    unit_data = g_helpmenu_active_unit_data;
    billboard = &g_helpmenu_billboard;
    cursor_tile = g_helpmenu_menu_cursor_tile;
    g_battle_text_section_pointers[1] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[1];
    g_battle_text_section_pointers[2] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[2];
    g_battle_text_section_pointers[11] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[11];
    g_battle_text_section_pointers[12] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[12];
    g_battle_text_section_pointers[13] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[13];
    g_battle_text_section_pointers[15] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[15];
    g_battle_text_section_pointers[19] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[19];
    g_battle_text_section_pointers[20] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[20];
    self_thread = (battle_thread_t*)((g_battle_current_thread_id << 0xA) + (u8*)g_battle_threads);
    battle_action_copy_at_and_cursor_to(banner, unit_data, billboard, cursor_tile);
    if ((g_helpmenu_selected_unit_panel_mode != 0) || (g_battle_post_battle_unit_changes_active != 0)) {
        g_helpmenu_require_navigation[2].destination[2] = 7;
        g_helpmenu_require_navigation[7].destination[3] = 2;
    }
    poly_index = 0;
    text_thread_id = helpmenu_thread_find_running_from_4();
    controller_state = battle_script_get_controller_input_pointer(0);
    poly_offset = 0;
    g_helpmenu_controller_state_ptr = controller_state;
    local.menu_index = 0;
    do {
        init_poly = (POLY_FT4*)((u8*)cursor_polys + poly_offset);
        battle_gfx_init_default_poly_ft4(init_poly);
        SetSemiTrans(init_poly, 0);
        init_poly->tpage = GetTPage(0, 0, 0x3C0, 0x100);
        init_shadow_poly = (POLY_FT4*)((u8*)shadow_polys + poly_offset);
        init_poly->clut = 0x7D7C;
        battle_gfx_init_default_poly_ft4(init_shadow_poly);
        SetSemiTrans(init_shadow_poly, 1);
        init_shadow_poly->tpage = GetTPage(0, 2, 0x3C0, 0x100);
        poly_index += 1;
        init_shadow_poly->clut = 0x7DBC;
        poly_offset += 0x28;
    } while (poly_index < 2);
    frame = text_thread_id;
    if (text_thread_id < 9) {
        missing_value = -1;
        /* Walked as a u8* stepped by 0x400; a battle_thread_t* induction
           variable adds an instruction. */
        scan_slot = (frame << 0xA) + (u8*)g_battle_threads;
        /* Skip threads without a pending request. `continue`/`break` keep
           the kind test on top, as in the target. */
        for (;;) {
            request = (help_request_t*)((battle_thread_t*)scan_slot)->function_parameter_1;
            if (request->kind == missing_value) {
                frame += 1;
                scan_slot += 0x400;
                if (frame < 9) {
                    continue;
                }
            }
            break;
        }
        if (frame != 9) {
            if ((s32)g_battle_help_text_id_tables_ptr == missing_value) {
                g_battle_help_text_id_tables_ptr = (s32)g_helpmenu_menu_text_id_tables;
            }
            request_kind = request->kind;
            if (request_kind != 0x10) {
                if (request_kind == 0x100) {
                    thread_id = text_thread_id - 1;
                    help_id = *request->value;
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    battle_thread_start(thread_id, battle_text_character_handling_thread);
                    help_id &= 0x7FF;
                    help_id |= 0x7800;
                    /* Keep the packed help id in its own register: without the
                       extra reference combine folds the `ori` into the argument
                       copy, where the target keeps `ori s1` plus `move a2,s1`. */
                    __asm__("" : "=r"(help_id) : "0"(help_id));
                    battle_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    *g_helpmenu_controller_state_ptr = 0;
                    battle_thread_wait_frames(1);
                    wait_slot_offset = text_thread_id << 0xA;
                    helpmenu_menu_close();
                } else if (request_kind == 4) {
                    help_id = g_battle_action_menu_skillsets[request->selected_index];
                    thread_id = text_thread_id - 1;
                    if (help_id == 0xBC) {
                        *g_helpmenu_controller_state_ptr = 0;
                        battle_thread_wait_frames(1);
                        helpmenu_menu_close();
                    }
                    help_id |= 0x9800;
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    battle_thread_start(thread_id, battle_text_character_handling_thread);
                    battle_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    wait_slot_offset = text_thread_id << 0xA;
                } else if (request_kind == 0) {
                    *g_helpmenu_controller_state_ptr = 0;
                    battle_thread_wait_frames(1);
                    wait_slot_offset = text_thread_id << 0xA;
                    helpmenu_menu_close();
                } else if (request_kind == 5) {
                    thread_id = text_thread_id - 1;
                    request_table = request->table;
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    help_id = request_table->primary_ids[request->selected_index];
                    battle_thread_start(thread_id, battle_text_character_handling_thread);
                    help_id &= 0x7FF;
                    help_id |= 0x7800;
                    /* Keep the packed help id in its own register: without the
                       extra reference combine folds the `ori` into the argument
                       copy, where the target keeps `ori s1` plus `move a2,s1`. */
                    __asm__("" : "=r"(help_id) : "0"(help_id));
                    battle_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    wait_slot_offset = text_thread_id << 0xA;
                } else if (request_kind == 0x11) {
                    help_id = request->table->primary_ids[request->selected_index];
                    help_id &= 0x7FF;
                    thread_id = text_thread_id - 1;
                    if (help_id == 0) {
                        *g_helpmenu_controller_state_ptr = 0;
                        battle_thread_wait_frames(1);
                        helpmenu_menu_close();
                    }
                    help_id |= 0x6800;
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    battle_thread_start(thread_id, battle_text_character_handling_thread);
                    battle_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    wait_slot_offset = text_thread_id << 0xA;
                } else if (request_kind == 0x17) {
                    help_id = request->table->primary_ids[request->selected_index];
                    help_id &= 0x7FF;
                    thread_id = text_thread_id - 1;
                    if (help_id == 0) {
                        *g_helpmenu_controller_state_ptr = 0;
                        battle_thread_wait_frames(1);
                        helpmenu_menu_close();
                    }
                    help_id += 0x26;
                    help_id |= 0x7800;
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    battle_thread_start(thread_id, battle_text_character_handling_thread);
                    battle_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    wait_slot_offset = text_thread_id << 0xA;
                } else if (request_kind == 0x12) {
                    request_table = request->table;
                    help_id = *request_table->mode_flag;
                    if (help_id == 0) {
                        id_table = (s32)request_table->primary_ids;
                        id_offset = request->selected_index * 2;
                    } else {
                        id_table = (s32)request_table->secondary_ids;
                        id_offset = request->selected_index * 2;
                    }
                    help_id = *(s16*)(id_offset + id_table);
                    help_id &= 0x7FF;
                    thread_id = text_thread_id - 1;
                    if (help_id == 0) {
                        *g_helpmenu_controller_state_ptr = 0;
                        battle_thread_wait_frames(1);
                        helpmenu_menu_close();
                    }
                    help_id |= 0xA000;
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    battle_thread_start(thread_id, battle_text_character_handling_thread);
                    battle_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    wait_slot_offset = text_thread_id << 0xA;
                } else if (request_kind == 0x13) {
                    thread_id = text_thread_id - 1;
                    help_id = request->table->secondary_ids[request->selected_index];
                    text_base = help_id & 0xF800;
                    if (text_base == TEXT_ID_ABILITY_NAME_BASE) {
                        help_id &= 0x7FF;
                        help_id |= 0x7800;
                    } else if (text_base == 0xB000) {
                        help_id &= 0x7FF;
                        help_id |= 0x9800;
                    } else if (text_base == TEXT_ID_ITEM_NAME_BASE) {
                        help_id &= 0x7FF;
                        if ((u32)(help_id - 0x26) < 0xAU) {
                            help_id += 0x26;
                            help_id |= 0x7800;
                        } else {
                            help_id |= 0x6800;
                        }
                    } else {
                        help_id = 0x807;
                    }
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    thread_id2 = text_thread_id - 1;
                    battle_thread_start(thread_id2, battle_text_character_handling_thread);
                    battle_thread_set_parameters(thread_id2, 0x3B, help_id, 0);
                    wait_slot_offset = text_thread_id << 0xA;
                } else {
                    thread_id = text_thread_id - 1;
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    g_helpmenu_current_text_ids = (u16*)((s32*)g_battle_help_text_id_tables_ptr)[request_kind];
                    battle_thread_start(thread_id, battle_text_character_handling_thread);
                    battle_thread_set_parameters(
                        thread_id, 0x3B, g_helpmenu_current_text_ids[request->selected_index], 0);
                    wait_slot_offset = text_thread_id << 0xA;
                }
                /* Wait for the text thread in the preceding slot. `offset + (s32)base`
                   keeps the target's addu operand order; pointer + int swaps it. */
                do {
                    battle_thread_wait_frames(1);
                } while (((battle_thread_t*)(wait_slot_offset + (s32)g_battle_threads))[-1].is_running != 0);
                *g_helpmenu_controller_state_ptr = 0;
                battle_thread_wait_frames(1);
                helpmenu_menu_close();
            }
        } else {
            *g_helpmenu_controller_state_ptr = 0;
            battle_thread_wait_frames(1);
            helpmenu_menu_close();
        }
    }
    if (text_thread_id < 9) {
        scratch = (s32)g_helpmenu_require_navigation;
        local.navigation = (help_navigation_record_t*)scratch;
        text_thread_id -= 1;
        frame = 0;
    } else {
        help_mode = g_battle_unit_view_mode;
        if ((u32)(help_mode - 1) < 2U) {
            g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
            battle_thread_start(2, battle_text_character_handling_thread);
            battle_thread_set_parameters(2, 0x3B, g_helpmenu_menu_cursor_tile[0] + 0x5800, 0);
            ((battle_thread_t*)g_battle_threads)[2].task_words[3] = -0x20;
            battle_thread_wait_until_inactive(2);
            *g_helpmenu_controller_state_ptr = 0;
            battle_thread_wait_frames(1);
            frame = 0;
            helpmenu_menu_close();
        } else if (help_mode == 2) {
            /* Unreachable: the arm above already takes modes 1 and 2. */
            scratch = (s32)g_helpmenu_view_mode_2_navigation;
            local.navigation = (help_navigation_record_t*)scratch;
            text_thread_id = 2;
            frame = 0;
        } else if (help_mode == 3) {
            scratch = (s32)g_helpmenu_view_mode_3_navigation;
            local.navigation = (help_navigation_record_t*)scratch;
            text_thread_id = 2;
            frame = 0;
            /* The render loop's cancel handler: the target places it here,
               after this arm, where the arm's `bne` skips it. */
            if (0) {
            cancel_close:
                g_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                goto close_and_return;
            }
        } else if (help_mode == 4) {
            scratch = (s32)g_helpmenu_view_mode_4_navigation;
            local.navigation = (help_navigation_record_t*)scratch;
            text_thread_id = 2;
            frame = 0;
        } else {
            frame = 0;
        }
    }
    cursor_base = (u8*)cursor_polys;
    cursor_height = 0x10;
    local.text_thread = (battle_thread_t*)((text_thread_id << 0xA) + (u8*)g_battle_threads);
/* Label loop: a for (;;) adds loop depth to every pseudo's weight and changes the allocation. */
loop_73:
    current_thread = local.text_thread;
    if (current_thread->is_running == 0) {
        if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_HELP_MENU) != 0) {
            g_event_mode = 0;
        }
        if (self_thread->task_words[1] != 0) {
            self_thread->task_words[1] = 0;
            self_thread->task_words[0] = 0;
        }
        scratch = local.menu_index;
        /* The target reloads the menu index and then the table base into the same
           scratch register (lw $9,0xb8(sp); sll; lw $9,0xb0(sp)). This keeps the
           index reload in $9; without it the index loads into v0 and the two
           reloads issue back to back. */
        __asm__("" : "=r"(scratch) : "0"(scratch));
        navigation_offset = scratch * 0x10;
        scratch = (s32)local.navigation;
        navigation_entry = (help_navigation_record_t*)((u8*)scratch + navigation_offset);
        cursor_y = navigation_entry->y;
        scratch = navigation_entry->x;
        local.cursor_x.w = scratch;
        if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_DEBUG_UNIT_EDITOR) != 0) {
            cursor_y += 8;
        }
        if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_HELP_MENU) != 0) {
            cursor_y += 8;
        }
        input_ptr = g_helpmenu_controller_state_ptr;
        input = *input_ptr;
        /* The empty loop's notes mark `cancel_close` as outside a loop, so reorg
           predicts this branch not-taken and fills its delay slot from the
           fall-through instead of taking `li v0,2` from the cancel block. */
        do {
            if (input & PSX_PAD_CROSS) {
                goto cancel_close;
            }
        } while (0);
        if (input & PSX_PAD_CIRCLE) {
            value_ptr = navigation_entry->value;
            if (value_ptr != 0) {
                s32 raw;
                s32 raw2;
                help_base = navigation_entry->help_base;
                raw = *value_ptr;
                help_id = help_base + raw;
                raw2 = raw;
                /* Keep the raw help id in its own register: without it the value
                   loads straight into a0 and the help_base sum drops into the
                   branch delay slot instead of preceding the compare. */
                __asm__("" : "=r"(raw2) : "0"(raw2));
                if (help_base == -0x6800 && g_helpmenu_billboard.generic_monster != 0) {
                    entry_id = raw2 & 0x7FF;
                    help_id = raw2 | 0x7800;
                    if ((entry_id == 0x7FF) || (entry_id == 0)) {
                        *input_ptr = 0;
                    } else {
                        g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                        battle_thread_start(text_thread_id, battle_text_character_handling_thread);
                        battle_thread_set_parameters(text_thread_id, 0x3B, help_id, 0);
                    }
                } else if ((navigation_entry->help_base == 0x7800)
                    && ((u32)(*(u16*)navigation_entry->value - 1) >= 0x301U)) {
                    *g_helpmenu_controller_state_ptr = 0;
                } else {
                    reloaded_base = navigation_entry->help_base;
                    if ((reloaded_base == 0x6800) && ((u32)(*(u16*)navigation_entry->value - 1) >= 0x101U)) {
                        *g_helpmenu_controller_state_ptr = 0;
                    } else if ((reloaded_base == -0x6800) && ((u32)(*(u16*)navigation_entry->value - 1) >= 0x101U)) {
                        *g_helpmenu_controller_state_ptr = 0;
                    } else {
                        g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                        battle_thread_start(text_thread_id, battle_text_character_handling_thread);
                        battle_thread_set_parameters(text_thread_id, 0x3B, help_id, 0);
                    }
                }
            } else {
                help_id = navigation_entry->help_base;
                g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                battle_thread_start(text_thread_id, battle_text_character_handling_thread);
                battle_thread_set_parameters(text_thread_id, 0x3B, help_id, 0);
            }
        } else if (input & PSX_PAD_DPAD_MASK) {
            help_id = 0;
            if (!(input & PSX_PAD_UP)) {
                if (input & PSX_PAD_DOWN) {
                    help_id = 1;
                } else {
                    help_id = 3;
                    if (input & PSX_PAD_RIGHT) {
                        help_id = 2;
                    }
                }
            }
            navigation_offset = navigation_entry->destination[help_id];
            scratch = local.menu_index;
            if (scratch != navigation_offset) {
                g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
            }
            reload_neighbor = navigation_entry->destination[help_id];
            local.menu_index = reload_neighbor;
        }
    } else {
        if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_HELP_MENU) != 0) {
            g_event_mode = 1;
        }
        if (self_thread->task_words[1] == 0) {
            self_thread->task_words[1] = 2;
            self_thread->task_words[0] = 0;
        }
    }
    if (self_thread->task_words[1] != 0) {
        cursor_bob = battle_menu_get_cursor_bob_offset(1);
    } else {
        cursor_bob = battle_menu_get_cursor_bob_offset(0);
    }
    self_thread->task_words[0] = (s32)(self_thread->task_words[0] + g_battle_event_speed);
    cursor_bob -= 4;
    buffer_index = frame & 1;
    /* vert_* locals build the cursor that bobs along y, horiz_* the one that
       bobs along x. */
    if (navigation_entry->vertical_cursor != 0) {
        tex_u = 0xA8;
        vert_offset = buffer_index * 0x28;
        vert_poly = (POLY_FT4*)(cursor_base + vert_offset);
        vert_poly->u0 = tex_u;
        vert_poly->u1 = tex_u;
        tex_u = 0xB8;
        scratch = 0xB8;
        vert_poly->u2 = scratch;
        /* Scheduling boundary: without it the sp+0x60 primitive address is
           hoisted above this store and it lands three slots late. */
        __asm__ volatile("");
        vert_shadow_poly = (POLY_FT4*)((u8*)shadow_polys + vert_offset);
        tex_u_alt = 0xC8;
        vert_poly->u3 = scratch;
        vert_poly->v0 = 0;
        vert_poly->v1 = cursor_height;
        vert_poly->v2 = 0;
        vert_poly->v3 = cursor_height;
        vert_shadow_poly->u0 = tex_u;
        vert_shadow_poly->u1 = tex_u;
        vert_shadow_poly->u2 = tex_u_alt;
        vert_shadow_poly->u3 = tex_u_alt;
        vert_shadow_poly->v0 = 0;
        vert_shadow_poly->v1 = cursor_height;
        vert_shadow_poly->v2 = 0;
        vert_shadow_poly->v3 = cursor_height;
        scratch = local.cursor_x.h;
        vert_y = cursor_y + cursor_bob;
        vert_poly->y0 = vert_y;
        vert_poly->x0 = scratch;
        /* Scheduling boundaries around the bottom-y sum: the second keeps its
           `addiu v0,v1,0x10` in the cursor-x `lw` delay slot instead of after the
           y1 store; without the first, the frame-parity temporaries earlier in
           the function shift from a2/a3 to a3/t0. */
        scratch = local.cursor_x.w;
        __asm__ volatile("");
        vert_bottom_y = vert_y + 0x10;
        __asm__ volatile("");
        vert_poly->y1 = vert_y;
        vert_right_x = scratch + 0x10;
        vert_poly->x1 = vert_right_x;
        scratch = local.cursor_x.h;
        vert_poly->y2 = vert_bottom_y;
        vert_poly->y3 = vert_bottom_y;
        vert_shadow_y = vert_y + 2;
        vert_poly->x3 = vert_right_x;
        vert_poly->x2 = scratch;
        scratch = local.cursor_x.w;
        vert_y += 0x12;
        vert_poly[2].y0 = vert_shadow_y;
        /* cursor_bob now holds the shadow quad's left x. */
        cursor_bob = scratch + 2;
        shadow_right_x = scratch + 0x12;
        /* Scheduling boundary: without it the list scheduler sinks these two
           `addiu` below the shadow quad's y stores. */
        __asm__ volatile("");
        vert_poly[2].x0 = cursor_bob;
        vert_poly[2].x1 = shadow_right_x;
        vert_poly[2].y1 = vert_shadow_y;
        vert_poly[2].x2 = cursor_bob;
        vert_poly[2].y2 = vert_y;
        vert_poly[2].x3 = shadow_right_x;
        vert_poly[2].y3 = vert_y;
    } else {
        tex_u = 0xA8;
        horiz_addr = buffer_index * 0x28;
        horiz_addr += (s32)cursor_base;
        horiz_poly = (POLY_FT4*)horiz_addr;
        horiz_poly->u0 = tex_u;
        tex_u = 0xB8;
        scratch = 0xB8;
        tex_u_alt = 0xA8;
        horiz_poly->u1 = scratch;
        /* Scheduling boundary: keeps the u1 store ahead of the u2 store. */
        __asm__ volatile("");
        horiz_poly->u2 = tex_u_alt;
        tex_u_alt = 0xC8;
        horiz_poly->u3 = scratch;
        horiz_poly->v0 = 0;
        horiz_poly->v1 = 0;
        horiz_poly->v2 = cursor_height;
        horiz_poly->v3 = cursor_height;
        horiz_poly[2].u0 = tex_u;
        horiz_poly[2].v0 = 0;
        horiz_poly[2].u1 = tex_u_alt;
        horiz_poly[2].v1 = 0;
        horiz_poly[2].u2 = scratch;
        horiz_poly[2].v2 = cursor_height;
        horiz_poly[2].u3 = tex_u_alt;
        horiz_poly[2].v3 = cursor_height;
        scratch = local.cursor_x.w;
        horiz_poly->y0 = cursor_y;
        horiz_poly->y1 = cursor_y;
        horiz_shadow_x = scratch + cursor_bob;
        horiz_coord = horiz_shadow_x + 0x10;
        horiz_poly->x0 = horiz_shadow_x;
        horiz_poly->x2 = horiz_shadow_x;
        horiz_edge = cursor_y + 0x10;
        horiz_poly->y2 = horiz_edge;
        horiz_poly->y3 = horiz_edge;
        horiz_shadow_x = cursor_bob + 2;
        horiz_shadow_x = scratch + horiz_shadow_x;
        horiz_poly->x1 = horiz_coord;
        horiz_poly->x3 = horiz_coord;
        horiz_coord = cursor_y + 2;
        shadow_right_x = horiz_shadow_x + 0x10;
        horiz_poly[2].x0 = horiz_shadow_x;
        horiz_poly[2].x2 = horiz_shadow_x;
        horiz_shadow_bottom_y = cursor_y + 0x12;
        horiz_poly[2].y0 = horiz_coord;
        horiz_poly[2].x1 = shadow_right_x;
        horiz_poly[2].y1 = horiz_coord;
        horiz_poly[2].y2 = horiz_shadow_bottom_y;
        horiz_poly[2].x3 = shadow_right_x;
        horiz_poly[2].y3 = horiz_shadow_bottom_y;
    }
    if (self_thread->task_words[1] != 0) {
        active_offset = buffer_index * 0x28;
        ((POLY_FT4*)(cursor_base + active_offset))->clut = 0x7DFC;
        clut_poly = (POLY_FT4*)((u8*)shadow_polys + active_offset);
        shadow_clut = 0x7E3C;
    } else {
        idle_offset = buffer_index * 0x28;
        ((POLY_FT4*)(cursor_base + idle_offset))->clut = 0x7D7C;
        clut_poly = (POLY_FT4*)((u8*)shadow_polys + idle_offset);
        shadow_clut = 0x7DBC;
    }
    clut_poly->clut = shadow_clut;
    submit_offset = buffer_index * 0x28;
    battle_gfx_draw_or_append_gpu_primitive(cursor_base + submit_offset);
    battle_gfx_draw_or_append_gpu_primitive((u8*)shadow_polys + submit_offset);
    battle_thread_wait_frames(1);
    frame += 1;
    goto loop_73;
close_and_return:
    *g_helpmenu_controller_state_ptr = 0;
    battle_thread_wait_frames(1);
    helpmenu_menu_close();
}
