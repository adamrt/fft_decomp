/* Text, deep dungeon, SetVar2 and DrawPath requests. */

#include "fft/wldcore.h"

/* Stage the text ID consumed by the deferred world-script text request. */
void wldcore_opcode_map_request_text(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_TEXT;
}

/* Stage the three operands for a deep-dungeon entry request.
 *
 * Each operand advances the script IP before the request becomes ready.
 */
void wldcore_opcode_map_request_deep_dungeon_entry(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[2] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY;
}

/* Stage the two operands for the deferred SetVar2 request.
 *
 * This opcode only prepares the request; a later dispatcher performs it.
 */
void wldcore_opcode_map_request_set_var_2(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_SET_VAR_2;
}

/* Stage the two operands for a deferred DrawPath request.
 *
 * The request flag is published after both operands and IP updates.
 */
void wldcore_opcode_map_request_draw_path(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_DRAW_PATH;
}
