/* Run the help menu over the REQUIRE screen.
 *
 * Load-bearing source shapes in the two cursor-quad builders (+0x560..+0x700):
 *   - The upright branch's y origin is one `$3`-pinned s32 updated in place
 *     (`vert_y = s4 + a2; ...; vert_y += 0x12;`). The pin plus the
 *     s32 type is what makes GCC emit `addu $3` directly instead of computing
 *     into a pseudo and copying; as s16 the truncation leaves a live `move`.
 *     Keeping it in `$3` also frees `$7` for the `frame & 1` global allocno,
 *     which is what puts that value in `$7` rather than `$8`.
 *   - The split-cursor branch derives its primitive address with `+=`
 *     (`horiz_addr = buffer_index * 0x28; horiz_addr += (s32)cursor_base;`), which
 *     is what produces `addu v0,v0,s8`. Written as one `cursor_base + product`
 *     expression the operands canonicalise the other way round and no source
 *     ordering or extra local changes that.
 *   - Four empty `__asm__ volatile("")` statements act as scheduling
 *     boundaries; each is commented at its use. They are not general barriers -
 *     an extra one anywhere else in either builder splits a live range and
 *     costs a `move`.
 *
 * The frame holds two separate 80-byte primitive pairs at sp+0x10 and sp+0x60;
 * one `POLY_FT4[4]` changes the frame. Making the 0xB8 colour a plain local
 * lets CSE fold it into the $5 copy and loses the separate `li $9,0xb8`;
 * literal colours fold two constants away.
 */
#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/helpmenu.h"
#include "fft/main_heap.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_thread_start(s32 thread_id, void (*function)(void));

