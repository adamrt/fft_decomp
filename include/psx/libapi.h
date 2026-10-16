#ifndef PSX_LIBAPI_H
#define PSX_LIBAPI_H

#include "psx/kernel.h"
#include "psx/libc.h"
#include "psx/types.h"

/* files */
s32 close(s32 descriptor);
/* BIOS error code for this file descriptor. */
s32 _get_error(s32 descriptor);
s32 open(const char* path, u32 mode);
s32 read(s32 descriptor, void* destination, s32 size);
s32 lseek(s32 descriptor, s32 offset, s32 origin);
s32 write(s32 descriptor, const void* source, s32 size);
s32 erase(const char* path);
s32 format(const char* path);
DIRENTRY* firstfile(char* pattern, DIRENTRY* entry);
DIRENTRY* nextfile(DIRENTRY* entry);

/* events */
typedef s32 (*psyq_api_event_callback_t)(void);

s32 CloseEvent(s32 event);
s32 OpenEvent(u32 desc, s32 spec, s32 mode, psyq_api_event_callback_t callback);
s32 WaitEvent(s32 event);
s32 TestEvent(s32 event);
s32 EnableEvent(s32 event);
s32 DisableEvent(s32 event);
void DeliverEvent(u32 desc, u32 spec);

/* interrupts */
void EnterCriticalSection(void);
void ExitCriticalSection(void);
void ReturnFromException(void);
void ResetEntryInt(void);
void HookEntryInt(jmp_buf jump_buffer);
s32 ChangeClearRCnt(s32 counter, s32 mode);
void ChangeClearPAD(s32 val);

/* root counters */
s32 SetRCnt(u32 spec, u16 target, s32 mode);
s32 GetRCnt(u32 spec);
s32 StartRCnt(u32 spec);
s32 StopRCnt(u32 spec);
s32 ResetRCnt(u32 spec);

/* card */
void InitCARD(s32 val);
s32 StartCARD(void);
s32 StopCARD(void);
s32 _card_clear(s32 port);
void _bu_init(void);
s32 _card_auto(s32 val);
s32 _card_info(s32 port);
s32 _card_load(s32 port);
s32 _card_status(s32 slot);
s32 _card_write(s32 port, s32 sector, const u8* data);
void _new_card(void);
void InitCARD2(s32 pad_enable);
s32 StartCARD2(void);
s32 StopCARD2(void);

/* pad */
void StopPAD(void);
s32 PAD_init2(s32 type, u32* buttons);

/* system */
void InitHeap(u32* arena, u32 size);
void FlushCache(void);
void SetMem(s32 mode);
void _96_remove(void);
void GPU_cw(u32 command);

/* root counter registers */
typedef struct {
    u16 count;
    u16 _padding_02; /* Four-byte register spacing. */
    u16 mode;
    u16 _padding_06; /* Four-byte register spacing. */
    u16 target;
    u16 _padding_0a; /* Four-byte register spacing. */
    u32 _padding_0c; /* Sixteen-byte channel spacing. */
} psyq_api_root_counter_t;

typedef struct {
    u32 status;
    u32 mask;
} psyq_api_interrupt_controller_t;

enum {
    PSYQ_RCNT_SPEC_MASK = 0xffff,
    PSYQ_RCNT_HARDWARE_COUNT = 3,
};

/* The mode argument of SetRCnt. */
typedef enum {
    PSYQ_RCNT_API_SYSTEM_CLOCK = 0x1,
    PSYQ_RCNT_API_SYNC_ENABLE = 0x10,
    PSYQ_RCNT_API_IRQ_ENABLE = 0x1000,
} psyq_root_counter_api_mode_e;

/* The 16-bit hardware mode SetRCnt builds from it. */
typedef enum {
    PSYQ_RCNT_SYNC_ENABLE = 0x1,
    PSYQ_RCNT_RESET_AT_TARGET = 0x8,
    PSYQ_RCNT_IRQ_ON_TARGET = 0x10,
    PSYQ_RCNT_IRQ_REPEAT = 0x40,
    PSYQ_RCNT_CLOCK_ALTERNATE = 0x100,
    PSYQ_RCNT_CLOCK_DIV8 = 0x200,
    PSYQ_RCNT_BASE_MODE = PSYQ_RCNT_RESET_AT_TARGET | PSYQ_RCNT_IRQ_REPEAT
} psyq_root_counter_control_e;

extern volatile psyq_api_root_counter_t* g_psyq_api_root_counters;
extern volatile psyq_api_interrupt_controller_t* g_psyq_api_interrupt_controller;
extern u32 g_psyq_api_root_counter_irq_masks[4];

/* linked BIOS interfaces */
void PAD_dr(void);
/* This link alias keeps the native call; standard memcpy expands into an inline loop. */
void* psyq_api_memcpy(void* destination, const void* source, unsigned int size);

#endif
