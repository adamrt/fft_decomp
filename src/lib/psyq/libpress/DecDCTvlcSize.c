#include "psx/cpu_return_abi.h"
#include "psx/libpress.h"
#include "psx/libpress_abi_inline.h"

/* open 0x80073ba0–0x80073bd0: select VLC budget; return the old 16-bit limit. */
s32 DecDCTvlcSize(s32 words) {
    /* The handwritten body addresses the limit through t0 and tests through at. */
    register u32* limit __asm__("$8");
    register s32 condition __asm__("$1");
    limit = &g_psyq_press_vlc_limit_halfwords;
    PSYQ_CPU_TRAP_ADDI(condition, words, -1);
    /* Architectural v0 receives this typed LW on both paths in the shared delay slot. */
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (condition <= 0)
        goto default_limit;
    psyq_cpu_return_value = *limit;
    PSYQ_CPU_SHARED_DELAY_END();
    condition = words << 1;
    *limit = condition;
    goto* psyq_cpu_return_address;
default_limit:
    /* Keep the default-limit LUI out of the shared branch delay. */
    __asm__ volatile("" : : : "memory");
    condition = PSYQ_PRESS_VLC_DEFAULT_HALFWORDS;
    *limit = condition;
    goto* psyq_cpu_return_address;
}
