#ifndef MAIN_ITEM_H
#define MAIN_ITEM_H

/* Resident item data, the inventory and the item sort orders. */

#include "fft/data_item.h"
#include "fft/data_job.h"
#include "psx/types.h"

/* item data */
extern item_data_t g_main_item_primary_data[];
extern item_secondary_data_t g_main_item_secondary_data[];
/* &g_main_item_primary_data[0].secondary_data_id viewed as a stride-12 byte
 * array. main_unit_set_equipment_stats reads the secondary id through its own
 * %lo(g_main_item_primary_data+4) address while the type_flags test uses a
 * record pointer; reading both through item_data_t folds them onto one base
 * register and misses the byte-exact match. */
extern u8 g_main_item_secondary_data_ids[];
extern item_attribute_t g_main_item_attributes[];
extern weapon_data_t g_main_item_weapon_data[];
extern item_evade_t g_main_item_shield_data[]; /* unit right/left shield evade slots */
extern armor_data_t g_main_item_helm_armor_secondary_data[];
extern item_evade_t g_main_item_accessory_data[]; /* unit accessory evade slots */
extern u8 g_main_item_ability_item_ids[14];
extern item_poach_t g_main_item_poach_table[JOB_ID_MONSTER_LAST - JOB_ID_MONSTER_FIRST + 1];

item_data_t* main_item_get_data_pointer(s32 item_id);

/* inventory */
extern u8 g_main_item_quantities[ITEM_ID_COUNT];
extern u8 g_main_item_poached_quantities[ITEM_ID_COUNT];
/* The first 0x40 bytes are Move-Find-Item flags; the second half is not yet
 * identified. Both halves are initialized and serialized as one block. */
extern u8 g_main_item_location_flags[0x80];

void main_item_init_new_game_inventory(void);
s32 main_item_get_total_equipment_quantity(u8 item_id, s32 include_battle);

/* sort orders */
extern item_type_order_tables_t g_main_item_type_order_tables;
/* Views of g_main_item_type_order_tables.order_1..6 for the WORLD save-image
 * copies (save_build_image, save_restore_image),
 * whose code the member spelling changes. */
extern s8 g_main_item_type_order_1[8];
extern u8 g_main_item_type_order_2[7];
extern u8 g_main_item_type_order_3[5];
extern u8 g_main_item_type_order_4[5];
extern u8 g_main_item_type_order_5[7];
extern u8 g_main_item_type_order_6[5];
extern u8 g_main_weapon_page_order[0x8a];
extern u8 g_main_helmet_page_order[0x1d];
extern u8 g_main_armor_page_order[0x25];
extern u8 g_main_accessory_page_order[0x21];
extern u8 g_main_item_page_order[0x15];

void main_item_init_order_tables(void);

#endif
