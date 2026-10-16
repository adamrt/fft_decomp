#include "psx/cpu_return_address_abi.h"
#include "psx/libpress.h"
#include "psx/libpress_abi_inline.h"

#define PSYQ_PRESS_DC_CHROMA_BACK_BYTES 1024
#define PSYQ_PRESS_DC_LUMA_BACK_BYTES   1024
#define PSYQ_PRESS_ESCAPE_CODE          0x7c1f
#define PSYQ_PRESS_END_BLOCK            0xfe00
#define PSYQ_PRESS_END_FRAME_V2         0x1ff
#define PSYQ_PRESS_END_FRAME_V3         0x3ff
#define PSYQ_CPU_STATUS_CACHE_SWAP      0x00020000
#define PSYQ_PRESS_PADDING_LAST_INDEX   64

/* open 0x80073bd0–0x80073f10: software VLC expansion and resumable budget. */
s32 DecDCTvlc(u32* frame_data, u32* decode_buffer) {
    /* The handwritten decoder uses only caller-saved GPRs and no C ABI frame. */
    u16* input = (u16*)frame_data;
    u16* output = (u16*)decode_buffer;
    const psyq_press_ac_entry_t* primary;
    const u32* secondary;
    u32 window;
    s32 consumed;
    register u32 scratch __asm__("$8");
    register u32 first __asm__("$9");
    register u32 second __asm__("$10");
    register u32 extra __asm__("$11");
    register s32 quant __asm__("$12");
    register s32 component __asm__("$13");
    register u16* output_end __asm__("$14");
    register s32 predictor_cr __asm__("$15");
    register s32 predictor_cb __asm__("$24");
    register s32 predictor_y __asm__("$25");
    register s32 condition __asm__("$1");
    register u32* limit __asm__("$8");
    register psyq_press_vlc_state_t* state __asm__("$8");
    register const psyq_press_ac_entry_t* ac_entry __asm__("$8");
    register const u32* secondary_entry __asm__("$8");
    register const psyq_press_dc_entry_t* dc_entry __asm__("$8");
    register const psyq_press_dc_entry_t* dc_base __asm__("$1");
    register s32 zero __asm__("$0");
    register s32 other_zero __asm__("$0");
    limit = &g_psyq_press_vlc_limit_halfwords;
    primary = g_psyq_press_ac_primary_table;
    secondary = g_psyq_press_ac_secondary_table;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (input != 0)
        goto fresh_frame;
    first = *limit;
    PSYQ_CPU_SHARED_DELAY_END();
    state = &g_psyq_press_vlc_state;
    __asm__("" : "=r"(state) : "0"(state));
    input = state->input;
    output = state->output;
    window = state->bit_window;
    consumed = state->consumed_bits;
    quant = state->quant_scale_shifted;
    component = state->component;
    predictor_cr = state->predictor_cr;
    predictor_cb = state->predictor_cb;
    predictor_y = state->predictor_y;
    PSYQ_CPU_TRAP_ADD(first, first, first);
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (zero >= 0)
        goto decode_ac;
    PSYQ_CPU_TRAP_ADD(output_end, output, first);
    PSYQ_CPU_SHARED_DELAY_END();
fresh_frame:
    PSYQ_CPU_TRAP_ADD(component, zero, zero);
    PSYQ_CPU_TRAP_ADD(predictor_cr, zero, zero);
    PSYQ_CPU_TRAP_ADD(predictor_cb, zero, zero);
    PSYQ_CPU_TRAP_ADD(predictor_y, zero, zero);
    PSYQ_CPU_TRAP_ADD(first, first, first);
    PSYQ_CPU_TRAP_ADD(output_end, output, first);
    scratch = input[0];
    first = input[1];
    quant = input[2];
    second = input[3];
    window = input[4];
    consumed = input[5];
    PSYQ_PRESS_TRAP_ADDI(second, second, -3);
    quant <<= 10;
    if ((s32)second < 0)
        goto frame_version_ready;
    PSYQ_PRESS_TRAP_CONSTANT(component, 1);
frame_version_ready:
    PSYQ_PRESS_TRAP_ADDI(input, input, 12);
    window <<= 16;
    window |= consumed;
    PSYQ_PRESS_RESET_BIT_COUNT(consumed);
    output[0] = scratch;
    output[1] = first;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
decode_dc:
    scratch = window >> 22;
    if (component == 0)
        goto decode_dc_v2;
    condition = scratch ^ PSYQ_PRESS_END_FRAME_V3;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (condition == 0)
        goto frame_complete;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
    PSYQ_CPU_SHARED_DELAY_END();
    PSYQ_PRESS_TRAP_ADDI(condition, component, -3);
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (condition < 0)
        goto dc_table_ready;
    PSYQ_PRESS_TRAP_ADDI(dc_base, primary, -PSYQ_PRESS_DC_CHROMA_BACK_BYTES);
    PSYQ_CPU_SHARED_DELAY_END();
    PSYQ_PRESS_TRAP_ADDI(dc_base, dc_base, -PSYQ_PRESS_DC_LUMA_BACK_BYTES);
dc_table_ready:
    __asm__("" : "=r"(window) : "0"(window)); /* Block SRL from replacing the trapping ADDI delay. */
    scratch = window >> 24;
    scratch <<= 2;
    PSYQ_CPU_TRAP_ADD(dc_entry, scratch, dc_base);
    first = dc_entry->prefix_bits;
    second = dc_entry->magnitude_bits;
    scratch = zero & other_zero; /* Two zero operands retain the original AND encoding. */
    window <<= first;
    if (second == 0)
        goto dc_magnitude_ready;
    PSYQ_PRESS_TRAP_CONSTANT(condition, 32);
    PSYQ_CPU_TRAP_SUB(condition, condition, second);
    scratch = window >> condition;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if ((s32)window < 0)
        goto dc_positive_magnitude;
    window <<= second;
    PSYQ_CPU_SHARED_DELAY_END();
    PSYQ_PRESS_TRAP_CONSTANT(extra, -1);
    extra >>= condition;
    PSYQ_CPU_TRAP_SUB(scratch, scratch, extra);
dc_positive_magnitude:
    PSYQ_CPU_TRAP_ADD(consumed, consumed, second);
dc_magnitude_ready:
    PSYQ_CPU_TRAP_ADD(consumed, consumed, first);
    condition = consumed & 16;
    consumed &= 15;
    if (condition == 0)
        goto dc_refill_ready;
    first = *input;
    PSYQ_PRESS_TRAP_ADDI(input, input, 2);
    first <<= consumed;
    window |= first;
dc_refill_ready:
    PSYQ_PRESS_TRAP_ADDI(condition, component, -2);
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (condition > 0)
        goto dc_update_y;
    PSYQ_CPU_TRAP_ADD(first, predictor_y, scratch);
    PSYQ_CPU_SHARED_DELAY_END();
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (condition == 0)
        goto dc_update_cb;
    PSYQ_CPU_TRAP_ADD(first, predictor_cb, scratch);
    PSYQ_CPU_SHARED_DELAY_END();
    PSYQ_CPU_TRAP_ADD(first, predictor_cr, scratch);
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (zero >= 0)
        goto dc_predictor_ready;
    PSYQ_CPU_TRAP_ADD(predictor_cr, predictor_cr, scratch);
    PSYQ_CPU_SHARED_DELAY_END();
dc_update_cb:
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (zero >= 0)
        goto dc_predictor_ready;
    PSYQ_CPU_TRAP_ADD(predictor_cb, predictor_cb, scratch);
    PSYQ_CPU_SHARED_DELAY_END();
dc_update_y:
    PSYQ_CPU_TRAP_ADD(predictor_y, predictor_y, scratch);
dc_predictor_ready:
    first <<= 2;
    __asm__("" : "=r"(first) : "0"(first)); /* Preserve the retail 0x3ff mask after SLL. */
    first &= 0x3ff;
    first = quant | first;
    PSYQ_PRESS_TRAP_ADDI(component, component, 1);
    PSYQ_PRESS_TRAP_ADDI(condition, component, -7);
    *output = first;
    if (condition != 0)
        goto check_budget;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (zero >= 0)
        goto check_budget;
    PSYQ_PRESS_TRAP_ADDI(component, component, -6);
    PSYQ_CPU_SHARED_DELAY_END();
decode_dc_v2:
    condition = scratch ^ PSYQ_PRESS_END_FRAME_V2;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (condition == 0)
        goto frame_complete;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
    PSYQ_CPU_SHARED_DELAY_END();
    window <<= 10;
    PSYQ_PRESS_TRAP_ADDI(consumed, consumed, 10);
    condition = consumed & 16;
    consumed &= 15;
    if (condition == 0)
        goto dc_v2_refill_ready;
    first = *input;
    PSYQ_PRESS_TRAP_ADDI(input, input, 2);
    first <<= consumed;
    window |= first;
dc_v2_refill_ready:
    scratch = quant | scratch;
    *output = scratch;
check_budget:
    __asm__("" : "=r"(component));
    condition = (u32)output
        - (u32)output_end; /* Byte subtraction keeps the difference in bytes instead of 16-bit elements. */
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (condition >= 0)
        goto decoder_paused;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
    PSYQ_CPU_SHARED_DELAY_END();
decode_ac:
    __asm__ volatile(""
        : "=r"(output_end), "=r"(output), "=r"(window)
        : "0"(output_end), "1"(output), "2"(window)
        : "memory");
    scratch = window >> 19;
    scratch <<= 3;
    PSYQ_CPU_TRAP_ADD(ac_entry, scratch, primary);
    first = ac_entry->first_code_and_bits;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (first != 0)
        goto ac_primary_ready;
    __asm__("" : "=r"(first) : "0"(first)); /* Hide the known-zero arm so its ANDI is retained. */
    condition = first & 0xff;
    __asm__ volatile("" : : "r"(condition));
    PSYQ_CPU_SHARED_DELAY_END();
    window <<= 8;
    PSYQ_PRESS_TRAP_ADDI(consumed, consumed, 8);
    condition = consumed & 16;
    consumed &= 15;
    if (condition == 0)
        goto ac_secondary_refill_ready;
    scratch = *input;
    PSYQ_PRESS_TRAP_ADDI(input, input, 2);
    scratch <<= consumed;
    window |= scratch;
ac_secondary_refill_ready:
    scratch = window >> 23;
    scratch <<= 2;
    PSYQ_CPU_TRAP_ADD(secondary_entry, scratch, secondary);
    first = *secondary_entry;
    PSYQ_CPU_TRAP_ADD(extra, zero, zero);
    condition = first & 0xff;
    if (zero >= 0)
        goto ac_advance;
ac_primary_ready:
    __asm__("" : "=r"(condition));
    extra = ac_entry->extra_codes;
ac_advance:
    window <<= condition;
    PSYQ_CPU_TRAP_ADD(consumed, consumed, condition);
    condition = consumed & 16;
    consumed &= 15;
    if (condition == 0)
        goto ac_refill_ready;
    scratch = *input;
    PSYQ_PRESS_TRAP_ADDI(input, input, 2);
    scratch <<= consumed;
    window |= scratch;
ac_refill_ready:
    first >>= 16;
    condition = first ^ PSYQ_PRESS_ESCAPE_CODE;
    if (condition == 0)
        goto escaped_code;
    condition = first ^ PSYQ_PRESS_END_BLOCK;
    *output = first;
    if (condition == 0)
        goto decode_dc;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (extra == 0)
        goto decode_ac;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
    PSYQ_CPU_SHARED_DELAY_END();
    second = extra & 0xffff;
    condition = second ^ PSYQ_PRESS_ESCAPE_CODE;
    if (condition == 0)
        goto escaped_code;
    condition = second ^ PSYQ_PRESS_END_BLOCK;
    *output = second;
    if (condition == 0)
        goto decode_dc;
    second = extra >> 16;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (second == 0)
        goto decode_ac;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
    PSYQ_CPU_SHARED_DELAY_END();
    condition = second ^ PSYQ_PRESS_ESCAPE_CODE;
    if (condition == 0)
        goto escaped_code;
    condition = second ^ PSYQ_PRESS_END_BLOCK;
    *output = second;
    if (condition == 0)
        goto decode_dc;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (zero >= 0)
        goto decode_ac;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
    PSYQ_CPU_SHARED_DELAY_END();
escaped_code:
    scratch = window >> 16;
    *output = scratch;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
    scratch = *input;
    PSYQ_PRESS_TRAP_ADDI(input, input, 2);
    window <<= 16;
    scratch <<= consumed;
    window |= scratch;
    if (zero >= 0)
        goto decode_ac;
frame_complete:
    scratch = PSYQ_PRESS_END_BLOCK;
    PSYQ_PRESS_TRAP_CONSTANT(window, PSYQ_PRESS_PADDING_LAST_INDEX);
pad_end_blocks:
    *output = scratch;
    PSYQ_PRESS_TRAP_ADDI(output, output, 2);
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (window != 0)
        goto pad_end_blocks;
    PSYQ_PRESS_TRAP_ADDI(window, window, -1);
    PSYQ_CPU_SHARED_DELAY_END();
    PSYQ_PRESS_STATUS_READ_WAIT(first);
    condition = PSYQ_CPU_STATUS_CACHE_SWAP;
    first |= condition;
    PSYQ_PRESS_STATUS_WRITE(first);
    PSYQ_PRESS_RETURN_COMPLETE(window);
    goto* psyq_cpu_return_address;
decoder_paused:
    PSYQ_CPU_SHARED_DELAY_END();
    state = &g_psyq_press_vlc_state;
    __asm__("" : "=r"(state) : "0"(state));
    state->input = input;
    state->output = output;
    state->bit_window = window;
    state->consumed_bits = consumed;
    state->quant_scale_shifted = quant;
    state->component = component;
    state->predictor_cr = predictor_cr;
    state->predictor_cb = predictor_cb;
    state->predictor_y = predictor_y;
    PSYQ_PRESS_RETURN_PAUSED(window);
    goto* psyq_cpu_return_address;
}
PSYQ_PRESS_RETURN_END();
