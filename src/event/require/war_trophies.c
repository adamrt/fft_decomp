/* Collect the battle's War Trophies and bonus money, show the paged item
 * list, and animate the gil reward after adding it to war funds. */

#include "fft/after_battle.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/reward.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* g_war_trophy_entry_types[]: what a war trophy entry's
 * value in g_war_trophy_entry_values is. */
typedef enum war_trophy_entry {
    WAR_TROPHY_ENTRY_ITEM = 0,       /* an item id */
    WAR_TROPHY_ENTRY_BONUS_MONEY = 1 /* the gil amount */
} war_trophy_entry_e;

/* Double-buffered war-trophy primitives, copied when each page opens. */
typedef struct war_trophy_primitives {
    POLY_FT4 quads[16]; /* 0x000 */
    SPRT banner;        /* 0x280 */
    DR_MODE draw_mode;  /* 0x294 */
} war_trophy_primitives_t;
typedef char war_trophy_primitives_size_must_be_672[sizeof(war_trophy_primitives_t) == 672 ? 1 : -1];

extern war_trophy_primitives_t g_war_trophy_primitives[2];
extern RECT g_war_trophy_clear_rect;
extern RECT g_war_trophy_screen_clip_rect_0;
extern RECT g_war_trophy_screen_clip_rect_1;
extern s32 g_war_trophy_money_digit_targets[6];
extern u32 g_war_trophy_money_digit_positions_q8[6];
extern u32 g_war_trophy_money_digit_speeds_q8[6];
extern u32 g_war_trophy_money_digit_initial_speeds_q8[6];
extern s32 g_war_trophy_money_digit_settling[6];
extern POLY_FT4 g_war_trophy_money_digit_polys[2][6];
extern POLY_FT4 g_war_trophy_money_next_digit_polys[2][6];
extern POLY_FT4 g_war_trophy_money_comma_polys[2];
extern POLY_FT4 g_war_trophy_money_sign_polys[2];
extern RECT g_war_trophy_money_clip_rect_0;
extern RECT g_war_trophy_money_clip_rect_1;

/* Upload rectangle for the current entry's text image. */
extern RECT g_war_trophy_image_rect;

/* Same byte as g_war_trophy_search_result.item_count (+0x15); member access here shortens
 * war_trophy_collect by 4 bytes. */
extern u8 g_war_trophy_item_count;

/* The retail collector does not check these 16-entry capacities. */
static s32 g_war_trophy_entry_count;
static reward_war_result_t g_war_trophy_search_result = { 0 };
static s32 g_war_trophy_entry_types[16] = { 0 };
static s32 g_war_trophy_entry_values[16] = { 0 };
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
    s32 funds;
    s32 item_count;
    const item_data_t* item;
    u8* quantity_ptr;
    u32 pad[4];
    s32* entry_values;
    s32* entry_types;

    g_war_trophy_entry_count = 0;
    reward_find_war_trophies_and_bonus_money(&g_war_trophy_search_result);

    {
        s32 j;
        s32 next;
        entry_values = g_war_trophy_entry_values;
        entry_types = g_war_trophy_entry_types;
        j = g_war_trophy_entry_count;
        entry_values[j] = g_war_trophy_search_result.bonus_money;
        entry_types[j] = WAR_TROPHY_ENTRY_BONUS_MONEY;
        if (entry_values[j] != 0) {
            next = j + 1;
            g_war_trophy_entry_count = next;
        }
    }

    i = 0;
    item_count = g_war_trophy_item_count;
    if (item_count > 0) {
        s32* types_base;
        s32* values_base = entry_values;
        u8* item_count_ptr;
        s32 entry;
        s32 loop_count;
        types_base = entry_types;
        item_count_ptr = &g_war_trophy_search_result.item_count;
        do {
            entry = g_war_trophy_entry_count;
            item_id = g_war_trophy_search_result.item_ids[i];
            i++;
            values_base[entry] = item_id;
            types_base[entry] = WAR_TROPHY_ENTRY_ITEM;
            loop_count = *item_count_ptr;
            g_war_trophy_entry_count = entry + 1;
        } while (i < loop_count);
    }

    i = 0;
    if (g_war_trophy_entry_count > 0) {
        s32* value_ptr = g_war_trophy_entry_values;
        s32* type_ptr = g_war_trophy_entry_types;
        do {
            if (*type_ptr == WAR_TROPHY_ENTRY_ITEM) {
                item_id = *value_ptr;
                quantity = item_total_quantity_get(item_id, 1);
                if (quantity == 99) {
                    item = item_data_get(*value_ptr);
                    price = item->price;
                    price /= 4;
                    funds = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS);
                    script_variable_set(EVENT_SCRIPT_VAR_WAR_FUNDS, funds + price);
                } else {
                    s32 id = *value_ptr;
                    quantity_ptr = &g_item_quantities[id];
                    (*quantity_ptr)++;
                }
            } else {
                funds = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS);
                script_variable_set(EVENT_SCRIPT_VAR_WAR_FUNDS, funds + *value_ptr);
            }
            value_ptr++;
            type_ptr++;
        } while (++i < g_war_trophy_entry_count);
    }
}