void helpmenu_menu_run_require_help(void) {
    POLY_FT4 cursor_polys[2];
    POLY_FT4 shadow_polys[2];
    struct {
        s32 menu_index;
        s32 pad_b4;
        battle_thread_t* text_thread;
        s32 pad_bc;
        s32 pad_c0;
        s32 pad_c4;
    } local;
    /* Each register pin is load-bearing: unpinning it alone, or with its
       same-register partners, changes the allocation. */
    s16* value_ptr;
    /* One s16 carries the right x and then the shadow y. */
    s16 horiz_coord;
    register s16 vert_right_x __asm__("$5");
    s32 base_help_id;
    s16 first_tpage;
    register s16 vert_bottom_y __asm__("$2");
    register s16 vert_shadow_y __asm__("$2");
    s32 help_base;
    s32 reloaded_base;
    register s32 vert_y __asm__("$3");
    s16 horiz_left_x;
    register s16 horiz_edge __asm__("$3");
    register s32 horiz_shadow_x __asm__("$3");
    /* The 0xA8 and then the 0xC8 texture u share one variable. */
    s32 tex_u_alt;
    register s16 horiz_shadow_bottom_y __asm__("$3");
    register s16 shadow_right_x __asm__("$5");
    s32 cursor_bob;
    /* help_id also carries the navigation direction; the target keeps both in $s0. */
    s32 help_id;
    register s32 cursor_y __asm__("$20");
    register s32 cursor_x __asm__("$21");
    s16 shadow_clut;
    s32* input_ptr;
    s32 buffer_index;
    s32 submit_offset;
    s32 input;
    s32 entry_id;
    s32 vert_offset;
    s32 horiz_addr;
    register s32 active_offset __asm__("$3");
    register s32 idle_offset __asm__("$3");
    register s32 scratch __asm__("$9");
    register battle_thread_t* current_thread __asm__("$9");
    register s32 reload_neighbor __asm__("$3");
    register s32 tex_u __asm__("$5");
    s32 navigation_offset;
    register help_navigation_record_t* nav_base __asm__("$9");
    s32 text_thread_id;
    s32 frame;
    s32 cursor_height;
    u8* cursor_base;
    register POLY_FT4* vert_poly __asm__("$4");
    POLY_FT4* init_poly;
    register POLY_FT4* vert_shadow_poly __asm__("$2");
    POLY_FT4* horiz_poly;
    help_navigation_record_t* navigation_entry;
    POLY_FT4* clut_poly;
    void* banner = g_helpmenu_active_banner;
    void* unit_data = g_helpmenu_active_unit_data;
    void* billboard = &g_helpmenu_billboard;
    void* cursor_tile = g_helpmenu_menu_cursor_tile;
    s32* controller_state;
    s32 task_id;

    /* frame and text_thread_id double as the counter and byte offset of the
       primitive setup loop, as in the target registers. */
    frame = 0;
    text_thread_id = 0;
    {
        POLY_FT4* init_shadow_poly;
        do {
            init_poly = (POLY_FT4*)((u8*)cursor_polys + text_thread_id);
            battle_gfx_init_default_poly_ft4(init_poly);
            SetSemiTrans(init_poly, 0);
            first_tpage = GetTPage(0, 0, 0x3C0, 0x100);
            init_shadow_poly = (POLY_FT4*)((u8*)shadow_polys + text_thread_id);
            init_poly->tpage = first_tpage;
            init_poly->clut = 0x7D7C;
            battle_gfx_init_default_poly_ft4(init_shadow_poly);
            SetSemiTrans(init_shadow_poly, 1);
            init_shadow_poly->tpage = GetTPage(0, 2, 0x3C0, 0x100);
            frame += 1;
            init_shadow_poly->clut = 0x7DBC;
            text_thread_id += 0x28;
        } while (frame < 2);
    }
    battle_copy_bytes((u8*)g_helpmenu_text_saved_pointers, (void*)g_battle_text_section_pointers, 0x80);
    g_battle_text_section_pointers[1] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[1];
    g_battle_text_section_pointers[2] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[2];
    g_battle_text_section_pointers[11] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[11];
    g_battle_text_section_pointers[12] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[12];
    g_battle_text_section_pointers[13] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[13];
    g_battle_text_section_pointers[15] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[15];
    g_battle_text_section_pointers[19] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[19];
    g_battle_text_section_pointers[20] = g_helpmenu_text_data + g_helpmenu_text_section_offsets[20];
    {
        /* g_battle_threads holds the base of the native scheduler contexts;
           the << 0xA index confirms the 0x400 stride of battle_thread_t.
           task_words[0] is the elapsed counter and task_words[1] the display state
           this handler drives (per-task words, see fft/thread.h). */
        battle_thread_t* self_thread;
        self_thread = (battle_thread_t*)((g_battle_current_thread_id << 0xA) + (s32)g_battle_threads);
        battle_action_copy_at_and_cursor_to(banner, unit_data, billboard, cursor_tile);
        if ((g_helpmenu_selected_unit_panel_mode != 0) || (g_battle_post_battle_unit_changes_active != 0)) {
            g_helpmenu_require_navigation[2].destination[2] = 7;
            g_helpmenu_require_navigation[7].destination[3] = 2;
        }
        controller_state = (s32*)battle_script_get_controller_input_pointer(0);
        {
            s16* help_x_ptr = &g_helpmenu_require_navigation[0].x;
            /* Break the constant equivalence so the address stays in a register
               across the store below, matching the target's operand order. */
            __asm__("" : "=r"(help_x_ptr) : "0"(help_x_ptr));
            g_helpmenu_controller_state_ptr = controller_state;
            local.menu_index = 0;
            *help_x_ptr = 0xC4;
        }
        g_helpmenu_require_navigation[2].x = 0xE0;
        g_helpmenu_require_navigation[0].destination[3] = 0;
        g_helpmenu_require_navigation[3].destination[0] = 0;
        task_id = battle_thread_resolve_id(0x10);
        frame = 0;
        cursor_base = (u8*)cursor_polys;
        cursor_height = 0x10;
        text_thread_id = task_id;
        local.text_thread = (battle_thread_t*)((u8*)g_battle_threads + (text_thread_id << 0xA));
        /* Label loop: a for (;;) adds loop depth to every pseudo's reference
           count in flow and the allocation no longer matches. */
    loop_6:
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
            /* The target reloads the menu index and the table base into the same
               scratch register in sequence (lw $9,0xb0(sp); sll; lw $9 again). The
               tied constraints keep each reload in $9. */
            __asm__("" : "=r"(scratch) : "0"(scratch));
            navigation_offset = scratch * 0x10;
            nav_base = g_helpmenu_require_navigation;
            __asm__("" : "=r"(nav_base) : "0"(nav_base));
            /* The explicit scaled byte offset schedules the index scaling ahead
               of the base reload (offset 0x270); `nav_base[menu_index]` schedules
               them together. */
            navigation_entry = (help_navigation_record_t*)((u8*)nav_base + navigation_offset);
            cursor_y = navigation_entry->y;
            cursor_x = navigation_entry->x;
            if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_DEBUG_UNIT_EDITOR) != 0) {
                cursor_y += 8;
            }
            if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_HELP_MENU) != 0) {
                cursor_y += 8;
            }
            input_ptr = g_helpmenu_controller_state_ptr;
            input = *input_ptr;
            if (input & PSX_PAD_CROSS) {
                g_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                goto cleanup;
            }
            if (input & PSX_PAD_CIRCLE) {
                value_ptr = navigation_entry->value;
                if (value_ptr != 0) {
                    s32 raw;
                    s32 raw2;
                    help_base = navigation_entry->help_base;
                    raw = *value_ptr;
                    help_id = help_base + raw;
                    raw2 = raw;
                    /* Keep the raw help id live in its own register: the target
                       masks and re-ors it without recomputing the sum above. */
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
                        } else if ((reloaded_base == -0x6800)
                            && ((u32)(*(u16*)navigation_entry->value - 1) >= 0x101U)) {
                            *g_helpmenu_controller_state_ptr = 0;
                        } else {
                            g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                            battle_thread_start(text_thread_id, battle_text_character_handling_thread);
                            battle_thread_set_parameters(text_thread_id, 0x3B, help_id, 0);
                        }
                    }
                } else {
                    base_help_id = navigation_entry->help_base;
                    g_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
                    battle_thread_start(text_thread_id, battle_text_character_handling_thread);
                    battle_thread_set_parameters(text_thread_id, 0x3B, base_help_id, 0);
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
        self_thread->task_words[0] = self_thread->task_words[0] + g_battle_event_speed;
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
            vert_poly->u3 = scratch;
            vert_poly->v0 = 0;
            vert_poly->v1 = cursor_height;
            vert_poly->v2 = 0;
            vert_poly->v3 = cursor_height;
            vert_shadow_poly->u2 = 0xC8;
            vert_shadow_poly->u3 = 0xC8;
            /* Scheduling boundary: without it the two 0xB8 stores below sink
               ahead of this 0xC8 pair. */
            __asm__ volatile("");
            vert_y = cursor_y + cursor_bob;
            vert_shadow_poly->u0 = tex_u;
            vert_shadow_poly->u1 = tex_u;
            vert_right_x = cursor_x + 0x10;
            vert_shadow_poly->v0 = 0;
            vert_shadow_poly->v1 = cursor_height;
            vert_shadow_poly->v2 = 0;
            vert_shadow_poly->v3 = cursor_height;
            vert_bottom_y = vert_y + 0x10;
            /* cursor_bob now holds the shadow quad's left x. */
            cursor_bob = cursor_x + 2;
            vert_poly->y2 = vert_bottom_y;
            vert_poly->y3 = vert_bottom_y;
            vert_shadow_y = vert_y + 2;
            vert_poly->x1 = vert_right_x;
            vert_poly->x3 = vert_right_x;
            shadow_right_x = cursor_x + 0x12;
            vert_poly->y0 = vert_y;
            vert_poly->y1 = vert_y;
            vert_y += 0x12;
            vert_poly->x0 = cursor_x;
            vert_poly->x2 = cursor_x;
            vert_poly[2].x0 = cursor_bob;
            vert_poly[2].y0 = vert_shadow_y;
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
            /* Scheduling boundary: without it the 0xA8 u2 store is hoisted
               above this 0xB8 u1 store. */
            __asm__ volatile("");
            horiz_poly->u2 = tex_u_alt;
            tex_u_alt = 0xC8;
            horiz_poly->u3 = scratch;
            horiz_poly[2].u1 = tex_u_alt;
            horiz_poly[2].u3 = tex_u_alt;
            /* Scheduling boundary: without it GCC sinks the two 0xB8
               texture-page byte stores past the 0xC8 pair. */
            __asm__ volatile("");
            horiz_left_x = cursor_x + cursor_bob;
            horiz_coord = horiz_left_x + 0x10;
            /* Copy into the $3-pinned temporary rather than pinning the sum
               itself: pinning it ties the addu to the $4 result above and emits
               an extra `move`. The copy gives local_alloc a $3 suggestion and
               coalesces away. */
            horiz_edge = horiz_left_x;
            horiz_poly->x0 = horiz_edge;
            horiz_poly->x2 = horiz_edge;
            horiz_edge = cursor_y + 0x10;
            horiz_poly->y2 = horiz_edge;
            horiz_poly->y3 = horiz_edge;
            /* One value updated in place, with the cursor x added on the left
               so the commutative `addu v1,s5,v1` keeps the target's operand
               order. */
            horiz_shadow_x = cursor_bob + 2;
            horiz_shadow_x = cursor_x + horiz_shadow_x;
            horiz_poly->x1 = horiz_coord;
            horiz_poly->x3 = horiz_coord;
            horiz_coord = cursor_y + 2;
            horiz_poly[2].u0 = tex_u;
            shadow_right_x = horiz_shadow_x + 0x10;
            horiz_poly[2].x0 = horiz_shadow_x;
            horiz_poly[2].x2 = horiz_shadow_x;
            horiz_shadow_bottom_y = cursor_y + 0x12;
            horiz_poly->v0 = 0;
            horiz_poly->v1 = 0;
            horiz_poly->v2 = cursor_height;
            horiz_poly->v3 = cursor_height;
            horiz_poly[2].v0 = 0;
            horiz_poly[2].v1 = 0;
            horiz_poly[2].u2 = scratch;
            horiz_poly[2].v2 = cursor_height;
            horiz_poly[2].v3 = cursor_height;
            horiz_poly->y0 = cursor_y;
            horiz_poly->y1 = cursor_y;
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
        goto loop_6;

    cleanup:
        *g_helpmenu_controller_state_ptr = 0;
        battle_thread_wait_frames(1);
        helpmenu_menu_close();
    }
}
