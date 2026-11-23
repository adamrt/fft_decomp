#ifndef OFFICE_H
#define OFFICE_H

/* WORLD's Soldier Office, a mode of the shop screen (shop.h): hiring recruits, naming them and paying the fee, and
 * renaming units. */

/* menus */
void office_update_menu(void);
void office_hire_update_preview(void);
void office_hire_update_confirm(void);
void office_hire_update_name(void);
void office_hire_update_fee(void);
void office_reopen_menu(void);
void office_rename_update_unit(void);
void office_rename_update_name(void);
void office_draw_fee(void);
void office_draw_unit_count(void);

#endif
