#include "fft/open.h"

typedef void (*open_script_entry_handler_t)(s16* entry);

extern open_script_entry_handler_t g_open_opcode_handlers[];

s32 open_script_init_and_dispatch_entries(void) {
    open_script_dispatch_state_t* state;
    s32* dispatch_arguments;
    open_script_entry_handler_t* handlers;
    s32 result;
    s16* entry;
    u32 flags;

    open_script_update_timing_and_record_values();
    open_script_update_xa_audio();
    open_gfx_update_opntex_sequence();
    open_script_update_screen_fade();

    state = &g_open_script_state.dispatch;
    result = state->flags | 2;
    state->flags = result;
    if (result & 2) {
        dispatch_arguments = &state->data_base;
        handlers = g_open_opcode_handlers;
        do {
            entry = (s16*)open_script_add_pointer_offset(dispatch_arguments[0], dispatch_arguments[1]);
            handlers[*entry](entry);
            /* The target keeps only the &data_base pointer live across the
             * handler call and reads flags at -0x28 from it; `state->flags`
             * allocates a separate base register. */
            flags = *(u32*)((u8*)dispatch_arguments - 0x28);
            if ((flags & 1) == 0) {
                return 0;
            }
        } while (flags & 2);
    }
    return 1;
}
