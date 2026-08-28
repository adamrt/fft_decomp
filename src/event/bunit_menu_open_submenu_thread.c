#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct {
    void (*func)(void); /* 0x00 */
    u16 unknown_04;     /* 0x04 */
    u16 text_thread_id; /* 0x06: world_menu_text_binding_t.text_thread_id */
} bunit_event_def_t;

typedef struct {
    u8 pad_00[0x1C];        /* 0x00 */
    u16 text_id;            /* 0x1C: menu struct text entry */
    u8 pad_1e[0xE];         /* 0x1E */
    u16 header_id;          /* 0x2C: menu struct header */
    u8 pad_2e[2];           /* 0x2E */
    bunit_event_def_t* def; /* 0x30 */
} bunit_event_obj_t;

void bunit_menu_open_submenu_thread(void) {
    bunit_event_obj_t* obj;
    bunit_event_def_t* def;
    u32* ctl;
    u16 id;
    u16 arg;
    u16 unknown_2c;
    u16 unknown_1c;

    obj = (bunit_event_obj_t*)g_battle_threads[g_battle_current_thread_id].function_parameter_1;
    obj->def->func();
    ctl = battle_script_get_controller_input_pointer(0);
    unknown_1c = obj->text_id;
    def = obj->def;
    unknown_2c = obj->header_id;
    g_bunit_input_controller = ctl;
    id = def->text_thread_id;
    arg = def->unknown_04;
    *ctl = PSX_PAD_CIRCLE;
    battle_menu_handle_action(obj, 0);
    battle_thread_start(id, battle_text_character_handling_thread);
    battle_thread_set_parameters_4(id, unknown_2c, unknown_1c, arg, arg);
    battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
    battle_thread_wait_until_inactive(id);
    battle_thread_exit_current();
}
