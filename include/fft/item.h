#ifndef ITEM_H
#define ITEM_H

/* Item and equipment logic shared by EQUIP and WORLD: categories, stat previews and inventory counts. */

#include "fft/data.h"
#include "fft/menu.h"
#include "psx/types.h"

/* unified */
void item_sum_equipment_stat_details(menu_status_panel_slot_storage_t* total, const u16* equipment);
void item_collect_stat_deltas_variant(
    s16 raw_item, menu_unit_status_record_t* summary, menu_status_panel_slot_storage_t* detail, s32 alternate);
item_menu_category_e item_get_menu_category(s16 item_id);
s32 item_get_ranking_value(s32 item_id);
void item_calculate_equipment_swap_stat_delta(menu_status_panel_slot_storage_t* detail_total,
    menu_unit_status_record_t* summary_total, const u16* from_items, const u16* to_items);
void item_rebuild_category_list(s32 category);
s32 item_get_total_count_variant(s16 item_id);
s32 item_is_in_category_list(s32 item_id, s32 category_index);
void item_prepend_to_category_list(s32 item_id, s32 category_index);
void item_remove_from_category_list(s32 item_id, s32 category_index);
extern s16 g_item_list_entries[];
s32 item_get_equipped_count_variant(s32 item_entry);
s32 item_adjust_inventory_count(s16 item_entry, s32 delta);
s32 item_sort_list_by_category_order(s32 category_index, s16* list);
extern u8* g_item_category_lists[];
extern s8* g_item_type_order_lists[];
void item_store_category_list(s32 category_index, const s16* source);
extern menu_icon_source_t g_item_graphic_descriptor;
s32 item_get_type(s32 item_id);

#endif
