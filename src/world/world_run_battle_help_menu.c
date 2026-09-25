/* WORLD copy of the EVENT HELPMENU battle help menu
 * (helpmenu_run_battle_help_menu), byte-identical apart from addresses.
 *
 * The register pins, scheduling boundaries and the load-bearing
 * `do { ... } while (0)` around the cancel test are the HELPMENU twin's; see
 * src/event/helpmenu_run_battle_help_menu.c for why each one is required.
 *
 * Boundary evidence: the range disassembles to the same 865 instructions as
 * the twin, in the same order, with identical registers and immediates; only
 * global addresses and call targets differ. The trailing 0x4c bytes at
 * 0x801055b4 are the out-of-line `close_and_return` tail of this same
 * function, as in the twin.
 */
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Opens help for the running menu's pending request, or runs the bouncing
 * help cursor over the navigation table for that menu or the unit view. */
void world_run_battle_help_menu(void) {
    POLY_FT4 cursor_quads[2];
    POLY_FT4 shadow_quads[2];
    /* The target holds four values in the 8-byte-strided slots at sp+0xb0,
       0xb8, 0xc0 and 0xc8 and reloads them at each use. A local aggregate is
       what keeps GCC 2.6.3 from register-allocating them; the trailing pad
       reproduces the 0x100 frame: declared vars run to sp+0xd8, where the
       saved registers begin. */
    struct {
        world_help_navigation_entry_t* navigation;
        s32 pad_b4;
        s32 menu_index;
        s32 pad_bc;
        union {
            s32 w;
            u16 h;
        } cursor_x;
        s32 pad_c4;
        native_thread_t* help_thread;
        s32 pad_cc;
        s32 pad_d0;
        s32 pad_d4;
    } local;
    s32 help_id; /* also the pad direction index */
    register s32 reload_neighbor __asm__("$3");
    s32 thread_id;
    s32 thread_id2;
    s16* value_ptr;
    s16 request_kind;
    s32 value_base;
    s32 base_reload;
    s32 bounce; /* cursor bounce; then the vertical shadow left x */
    register s32 cursor_y __asm__("$22");
    s32* input_ptr;
    s32 buffer_index;
    s32 text_section;
    s32 view_mode;
    s32 input;
    s32 help_index;
    register s32 help_thread_offset __asm__("$16");
    s32 quad_offset;
    s32 quad_index;
    register s32 scan_id __asm__("$19");
    register s32 frame __asm__("$19");
    register s32 help_thread_id __asm__("$21");
    s32 id_offset;
    s32 id_table;
    u8* scan_thread;
    POLY_FT4* init_cursor;
    POLY_FT4* init_shadow;
    register world_help_request_t* request __asm__("$18");
    native_thread_t* self;
    world_help_request_table_t* request_table;
    world_help_navigation_entry_t* navigation_entry;
    void* setup_banner;
    void* setup_unit_data;
    void* setup_billboard;
    void* setup_cursor_tile;
    s32* controller_state;
    s32 missing_value;
    register s32 t1_scratch __asm__("$9"); /* menu index, table base, texture u or cursor x reload */
    register native_thread_t* current_thread __asm__("$9");
    register s16 h_right __asm__("$4");
    register s16 h_shadow_top __asm__("$4");
    s16 v_right;
    register s16 v_bottom __asm__("$2");
    register s16 v_shadow_top __asm__("$2");
    s32 v_top; /* later the shadow bottom */
    register s16 h_bottom __asm__("$3");
    s32 h_quad_addr;
    register s32 h_shadow_left __asm__("$3");
    register s32 h_tex_u __asm__("$3");
    register s32 shadow_tex_u __asm__("$3");
    register s16 h_shadow_bottom __asm__("$3");
    register s16 shadow_right __asm__("$5");
    s16 shadow_clut;
    s32 draw_offset;
    s32 v_offset;
    register s32 active_offset __asm__("$3");
    register s32 idle_offset __asm__("$3");
    register s32 tex_u __asm__("$5");
    register POLY_FT4* v_quad __asm__("$4");
    register POLY_FT4* v_shadow __asm__("$2");
    POLY_FT4* h_quad;
    POLY_FT4* shadow_quad;
    s32 tex_v_max;
    u8* cursor_quad_base;
    s32 navigation_offset;

    if (g_world_menu_transition_active != 0) {
        world_thread_exit_current();
    }
    world_script_copy_bytes(g_world_text_section_pointer_backup, g_world_text_section_pointers, 0x80);
    setup_banner = g_world_help_active_banner;
    setup_unit_data = g_world_help_active_unit_data;
    setup_billboard = &g_world_help_billboard;
    setup_cursor_tile = g_world_help_menu_cursor_tile;
    g_world_text_section_pointers[1] = (u8*)(g_world_help_text_section_offsets[1] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[2] = (u8*)(g_world_help_text_section_offsets[2] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[11] = (u8*)(g_world_help_text_section_offsets[11] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[12] = (u8*)(g_world_help_text_section_offsets[12] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[13] = (u8*)(g_world_help_text_section_offsets[13] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[15] = (u8*)(g_world_help_text_section_offsets[15] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[19] = (u8*)(g_world_help_text_section_offsets[19] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[20] = (u8*)(g_world_help_text_section_offsets[20] + (s32)g_world_help_data_base);
    self = (native_thread_t*)((g_world_thread_current_id << 0xA) + (u8*)g_world_threads);
    world_gfx_copy_screen_setup_out(setup_banner, setup_unit_data, setup_billboard, setup_cursor_tile);
    if ((g_world_selected_unit_stat_summary.team_state != 0) || (g_world_unit_join_screen_active != 0)) {
        g_world_help_require_navigation[2].neighbors[2] = 7;
        g_world_help_require_navigation[7].neighbors[3] = 2;
    }
    quad_index = 0;
    help_thread_id = world_thread_find_running_at_or_after_4();
    controller_state = (s32*)world_input_get_menu_controller(0);
    quad_offset = 0;
    g_world_help_controller_state_ptr = controller_state;
    local.menu_index = 0;
    do {
        init_cursor = (POLY_FT4*)((u8*)cursor_quads + quad_offset);
        world_menu_init_quad(init_cursor);
        SetSemiTrans(init_cursor, 0);
        init_cursor->tpage = GetTPage(0, 0, 0x3C0, 0x100);
        init_shadow = (POLY_FT4*)((u8*)shadow_quads + quad_offset);
        init_cursor->clut = 0x7D7C;
        world_menu_init_quad(init_shadow);
        SetSemiTrans(init_shadow, 1);
        init_shadow->tpage = GetTPage(0, 2, 0x3C0, 0x100);
        quad_index += 1;
        init_shadow->clut = 0x7DBC;
        quad_offset += 0x28;
    } while (quad_index < 2);
    scan_id = help_thread_id;
    if (help_thread_id < 9) {
        missing_value = -1;
        scan_thread = (scan_id << 0xA) + (u8*)g_world_threads;
        /* Skip threads without a pending request. `continue`/`break` keep
           the kind test on top, as in the target. */
        for (;;) {
            request = (world_help_request_t*)((native_thread_t*)scan_thread)->function_parameter_1;
            if (request->kind == missing_value) {
                scan_id += 1;
                scan_thread += 0x400;
                if (scan_id < 9) {
                    continue;
                }
            }
            break;
        }
        if (scan_id != 9) {
            if ((s32)g_world_help_text_id_tables_ptr == missing_value) {
                g_world_help_text_id_tables_ptr = (s32)g_world_help_menu_text_id_tables;
            }
            request_kind = request->kind;
            if (request_kind != 0x10) {
                if (request_kind == 0x100) {
                    thread_id = help_thread_id - 1;
                    help_id = *request->value;
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    world_thread_start(thread_id, world_text_character_handling_thread);
                    help_id &= 0x7FF;
                    help_id |= 0x7800;
                    /* Keep the packed help id in its own register: without the
                       extra reference combine folds the `ori` into the argument
                       copy, where the target keeps `ori s1` plus `move a2,s1`. */
                    __asm__("" : "=r"(help_id) : "0"(help_id));
                    world_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    *g_world_help_controller_state_ptr = 0;
                    world_thread_wait_frames(1);
                    help_thread_offset = help_thread_id << 0xA;
                    world_text_restore_sections_and_stop_thread();
                } else if (request_kind == 4) {
                    help_id = g_world_action_menu_skillsets[request->selected_index];
                    thread_id = help_thread_id - 1;
                    if (help_id == 0xBC) {
                        *g_world_help_controller_state_ptr = 0;
                        world_thread_wait_frames(1);
                        world_text_restore_sections_and_stop_thread();
                    }
                    help_id |= 0x9800;
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    world_thread_start(thread_id, world_text_character_handling_thread);
                    world_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    help_thread_offset = help_thread_id << 0xA;
                } else if (request_kind == 0) {
                    *g_world_help_controller_state_ptr = 0;
                    world_thread_wait_frames(1);
                    help_thread_offset = help_thread_id << 0xA;
                    world_text_restore_sections_and_stop_thread();
                } else if (request_kind == 5) {
                    thread_id = help_thread_id - 1;
                    request_table = request->table;
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    help_id = request_table->primary_ids[request->selected_index];
                    world_thread_start(thread_id, world_text_character_handling_thread);
                    help_id &= 0x7FF;
                    help_id |= 0x7800;
                    /* Keep the packed help id in its own register: without the
                       extra reference combine folds the `ori` into the argument
                       copy, where the target keeps `ori s1` plus `move a2,s1`. */
                    __asm__("" : "=r"(help_id) : "0"(help_id));
                    world_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    help_thread_offset = help_thread_id << 0xA;
                } else if (request_kind == 0x11) {
                    help_id = request->table->primary_ids[request->selected_index];
                    help_id &= 0x7FF;
                    thread_id = help_thread_id - 1;
                    if (help_id == 0) {
                        *g_world_help_controller_state_ptr = 0;
                        world_thread_wait_frames(1);
                        world_text_restore_sections_and_stop_thread();
                    }
                    help_id |= 0x6800;
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    world_thread_start(thread_id, world_text_character_handling_thread);
                    world_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    help_thread_offset = help_thread_id << 0xA;
                } else if (request_kind == 0x17) {
                    help_id = request->table->primary_ids[request->selected_index];
                    help_id &= 0x7FF;
                    thread_id = help_thread_id - 1;
                    if (help_id == 0) {
                        *g_world_help_controller_state_ptr = 0;
                        world_thread_wait_frames(1);
                        world_text_restore_sections_and_stop_thread();
                    }
                    help_id += 0x26;
                    help_id |= 0x7800;
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    world_thread_start(thread_id, world_text_character_handling_thread);
                    world_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    help_thread_offset = help_thread_id << 0xA;
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
                    thread_id = help_thread_id - 1;
                    if (help_id == 0) {
                        *g_world_help_controller_state_ptr = 0;
                        world_thread_wait_frames(1);
                        world_text_restore_sections_and_stop_thread();
                    }
                    help_id |= 0xA000;
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    world_thread_start(thread_id, world_text_character_handling_thread);
                    world_thread_set_parameters(thread_id, 0x3B, help_id, 0);
                    help_thread_offset = help_thread_id << 0xA;
                } else if (request_kind == 0x13) {
                    thread_id = help_thread_id - 1;
                    help_id = request->table->secondary_ids[request->selected_index];
                    text_section = help_id & 0xF800;
                    if (text_section == TEXT_ID_ABILITY_NAME_BASE) {
                        help_id &= 0x7FF;
                        help_id |= 0x7800;
                    } else if (text_section == 0xB000) {
                        help_id &= 0x7FF;
                        help_id |= 0x9800;
                    } else if (text_section == TEXT_ID_ITEM_NAME_BASE) {
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
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    thread_id2 = help_thread_id - 1;
                    world_thread_start(thread_id2, world_text_character_handling_thread);
                    world_thread_set_parameters(thread_id2, 0x3B, help_id, 0);
                    help_thread_offset = help_thread_id << 0xA;
                } else {
                    thread_id = help_thread_id - 1;
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    g_world_help_topic_text_ids = (u16*)((s32*)g_world_help_text_id_tables_ptr)[request_kind];
                    world_thread_start(thread_id, world_text_character_handling_thread);
                    world_thread_set_parameters(
                        thread_id, 0x3B, g_world_help_topic_text_ids[request->selected_index], 0);
                    help_thread_offset = help_thread_id << 0xA;
                }
                do {
                    world_thread_wait_frames(1);
                } while (((native_thread_t*)(help_thread_offset + (s32)g_world_threads))[-1].is_running != 0);
                *g_world_help_controller_state_ptr = 0;
                world_thread_wait_frames(1);
                world_text_restore_sections_and_stop_thread();
            }
        } else {
            *g_world_help_controller_state_ptr = 0;
            world_thread_wait_frames(1);
            world_text_restore_sections_and_stop_thread();
        }
    }
    if (help_thread_id < 9) {
        t1_scratch = (s32)g_world_help_require_navigation;
        local.navigation = (world_help_navigation_entry_t*)t1_scratch;
        help_thread_id -= 1;
        frame = 0;
    } else {
        view_mode = g_world_unit_view_mode;
        if ((u32)(view_mode - 1) < 2U) {
            g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
            world_thread_start(2, world_text_character_handling_thread);
            world_thread_set_parameters(2, 0x3B, (*(s16*)g_world_help_menu_cursor_tile) + 0x5800, 0);
            g_world_threads[2].task_words[3] = -0x20;
            world_thread_wait_until_inactive(2);
            *g_world_help_controller_state_ptr = 0;
            world_thread_wait_frames(1);
            frame = 0;
            world_text_restore_sections_and_stop_thread();
        } else if (view_mode == 2) {
            t1_scratch = (s32)g_world_help_terrain_navigation;
            local.navigation = (world_help_navigation_entry_t*)t1_scratch;
            help_thread_id = 2;
            frame = 0;
        } else if (view_mode == 3) {
            t1_scratch = (s32)g_world_help_unit_box_navigation;
            local.navigation = (world_help_navigation_entry_t*)t1_scratch;
            help_thread_id = 2;
            frame = 0;
            /* The cancel handler of the frame loop below: the target places
               it here, after this arm, where the arm's `bne` skips it. */
            if (0) {
            cancel_close:
                g_world_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                goto close_and_return;
            }
        } else if (view_mode == 4) {
            t1_scratch = (s32)g_world_help_attack_preview_navigation;
            local.navigation = (world_help_navigation_entry_t*)t1_scratch;
            help_thread_id = 2;
            frame = 0;
        } else {
            frame = 0;
        }
    }
    cursor_quad_base = (u8*)cursor_quads;
    tex_v_max = 0x10;
    local.help_thread = (native_thread_t*)((help_thread_id << 0xA) + (u8*)g_world_threads);
    /* A label loop: as a C loop the loop notes re-weight register allocation
       and hoist a quad address out of the frame loop. */
loop_73:
    current_thread = local.help_thread;
    if (current_thread->is_running == 0) {
        if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_HELP_MENU) != 0) {
            g_world_thread_task_active = 0;
        }
        if (self->task_words[1] != 0) {
            self->task_words[1] = 0;
            self->task_words[0] = 0;
        }
        t1_scratch = local.menu_index;
        /* The target reloads the menu index and then the table base into the same
           scratch register (lw $9,0xb8(sp); sll; lw $9,0xb0(sp)). This keeps the
           index reload in $9; without it the index loads into v0 and the two
           reloads issue back to back. */
        __asm__("" : "=r"(t1_scratch) : "0"(t1_scratch));
        navigation_offset = t1_scratch * 0x10;
        t1_scratch = (s32)local.navigation;
        navigation_entry = (world_help_navigation_entry_t*)((u8*)t1_scratch + navigation_offset);
        cursor_y = navigation_entry->cursor_y;
        t1_scratch = navigation_entry->cursor_x;
        local.cursor_x.w = t1_scratch;
        if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_DEBUG_UNIT_EDITOR) != 0) {
            cursor_y += 8;
        }
        if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_HELP_MENU) != 0) {
            cursor_y += 8;
        }
        input_ptr = g_world_help_controller_state_ptr;
        input = *input_ptr;
        /* The loop notes this emits are load-bearing: they mark `cancel_close` as a
           label outside a loop, which makes reorg predict the branch not-taken and
           fill its delay slot from the fall-through rather than stealing the first
           instruction of the cancel block. See the header. */
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
                value_base = navigation_entry->value_base;
                raw = *value_ptr;
                help_id = value_base + raw;
                raw2 = raw;
                /* Keep the raw help id in its own register: without it the value
                   loads straight into a0 and the base-plus-raw sum drops into the
                   branch delay slot instead of preceding the compare. */
                __asm__("" : "=r"(raw2) : "0"(raw2));
                if (value_base == -0x6800 && g_world_help_billboard.generic_monster != 0) {
                    help_index = raw2 & 0x7FF;
                    help_id = raw2 | 0x7800;
                    if ((help_index == 0x7FF) || (help_index == 0)) {
                        *input_ptr = 0;
                    } else {
                        g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                        world_thread_start(help_thread_id, world_text_character_handling_thread);
                        world_thread_set_parameters(help_thread_id, 0x3B, help_id, 0);
                    }
                } else {
                    if ((navigation_entry->value_base == 0x7800)
                        && ((u32)(*(u16*)navigation_entry->value - 1) >= 0x301U)) {
                        *g_world_help_controller_state_ptr = 0;
                    } else if (((base_reload = navigation_entry->value_base) == 0x6800)
                        && ((u32)(*(u16*)navigation_entry->value - 1) >= 0x101U)) {
                        *g_world_help_controller_state_ptr = 0;
                    } else if ((base_reload == -0x6800) && ((u32)(*(u16*)navigation_entry->value - 1) >= 0x101U)) {
                        *g_world_help_controller_state_ptr = 0;
                    } else {
                        g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                        world_thread_start(help_thread_id, world_text_character_handling_thread);
                        world_thread_set_parameters(help_thread_id, 0x3B, help_id, 0);
                    }
                }
            } else {
                help_id = navigation_entry->value_base;
                g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                world_thread_start(help_thread_id, world_text_character_handling_thread);
                world_thread_set_parameters(help_thread_id, 0x3B, help_id, 0);
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
            navigation_offset = navigation_entry->neighbors[help_id];
            t1_scratch = local.menu_index;
            if (t1_scratch != navigation_offset) {
                g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
            }
            reload_neighbor = navigation_entry->neighbors[help_id];
            local.menu_index = reload_neighbor;
        }
    } else {
        if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_HELP_MENU) != 0) {
            g_world_thread_task_active = 1;
        }
        if (self->task_words[1] == 0) {
            self->task_words[1] = 2;
            self->task_words[0] = 0;
        }
    }

    if (self->task_words[1] != 0) {
        bounce = world_lookup_thread_parameter_threshold_value(1);
    } else {
        bounce = world_lookup_thread_parameter_threshold_value(0);
    }
    self->task_words[0] = (s32)(self->task_words[0] + g_world_event_speed);
    bounce -= 4;
    buffer_index = frame & 1;
    if (navigation_entry->orientation != 0) {
        tex_u = 0xA8;
        v_offset = buffer_index * 0x28;
        v_quad = (POLY_FT4*)(cursor_quad_base + v_offset);
        v_quad->u0 = tex_u;
        v_quad->u1 = tex_u;
        tex_u = 0xB8;
        t1_scratch = 0xB8;
        v_quad->u2 = t1_scratch;
        /* Scheduling boundary: without it the sp+0x60 primitive address is
           hoisted above this store and it lands three slots late. */
        __asm__ volatile("");
        v_shadow = (POLY_FT4*)((u8*)shadow_quads + v_offset);
        shadow_tex_u = 0xC8;
        v_quad->u3 = t1_scratch;
        v_quad->v0 = 0;
        v_quad->v1 = tex_v_max;
        v_quad->v2 = 0;
        v_quad->v3 = tex_v_max;
        v_shadow->u0 = tex_u;
        v_shadow->u1 = tex_u;
        v_shadow->u2 = shadow_tex_u;
        v_shadow->u3 = shadow_tex_u;
        v_shadow->v0 = 0;
        v_shadow->v1 = tex_v_max;
        v_shadow->v2 = 0;
        v_shadow->v3 = tex_v_max;
        t1_scratch = local.cursor_x.h;
        v_top = cursor_y + bounce;
        v_quad->y0 = v_top;
        v_quad->x0 = t1_scratch;
        /* Scheduling boundaries around the bottom-y sum: the second keeps its
           `addiu v0,v1,0x10` in the cursor-x `lw` delay slot instead of after the
           y1 store; without the first, the frame-parity temporaries earlier in
           the function shift from a2/a3 to a3/t0. */
        t1_scratch = local.cursor_x.w;
        __asm__ volatile("");
        v_bottom = v_top + 0x10;
        __asm__ volatile("");
        v_quad->y1 = v_top;
        v_right = t1_scratch + 0x10;
        v_quad->x1 = v_right;
        t1_scratch = local.cursor_x.h;
        v_quad->y2 = v_bottom;
        v_quad->y3 = v_bottom;
        v_shadow_top = v_top + 2;
        v_quad->x3 = v_right;
        v_quad->x2 = t1_scratch;
        t1_scratch = local.cursor_x.w;
        v_top += 0x12;
        v_quad[2].y0 = v_shadow_top;
        bounce = t1_scratch + 2;
        shadow_right = t1_scratch + 0x12;
        /* Scheduling boundary: without it the list scheduler sinks these two
           `addiu` below the shadow quad's y stores. */
        __asm__ volatile("");
        v_quad[2].x0 = bounce;
        v_quad[2].x1 = shadow_right;
        v_quad[2].y1 = v_shadow_top;
        v_quad[2].x2 = bounce;
        v_quad[2].y2 = v_top;
        v_quad[2].x3 = shadow_right;
        v_quad[2].y3 = v_top;
    } else {
        tex_u = 0xA8;
        h_quad_addr = buffer_index * 0x28;
        h_quad_addr += (s32)cursor_quad_base;
        h_quad = (POLY_FT4*)h_quad_addr;
        h_quad->u0 = tex_u;
        tex_u = 0xB8;
        t1_scratch = 0xB8;
        h_tex_u = 0xA8;
        h_quad->u1 = t1_scratch;
        /* Scheduling boundary: keeps the u1 store ahead of the u2 store. */
        __asm__ volatile("");
        h_quad->u2 = h_tex_u;
        shadow_tex_u = 0xC8;
        h_quad->u3 = t1_scratch;
        h_quad->v0 = 0;
        h_quad->v1 = 0;
        h_quad->v2 = tex_v_max;
        h_quad->v3 = tex_v_max;
        h_quad[2].u0 = tex_u;
        h_quad[2].v0 = 0;
        h_quad[2].u1 = shadow_tex_u;
        h_quad[2].v1 = 0;
        h_quad[2].u2 = t1_scratch;
        h_quad[2].v2 = tex_v_max;
        h_quad[2].u3 = shadow_tex_u;
        h_quad[2].v3 = tex_v_max;
        t1_scratch = local.cursor_x.w;
        h_quad->y0 = cursor_y;
        h_quad->y1 = cursor_y;
        h_shadow_left = t1_scratch + bounce;
        h_right = h_shadow_left + 0x10;
        h_quad->x0 = h_shadow_left;
        h_quad->x2 = h_shadow_left;
        h_bottom = cursor_y + 0x10;
        h_quad->y2 = h_bottom;
        h_quad->y3 = h_bottom;
        h_shadow_left = bounce + 2;
        h_shadow_left = t1_scratch + h_shadow_left;
        h_quad->x1 = h_right;
        h_quad->x3 = h_right;
        h_shadow_top = cursor_y + 2;
        shadow_right = h_shadow_left + 0x10;
        h_quad[2].x0 = h_shadow_left;
        h_quad[2].x2 = h_shadow_left;
        h_shadow_bottom = cursor_y + 0x12;
        h_quad[2].y0 = h_shadow_top;
        h_quad[2].x1 = shadow_right;
        h_quad[2].y1 = h_shadow_top;
        h_quad[2].y2 = h_shadow_bottom;
        h_quad[2].x3 = shadow_right;
        h_quad[2].y3 = h_shadow_bottom;
    }
    if (self->task_words[1] != 0) {
        active_offset = buffer_index * 0x28;
        ((POLY_FT4*)(cursor_quad_base + active_offset))->clut = 0x7DFC;
        shadow_quad = (POLY_FT4*)((u8*)shadow_quads + active_offset);
        shadow_clut = 0x7E3C;
    } else {
        idle_offset = buffer_index * 0x28;
        ((POLY_FT4*)(cursor_quad_base + idle_offset))->clut = 0x7D7C;
        shadow_quad = (POLY_FT4*)((u8*)shadow_quads + idle_offset);
        shadow_clut = 0x7DBC;
    }
    shadow_quad->clut = shadow_clut;
    draw_offset = buffer_index * 0x28;
    world_gfx_draw_or_append_gpu_primitive((cursor_quad_base + draw_offset));
    world_gfx_draw_or_append_gpu_primitive(((u8*)shadow_quads + draw_offset));
    world_thread_wait_frames(1);
    frame += 1;
    goto loop_73;
close_and_return:
    *g_world_help_controller_state_ptr = 0;
    world_thread_wait_frames(1);
    world_text_restore_sections_and_stop_thread();
}
