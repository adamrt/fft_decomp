#ifndef FFT_THREAD_H
#define FFT_THREAD_H

#include "psx/types.h"

/* Native cooperative scheduler context, not scenario bytecode or its operand
 * buffer. BATTLE 0x8014c8a0 and WORLD 0x800ffd70 initialize the same layout;
 * both index slots with thread_id << 10 and start SP/FP at slot + 0x3f0.
 * The parameter stores at BATTLE 0x8014ca38 and WORLD's corresponding
 * three/four-parameter helpers establish full 32-bit payload words.
 * The seven task words at 0x50 are zeroed by battle_thread_start /
 * world_thread_start; each task gives them its own meaning: menu blink/icon
 * cursor threads use [0] timer and [1] state, event unit/sprite-move threads
 * keep the misc id in [0], and DisplayMessage text threads use [2] x offset,
 * [3] y offset, [4] arrow offset, [5] opening type and [6] width override. The
 * remaining saved context/stack stays provisional. */
typedef struct native_thread {
    s32 function_parameter_1; /* 0x00 */
    s32 function_parameter_2; /* 0x04 */
    s32 function_parameter_3; /* 0x08 */
    s32 function_parameter_4; /* 0x0c; set by WORLD's four-parameter helper */
    u8 _pad10[0x28];
    void* global_pointer;       /* 0x38 */
    u32* stack_pointer;         /* 0x3c */
    u32* frame_pointer;         /* 0x40 */
    void (*code_pointer)(void); /* 0x44 */
    s32 is_running;             /* 0x48 */
    s32 task_id;                /* 0x4c */
    s32 task_words[7];          /* 0x50: per-task words, see above */
    u8 _pad6c[0x3f0 - 0x6c];
    u32 stack_top[4]; /* 0x3f0: initial SP/FP; the stack grows down from here */
} native_thread_t;

/* Same layout, separate module-owned scheduler instances. */
typedef native_thread_t world_thread_t;

typedef native_thread_t battle_thread_t;

/* Scheduler arrays use sixteen fixed 0x400-byte native thread slots. */
enum {
    NATIVE_THREAD_SLOT_COUNT = 16,
    NATIVE_THREAD_STRIDE = 0x400,
    NATIVE_THREAD_ARRAY_BYTES = 0x4000,
    NATIVE_THREAD_LAST_WORD_OFFSET = 0x3ffc,
};

/* task_id values shared by the BATTLE and WORLD schedulers. Each named id is
 * set by one family of twin thread functions (set_current_task_id or a
 * direct task_id store) and is looked up with *_thread_find_running_by_task.
 * 1/2/3 are the dialogue text threads' handshake: a text thread runs as 1,
 * sets 2 to wait until another thread stores 1 to resume it, and stops when
 * another thread stores 3. Ids without a known setter stay literal. */
typedef enum native_thread_task {
    NATIVE_THREAD_TASK_RESUME = 1,
    NATIVE_THREAD_TASK_WAIT_FOR_RESUME = 2,
    NATIVE_THREAD_TASK_STOP_REQUEST = 3,
    NATIVE_THREAD_TASK_CAMERA = 4,
    NATIVE_THREAD_TASK_MAP_DARKNESS = 6,
    NATIVE_THREAD_TASK_MAP_LIGHT = 7,
    NATIVE_THREAD_TASK_EVENT_BLOCK = 8,
    NATIVE_THREAD_TASK_SPRITE_MOVE = 0xb,
    NATIVE_THREAD_TASK_COLOR_SCREEN = 0xc,
    NATIVE_THREAD_TASK_EARTHQUAKE = 0xd,
    NATIVE_THREAD_TASK_ADD_UNIT = 0xe,
    NATIVE_THREAD_TASK_UNIT_EDITOR_PANEL = 0x15,
    NATIVE_THREAD_TASK_NUMERIC_DISPLAY_PANEL = 0x23,
    NATIVE_THREAD_TASK_MESSAGE_BOX = 0x26,
    NATIVE_THREAD_TASK_MENU_WINDOW_BUILD = 0x31,
    /* Text thread waiting for its next message; Change Dialog and
     * world_text_resume_printing swap in new text and store 1. */
    NATIVE_THREAD_TASK_DIALOG_AWAIT_TEXT = 0x33,
    NATIVE_THREAD_TASK_LOAD_EVTCHR = 0x34,
    NATIVE_THREAD_TASK_BG_SOUND = 0x35,
    /* Dark Screen thread start; Remove Dark Screen stores it into the
     * 0x37 thread to end the hold. */
    NATIVE_THREAD_TASK_DARK_SCREEN = 0x36,
    NATIVE_THREAD_TASK_DARK_SCREEN_HOLD = 0x37,
    NATIVE_THREAD_TASK_CONDITION_DISPATCH = 0x38,
    NATIVE_THREAD_TASK_UNIT_STATUS_BANNER = 0x39,
    NATIVE_THREAD_TASK_STATUS_PANEL = 0x3b,
    NATIVE_THREAD_TASK_SHOW_GRAPHIC = 0x3d,
    NATIVE_THREAD_TASK_DEBUG_UNIT_EDITOR = 0x3e,
    NATIVE_THREAD_TASK_HELP_MENU = 0x40,
    NATIVE_THREAD_TASK_PLAY_EFFECT = 0x41,
    NATIVE_THREAD_TASK_MENU_SOUND_DELAY = 0x42,
    NATIVE_THREAD_TASK_INFLICT_STATUS = 0x43,
    NATIVE_THREAD_TASK_EXECUTE_EVENT = 0x44,
    NATIVE_THREAD_TASK_UNIT_MESSAGE = 0x46,
} native_thread_task_e;

