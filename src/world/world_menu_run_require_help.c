/* WORLD copy of the EVENT HELPMENU require-help menu
 * (helpmenu_menu_run_require_help), byte-identical apart from addresses.
 * The register pins and scheduling boundaries are the HELPMENU twin's; see
 * src/event/helpmenu_menu_run_require_help.c for the load-bearing shapes. */
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Moves the bouncing help cursor over the require-help navigation table and
 * opens the confirmed entry's help text in a text thread until cancelled. */
void world_menu_run_require_help(void) {
    POLY_FT4 cursor_quads[2];
    POLY_FT4 shadow_quads[2];
    struct {
        s32 menu_index;
        s32 pad_b4;
        native_thread_t* help_thread;
        s32 pad_bc;
        s32 pad_c0;
        s32 pad_c4;
    } local;
    s16* value_ptr;
    register s16 h_right __asm__("$4");
    register s16 h_shadow_top __asm__("$4");
    register s16 v_right __asm__("$5");
    s32 fixed_help_id;
    s16 first_tpage;
    register s16 v_bottom __asm__("$2");
    register s16 v_shadow_top __asm__("$2");
    s32 value_base;
    s32 base_reload;
    register s32 v_top __asm__("$3"); /* later the shadow bottom */
    s16 h_left;
    register s16 h_edge __asm__("$3"); /* left x, then bottom y */
    register s32 h_shadow_left __asm__("$3");
    s32 h_tex_u;
    s32 shadow_tex_u;
    register s16 h_shadow_bottom __asm__("$3");
    register s16 shadow_right __asm__("$5");
    s32 bounce; /* cursor bounce; then the vertical shadow left x */
    s32 help_id;
    register s32 cursor_y __asm__("$20");
    register s32 cursor_x __asm__("$21");
    s16 shadow_clut;
    s32* input_ptr;
    s32 buffer_index;
    s32 draw_offset;
    s32 input;
    s32 help_index;
    s32 v_offset;
    s32 h_quad_addr;
    register s32 active_offset __asm__("$3");
    register s32 idle_offset __asm__("$3");
    register s32 t1_scratch __asm__("$9"); /* menu index or texture u reload */
    register native_thread_t* current_thread __asm__("$9");
    register s32 reload_neighbor __asm__("$3");
    register s32 tex_u __asm__("$5");
    s32 navigation_offset;
    register world_help_navigation_entry_t* nav_base __asm__("$9");
    register s32 direction __asm__("$16");
    s32 help_thread_id; /* quad byte offset during setup */
    s32 frame;
    s32 tex_v_max;
    u8* cursor_quad_base;
    register POLY_FT4* v_quad __asm__("$4");
    POLY_FT4* init_cursor;
    register POLY_FT4* v_shadow __asm__("$2");
    POLY_FT4* h_quad;
    world_help_navigation_entry_t* navigation_entry;
    POLY_FT4* shadow_quad;
    void* setup_banner = g_world_help_active_banner;
    void* setup_unit_data = g_world_help_active_unit_data;
    void* setup_billboard = &g_world_help_billboard;
    void* setup_cursor_tile = g_world_help_menu_cursor_tile;
    s32* controller_state;
    s32 task_id;

    frame = 0;
    help_thread_id = 0;
    {
        POLY_FT4* init_shadow;
        do {
            init_cursor = (POLY_FT4*)((u8*)cursor_quads + help_thread_id);
            world_menu_init_quad(init_cursor);
            SetSemiTrans(init_cursor, 0);
            first_tpage = GetTPage(0, 0, 0x3C0, 0x100);
            init_shadow = (POLY_FT4*)((u8*)shadow_quads + help_thread_id);
            init_cursor->tpage = first_tpage;
            init_cursor->clut = 0x7D7C;
            world_menu_init_quad(init_shadow);
            SetSemiTrans(init_shadow, 1);
            init_shadow->tpage = GetTPage(0, 2, 0x3C0, 0x100);
            frame += 1;
            init_shadow->clut = 0x7DBC;
            help_thread_id += 0x28;
        } while (frame < 2);
    }
    world_script_copy_bytes(
        g_world_text_section_pointer_backup, g_world_text_section_pointers, WORLD_TEXT_SECTION_TABLE_BYTES);
    g_world_text_section_pointers[1] = (u8*)(g_world_help_text_section_offsets[1] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[2] = (u8*)(g_world_help_text_section_offsets[2] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[11] = (u8*)(g_world_help_text_section_offsets[11] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[12] = (u8*)(g_world_help_text_section_offsets[12] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[13] = (u8*)(g_world_help_text_section_offsets[13] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[15] = (u8*)(g_world_help_text_section_offsets[15] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[19] = (u8*)(g_world_help_text_section_offsets[19] + (s32)g_world_help_data_base);
    g_world_text_section_pointers[20] = (u8*)(g_world_help_text_section_offsets[20] + (s32)g_world_help_data_base);
    {
        /* g_world_threads holds the base of the native scheduler contexts;
           the << 0xA index confirms the 0x400 stride of native_thread_t.
           task_words[0] is the elapsed counter and task_words[1] the display state
           this handler drives (per-task words, see fft/thread.h). */
        native_thread_t* self;
        self = (native_thread_t*)((g_world_thread_current_id * NATIVE_THREAD_STRIDE) + (s32)g_world_threads);
        world_gfx_copy_screen_setup_out(setup_banner, setup_unit_data, setup_billboard, setup_cursor_tile);
        if ((g_world_selected_unit_stat_summary.team_state != 0) || (g_world_unit_join_screen_active != 0)) {
            g_world_help_require_navigation[2].neighbors[2] = 7;
            g_world_help_require_navigation[7].neighbors[3] = 2;
        }
        controller_state = world_input_get_menu_controller(0);
        {
            s16* help_x_ptr = &g_world_help_require_navigation[0].cursor_x;
            /* Break the constant equivalence so the address stays in a register
               across the store below, matching the target's operand order. */
            __asm__("" : "=r"(help_x_ptr) : "0"(help_x_ptr));
            g_world_help_controller_state_ptr = controller_state;
            local.menu_index = 0;
            *help_x_ptr = 0xC4;
        }
        g_world_help_require_navigation[2].cursor_x = 0xE0;
        g_world_help_require_navigation[0].neighbors[3] = 0;
        g_world_help_require_navigation[3].neighbors[0] = 0;
        task_id = world_thread_resolve_id(0x10);
        frame = 0;
        cursor_quad_base = (u8*)cursor_quads;
        tex_v_max = 0x10;
        help_thread_id = task_id;
        local.help_thread = (native_thread_t*)((u8*)g_world_threads + (help_thread_id * NATIVE_THREAD_STRIDE));
        /* A label loop: as a C loop the loop notes re-weight register
           allocation and hoist the quad addresses out of the frame loop. */
    loop_6:
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
            /* The target reloads the menu index and the table base into the same
               scratch register in sequence (lw $9,0xb0(sp); sll; lw $9 again). The
               tied constraints keep each reload in $9. */
            __asm__("" : "=r"(t1_scratch) : "0"(t1_scratch));
            navigation_offset = t1_scratch * 0x10;
            nav_base = g_world_help_require_navigation;
            __asm__("" : "=r"(nav_base) : "0"(nav_base));
            /* The scaled byte offset stays explicit: writing this as
               `nav_base[menu_index]` breaks the match at function offset 0x270,
               because the array form
               lets the index scaling be scheduled together with the base reload
               instead of ahead of it. */
            navigation_entry = (world_help_navigation_entry_t*)((u8*)nav_base + navigation_offset);
            cursor_y = navigation_entry->cursor_y;
            cursor_x = navigation_entry->cursor_x;
            if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_DEBUG_UNIT_EDITOR) != 0) {
                cursor_y += 8;
            }
            if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_HELP_MENU) != 0) {
                cursor_y += 8;
            }
            input_ptr = g_world_help_controller_state_ptr;
            input = *input_ptr;
            if (input & PSX_PAD_CROSS) {
                g_world_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                goto cleanup;
            }
            if (input & PSX_PAD_CIRCLE) {
                value_ptr = navigation_entry->value;
                if (value_ptr != 0) {
                    s32 raw;
                    s32 raw2;
                    value_base = navigation_entry->value_base;
                    raw = *value_ptr;
                    help_id = value_base + raw;
                    raw2 = raw;
                    /* Keep the raw help id live in its own register: the target
                       masks and re-ors it without recomputing the sum above. */
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
                    fixed_help_id = navigation_entry->value_base;
                    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    world_thread_start(help_thread_id, world_text_character_handling_thread);
                    world_thread_set_parameters(help_thread_id, 0x3B, fixed_help_id, 0);
                }
            } else if (input & PSX_PAD_DPAD_MASK) {
                direction = 0;
                if (!(input & PSX_PAD_UP)) {
                    if (input & PSX_PAD_DOWN) {
                        direction = 1;
                    } else {
                        direction = 3;
                        if (input & PSX_PAD_RIGHT) {
                            direction = 2;
                        }
                    }
                }
                navigation_offset = navigation_entry->neighbors[direction];
                t1_scratch = local.menu_index;
                if (t1_scratch != navigation_offset) {
                    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                }
                reload_neighbor = navigation_entry->neighbors[direction];
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
        self->task_words[0] = self->task_words[0] + g_world_event_speed;
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
            v_quad->u3 = t1_scratch;
            v_quad->v0 = 0;
            v_quad->v1 = tex_v_max;
            v_quad->v2 = 0;
            v_quad->v3 = tex_v_max;
            v_shadow->u2 = 0xC8;
            v_shadow->u3 = 0xC8;
            /* Scheduling boundary: without it the two 0xB8 stores below sink
               ahead of this 0xC8 pair. */
            __asm__ volatile("");
            v_top = cursor_y + bounce;
            v_shadow->u0 = tex_u;
            v_shadow->u1 = tex_u;
            v_right = cursor_x + 0x10;
            v_shadow->v0 = 0;
            v_shadow->v1 = tex_v_max;
            v_shadow->v2 = 0;
            v_shadow->v3 = tex_v_max;
            v_bottom = v_top + 0x10;
            bounce = cursor_x + 2;
            v_quad->y2 = v_bottom;
            v_quad->y3 = v_bottom;
            v_shadow_top = v_top + 2;
            v_quad->x1 = v_right;
            v_quad->x3 = v_right;
            shadow_right = cursor_x + 0x12;
            v_quad->y0 = v_top;
            v_quad->y1 = v_top;
            v_top += 0x12;
            v_quad->x0 = cursor_x;
            v_quad->x2 = cursor_x;
            v_quad[2].x0 = bounce;
            v_quad[2].y0 = v_shadow_top;
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
            /* Scheduling boundary: without it the 0xA8 u2 store is hoisted
               above this 0xB8 u1 store. */
            __asm__ volatile("");
            h_quad->u2 = h_tex_u;
            shadow_tex_u = 0xC8;
            h_quad->u3 = t1_scratch;
            h_quad[2].u1 = shadow_tex_u;
            h_quad[2].u3 = shadow_tex_u;
            /* Scheduling boundary: without it GCC sinks the two 0xB8
               texture-page byte stores past the 0xC8 pair. */
            __asm__ volatile("");
            h_left = cursor_x + bounce;
            h_right = h_left + 0x10;
            /* Copy into the $3-pinned temporary rather than pinning the sum
               itself: pinning it ties the addu to the $4 result above and emits
               an extra `move`. The copy gives local_alloc a $3 suggestion and
               coalesces away. */
            h_edge = h_left;
            h_quad->x0 = h_edge;
            h_quad->x2 = h_edge;
            h_edge = cursor_y + 0x10;
            h_quad->y2 = h_edge;
            h_quad->y3 = h_edge;
            /* One value updated in place, with the cursor x added on the left
               so the commutative `addu v1,s5,v1` keeps the target's operand
               order. */
            h_shadow_left = bounce + 2;
            h_shadow_left = cursor_x + h_shadow_left;
            h_quad->x1 = h_right;
            h_quad->x3 = h_right;
            h_shadow_top = cursor_y + 2;
            h_quad[2].u0 = tex_u;
            shadow_right = h_shadow_left + 0x10;
            h_quad[2].x0 = h_shadow_left;
            h_quad[2].x2 = h_shadow_left;
            h_shadow_bottom = cursor_y + 0x12;
            h_quad->v0 = 0;
            h_quad->v1 = 0;
            h_quad->v2 = tex_v_max;
            h_quad->v3 = tex_v_max;
            h_quad[2].v0 = 0;
            h_quad[2].v1 = 0;
            h_quad[2].u2 = t1_scratch;
            h_quad[2].v2 = tex_v_max;
            h_quad[2].v3 = tex_v_max;
            h_quad->y0 = cursor_y;
            h_quad->y1 = cursor_y;
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
        world_gfx_draw_or_append_gpu_primitive(cursor_quad_base + draw_offset);
        world_gfx_draw_or_append_gpu_primitive((u8*)shadow_quads + draw_offset);
        world_thread_wait_frames(1);
        frame += 1;
        goto loop_6;

    cleanup:
        *g_world_help_controller_state_ptr = 0;
        world_thread_wait_frames(1);
        world_text_restore_sections_and_stop_thread();
    }
}