/* Screen x for an entry: one column at 0xd0 when the total entry count is
 * below five, otherwise 0x9c for even entry indices and 0x104 for odd ones. */
static s32 war_trophy_get_column_x(s32 entry_index, s32 entry_count) {
    s32 result = 0xd0;

    if (entry_count >= 5) {
        s32 alternate = 0x9c;

        if (entry_index & 1) {
            alternate = 0x104;
        }
        result = alternate;
    }
    return result;
}

/* Screen y for a page-relative entry index, starting at 0x78: 20 pixels
 * per entry in one column or per pair in two. The total entry count selects
 * the layout, so a short final page still uses two columns. */
static s32 war_trophy_get_row_y(s32 page_entry_index, s32 entry_count) {
    if (entry_count < 5) {
        return page_entry_index * 20 + 0x78;
    }
    return (page_entry_index / 2) * 20 + 0x78;
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
void war_trophy_list_thread(void) {
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
        thread_current_exit();
    }

    prims = g_war_trophy_primitives;
    thread = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
    last = 0;
    thread_start(thread, conditions_special_thread);
    thread_parameters_set(thread, CONDITIONS_BANNER_WAR_TROPHIES, 0, 0);

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
                g_menu_window_text_pen.stride = 0x50;
                timer = 0;
                if (g_war_trophy_entry_types[last] == WAR_TROPHY_ENTRY_ITEM) {
                    menu_set_text_origin(0, 0);
                    menu_display_text_entry(
                        g_war_trophy_entry_values[last] + TEXT_ID_ITEM_NAME_BASE, image, &g_menu_window_text_pen);
                } else {
                    menu_set_text_origin(0, 0);
                    menu_draw_large_number_glyphs(g_war_trophy_entry_values[last],
                        text_decimal_digits_count(g_war_trophy_entry_values[last]), image, &g_menu_window_text_pen);
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
                        + text_decimal_digits_count(g_war_trophy_entry_values[i]) * 7 + 0x12;
                    quad->y0 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 4;
                    quad->x1 = war_trophy_get_column_x(i, g_war_trophy_entry_count)
                        + text_decimal_digits_count(g_war_trophy_entry_values[i]) * 7 + 0x24;
                    quad->y1 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 4;
                    quad->x2 = war_trophy_get_column_x(i, g_war_trophy_entry_count)
                        + text_decimal_digits_count(g_war_trophy_entry_values[i]) * 7 + 0x12;
                    quad->y2 = war_trophy_get_row_y(i - start, g_war_trophy_entry_count) + 0xc;
                    quad->x3 = war_trophy_get_column_x(i, g_war_trophy_entry_count)
                        + text_decimal_digits_count(g_war_trophy_entry_values[i]) * 7 + 0x24;
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

    thread_parameters_set(thread, 0, 0, 1);
    thread_inactive_wait(thread);
    thread_current_exit();
}

#define X0 (0x108 - j * 13 - (5 - digits) * 13 / 2)

/*
 * Post-battle gil ticker: adds the war-result money to the war funds (EVENT_SCRIPT_VAR_WAR_FUNDS),
 * starts the display-condition thread, splits the amount into up to six
 * digits and rolls their odometer strips through 32-pixel vertical slots.
 * Positions and speeds are q8 pixels; each 13-pixel-wide digit slows by 0x44
 * per step once its target comes round, before the display holds for 60 frames
 * and fades out. The initial-speed table leaves the sixth strip stationary.
 *
 * The polygon pointers are formed as `(s32)array + offset` byte offsets, which
 * keeps the target's separate base and offset; `&g_war_trophy_money_digit_polys[buffer][j]` is reduced
 * to a single pointer induction variable. `pass` doubles as the fade level,
 * which the target's register allocation reflects.
 */
void war_trophy_run_gil_ticker(void) {
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
    script_variable_set(EVENT_SCRIPT_VAR_WAR_FUNDS,
        script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS) + g_war_trophy_search_result.level_sum_money);
    money = g_war_trophy_search_result.level_sum_money;

    thread_id = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_id, conditions_special_thread);
    thread_parameters_set(thread_id, CONDITIONS_BANNER_BONUS_MONEY, 0, 0);
    g_menu_input_controller = script_get_controller_input_pointer(0);

    /* `i = 0` here (not before the script calls) lets sched1 keep
     * `li a0,0x2c` above the money load. */
    for (i = 0; i < 6; i++) {
        g_war_trophy_money_digit_speeds_q8[i] = g_war_trophy_money_digit_initial_speeds_q8[i];
        g_war_trophy_money_digit_settling[i] = 0;
    }

    digits = text_decimal_digits_count(money);
    /* There are six digit strips; a longer amount is reported and shows as 0. */
    if (digits >= 7) {
        system_error_report(7, 0);
        money = 0;
        digits = 1;
    }

    /* Each strip starts on its target digit; one digit is 1 << 13 position units. */
    j = money;
    for (i = 0; i < digits; i++) {
        g_war_trophy_money_digit_targets[i] = j % 10;
        j /= 10;
        g_war_trophy_money_digit_positions_q8[i] = g_war_trophy_money_digit_targets[i] << 13;
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

                g_war_trophy_money_digit_positions_q8[j] += g_war_trophy_money_digit_speeds_q8[j];
                off = j * 0x28 + buffer * 0xf0;
                x = g_war_trophy_money_digit_polys[buffer][j].x0;
                pd = (POLY_FT4*)((s32)g_war_trophy_money_digit_polys + off);
                setXY4(pd, x, ((g_war_trophy_money_digit_positions_q8[j] >> 8) & 0x1f) + 0x80, x + 13,
                    ((g_war_trophy_money_digit_positions_q8[j] >> 8) & 0x1f) + 0x80, x,
                    ((g_war_trophy_money_digit_positions_q8[j] >> 8) & 0x1f) + 0x97, x + 13,
                    ((g_war_trophy_money_digit_positions_q8[j] >> 8) & 0x1f) + 0x97);
                setUV4(pd, (g_war_trophy_money_digit_positions_q8[j] >> 13) % 10 * 13, 0x32,
                    (g_war_trophy_money_digit_positions_q8[j] >> 13) % 10 * 13 + 13, 0x32,
                    (g_war_trophy_money_digit_positions_q8[j] >> 13) % 10 * 13, 0x49,
                    (g_war_trophy_money_digit_positions_q8[j] >> 13) % 10 * 13 + 13, 0x49);
                x = g_war_trophy_money_next_digit_polys[buffer][j].x0;
                off = j * 0x28 + buffer * 0xf0;
                pn = (POLY_FT4*)((s32)g_war_trophy_money_next_digit_polys + off);
                setXY4(pn, x, g_war_trophy_money_digit_polys[buffer][j].y0 - 0x20, x + 13,
                    g_war_trophy_money_digit_polys[buffer][j].y0 - 0x20, x,
                    g_war_trophy_money_digit_polys[buffer][j].y0 - 9, x + 13,
                    g_war_trophy_money_digit_polys[buffer][j].y0 - 9);
                setUV4(pn, ((g_war_trophy_money_digit_positions_q8[j] >> 13) + 1) % 10 * 13, 0x32,
                    ((g_war_trophy_money_digit_positions_q8[j] >> 13) + 1) % 10 * 13 + 13, 0x32,
                    ((g_war_trophy_money_digit_positions_q8[j] >> 13) + 1) % 10 * 13, 0x49,
                    ((g_war_trophy_money_digit_positions_q8[j] >> 13) + 1) % 10 * 13 + 13, 0x49);
                /* Once its turn comes (higher digits later), a digit slows from six short of its
                 * target and stops on it when its cell is exactly aligned. */
                if (j * 32 + (digits - 1) * 16 < tick) {
                    if (((g_war_trophy_money_digit_positions_q8[j] >> 13) + 6) % 10
                        == g_war_trophy_money_digit_targets[j]) {
                        g_war_trophy_money_digit_settling[j] = 1;
                    }
                    if (g_war_trophy_money_digit_settling[j] == 1) {
                        if (g_war_trophy_money_digit_speeds_q8[j] > 0x100) {
                            g_war_trophy_money_digit_speeds_q8[j] -= 0x44;
                        } else {
                            g_war_trophy_money_digit_speeds_q8[j] = 0x100;
                            if ((g_war_trophy_money_digit_positions_q8[j] >> 13) % 10
                                    == g_war_trophy_money_digit_targets[j]
                                && ((g_war_trophy_money_digit_positions_q8[j] >> 8) & 0x1f) == 0) {
                                g_war_trophy_money_digit_speeds_q8[j] = 0;
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
    thread_parameters_set(thread_id, 0, 0, 1);
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
    thread_inactive_wait(thread_id);
    thread_current_exit();
}
