#include "fft/battle_effect.h"
#include "fft/main.h"

effect_script_result_e effect_code_script_2e_invalid(effect_record_t* unused_record) {
    main_system_report_stub(1, 0x22);
    /* Retail leaves the dispatch result undefined after an invalid opcode. */
}