typedef char native_thread_size_must_be_0x400[(sizeof(native_thread_t) == 0x400) ? 1 : -1];

typedef char native_thread_running_must_be_0x48[((unsigned long)&((native_thread_t*)0)->is_running == 0x48) ? 1 : -1];

typedef char native_thread_task_must_be_0x4c[((unsigned long)&((native_thread_t*)0)->task_id == 0x4c) ? 1 : -1];

/* 0x8014ceb4..0x8014cf58 forwards a0-a3 to the callback in 0x80173ca8,
 * using thread 0's stack when necessary. It does not copy stacked arguments
 * across that stack switch; callers must not assume they are transferred.
 * The unspecified argument list is intentional, not a missing prototype. */
extern void (*g_battle_thread_call_target)(void);

/* The unspecified argument list is intentional for the same stack-switching
 * callback bridge used by BATTLE. */
extern void (*g_world_thread_call_target)(void);
extern s32 g_battle_current_thread_id;

/* Full BATTLE backing array at 0x8016986c. The pointer slot 0x80165f98 is
 * separate. */
extern battle_thread_t g_battle_thread_contexts[];

/* View of g_battle_thread_contexts biased to task_id (0x801698b8): [i][0] is
 * thread i's task_id and [i][1] its task_words[0]. Target code addresses the
 * biased symbol directly, so it must not be rebound to the context base. The
 * WORLD twin is g_world_thread_task_ids. */
extern s32 g_battle_thread_task_ids[][256];
extern native_thread_t g_world_thread_contexts[];
extern s32 g_world_thread_current_id;

/* BATTLE's pointer slot at 0x80165f98, set to g_battle_thread_contexts by
 * battle_menu_init_subsystem_pointers. Every access indexes it with a 0x400
 * stride, and the scheduler primitives reach is_running (0x48) and task_id
 * (0x4c) through it, so it is a battle_thread_t*, not the u8* placeholder it
 * was spelled as in 22 files. Files doing raw byte arithmetic cast (u8*) at
 * the use site. */
extern battle_thread_t* g_battle_threads;

/* WORLD's pointer slot and fixed-array binding refer to the same scheduler
 * storage through different symbols. */
extern world_thread_t* g_world_threads;

/* thread */
s32 battle_thread_call_on_main_stack();
void battle_thread_exit_current(void);
s32 battle_thread_find_running_by_task(s32 task_id);
void* battle_thread_get_current_global_pointer(void);
s32 battle_thread_get_current_parameter_1(void);
s32 battle_thread_get_current_parameter_2(void);
s32 battle_thread_get_current_parameter_3(void);
s32 battle_thread_get_current_task_id(void);
s32 battle_thread_is_previous_running(void);
s32 battle_thread_is_running(s32 thread_id);
s32 battle_thread_is_running_8014cc94(s32 thread_id);
void battle_thread_reset_scheduler(void);
s32 battle_thread_resolve_id(s32 requested_thread_id);
s32 battle_thread_resolve_id_after_current(s32 requested_thread_id);
void battle_thread_resume(s32 thread_id);
void battle_thread_set_current_task_id(s32 task_id);
void battle_thread_set_parameters(s32 thread_id, s32 first, s32 second, s32 third);
void battle_thread_set_parameters_4(s32 thread_id, s32 first, s32 second, s32 third, s32 fourth);
void battle_thread_start(s32 thread_id, void (*function)(void));
void battle_thread_suspend(s32 thread_id);
void battle_thread_wait_frames(s32 count);
void battle_thread_wait_until_inactive(s32 thread_id);

/* 0x8014ca80..0x8014cbb4: save this native context and resume the next
 * active one. Shared state can change before this caller resumes. */
void battle_thread_yield(void);

/* thread */
s32 world_thread_call_on_main_stack();
void world_thread_exit_current(void);
s32 world_thread_find_running_by_task(s32 task_id);
void* world_thread_get_current_global_pointer(void);
void* world_thread_get_current_parameter_1(void);
s32 world_thread_get_current_parameter_2(void);
s32 world_thread_get_current_parameter_3(void);
s32 world_thread_get_current_task_id(void);
s32 world_thread_is_previous_running(void);
s32 world_thread_is_running(s32 thread_id);
s32 world_thread_is_running_80100164(s32 thread_id);
void world_thread_reset_scheduler(void);
s32 world_thread_resolve_id(s32 requested_thread_id);
s32 world_thread_resolve_id_after_current(s32 requested_thread_id);
void world_thread_resume(s32 thread_id);
void world_thread_set_current_task_id(s32 task_id);
void world_thread_set_parameters(s32 thread_id, s32 first, s32 second, s32 third);
void world_thread_set_parameters_4(s32 thread_id, s32 first, s32 second, s32 third, s32 fourth);
void world_thread_set_task_id_to_three(s32 thread_id);
void world_thread_start(s32 thread_id, void (*function)(void));
void world_thread_suspend(s32 thread_id);
void world_thread_wait_frames(s32 ticks);
void world_thread_wait_until_inactive(s32 thread_id);
void world_thread_yield(void);
#endif
