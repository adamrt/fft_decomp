#ifndef PSX_CRT_H
#define PSX_CRT_H

#include "psx/types.h"

typedef void (*psyq_crt_handler_t)(void);
typedef struct {
    u32 _padding_00; /* rounds the 12 saved bytes up to a 16-byte frame */
    u32 saved_s0;
    u32 saved_s1;
    void* saved_ra;
} psyq_crt_dispatch_frame_t;

extern s32 g_psyq_crt_constructors_ran;
extern psyq_crt_handler_t g_psyq_crt_constructor_table[];
extern psyq_crt_handler_t g_psyq_crt_destructor_table[];
/* These are absolute linker values, not memory to be read. Both counts are zero. */
extern u8 g_psyq_crt_constructor_count[];
extern u8 g_psyq_crt_destructor_count[];

extern u32 g_psyq_crt_saved_ra;
extern u8 g_psyq_crt_bss_end[];
extern void* g_psyq_crt_heap_base;
extern u32 g_psyq_crt_heap_size;
extern u32 _ramsize;
extern u32 _stacksize;

/* Supplied by the executable linked with this startup runtime. */
void main(void);

void __SN_ENTRY_POINT(void);
void __main(void);
void __do_global_dtors(void);

#endif
