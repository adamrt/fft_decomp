#ifndef THREAD_H
#define THREAD_H

/* Cooperative game threads: the thread context, the BATTLE and WORLD schedulers that run them, the slot and task ids
 * the programs and screens give them, and the main-stack call. */

#include "psx/types.h"

/* context */
/* Native cooperative scheduler context, not scenario bytecode or its operand buffer. thread_start initializes the
 * same layout in BATTLE and WORLD: both index slots with thread_id << 10 and start SP/FP at slot + 0x3f0.
 * thread_set_parameters and thread_set_four_parameters store full 32-bit payload values. The seven 32-bit task
 * values at 0x50 are zeroed by thread_start; each task gives them its own meaning, named beside its thread
 * functions: DIALOG_THREAD_WORD_* (text.h), SPRITE_MOVE_THREAD_WORD_* (script.h), and the menu cursor bob and
 * scrolling text list words MENU_CURSOR_THREAD_WORD_* (menu_cursor.h) and MENU_LIST_THREAD_WORD_* (menu_list.h). The
 * scheduler saves s0-s7 and k0/k1 before the GP, SP, FP and return address. */
typedef struct thread {
    s32 function_parameter_1;   /* 0x00 */
    s32 function_parameter_2;   /* 0x04 */
    s32 function_parameter_3;   /* 0x08 */
    s32 function_parameter_4;   /* 0x0c; set by WORLD's four-parameter helper */
    u32 saved_s0;               /* 0x10 */
    u32 saved_s1;               /* 0x14 */
    u32 saved_s2;               /* 0x18 */
    u32 saved_s3;               /* 0x1c */
    u32 saved_s4;               /* 0x20 */
    u32 saved_s5;               /* 0x24 */
    u32 saved_s6;               /* 0x28 */
    u32 saved_s7;               /* 0x2c */
    u32 saved_k0;               /* 0x30 */
    u32 saved_k1;               /* 0x34 */
    void* global_pointer;       /* 0x38 */
    u32* stack_pointer;         /* 0x3c */
    u32* frame_pointer;         /* 0x40 */
    void (*code_pointer)(void); /* 0x44 */
    s32 is_running;             /* 0x48 */
    s32 task_id;                /* 0x4c: thread_task_e */
    s32 task_words[7];          /* 0x50: per-task 32-bit values, see above */
    u8 stack[0x3f0 - 0x6c];     /* 0x6c: the thread's stack */
    u32 stack_top[4];           /* 0x3f0: initial SP/FP; the stack grows down from here */
} thread_t;
typedef char thread_size_must_be_1024[(sizeof(thread_t) == 1024) ? 1 : -1];

/* Reset loops clear the first sixteen 0x400-byte slots. The table itself has
 * VARIANT_THREAD_TABLE_SLOT_COUNT slots:
 * 16 in BATTLE, 17 in WORLD, whose slot 16 runs tutorial text; the scheduler
 * and the slot searches scan it, but reset does not clear it. */
enum { THREAD_ID_COUNT = 16, THREAD_STRIDE = 1024 };

/* BATTLE and WORLD each define the backing array, slot pointer and current id.
 * EVENT overlays use the resident BATTLE scheduler. */
extern thread_t g_thread_contexts[];
extern thread_t* g_threads;
extern s32 g_thread_current_id;

/* Per-thread running flags that the BUNIT, CARD and WORLD input loops sample
 * before a yield and compare after it; g_event_thread_status_snapshot is the
 * event interpreter's. */
extern u8 g_thread_status_snapshot[16];

/* slots */
/* Slot numbers of the cooperative threads: the index into g_threads and the first argument of the thread calls. Slot
 * 0 is the program's main loop. No slot belongs to one thread for good: the battle and world programs run their
 * threads in fixed slots, every menu screen runs its own in the same slots while it is up, and a menu thread starts
 * its child menu one slot below itself. Each enum here maps the slots of one of them, this one the battle and world
 * programs, which share the event library's threads; the screens' own headers map the slots they use
 * (screen_thread_slot_e, world_map_thread_slot_e, deploy_thread_slot_e, unit_list_thread_slot_e,
 * equipment_thread_slot_e, after_battle_thread_slot_e). */
typedef enum thread_slot {
    THREAD_SLOT_MAIN = 0,
    THREAD_SLOT_EVENT = 1,           /* the event script: script_run_next_event */
    THREAD_SLOT_UNIT_VIEW = 2,       /* the unit view supervisor that starts the four panels below */
    THREAD_SLOT_MENU_BUILDING = 3,   /* the menu entry starter and the help menu thread */
    THREAD_SLOT_SYSTEM_FUNCTION = 4, /* the system function thread and the spell quote it dispatches */
    THREAD_SLOT_MENU_GRANDCHILD = 6, /* the action slot list under the skillset list */
    THREAD_SLOT_MENU_CHILD = 7,      /* the skillset list under the action menu */
    THREAD_SLOT_MENU = 8,            /* the open entry of the menu table: action menu, mini menu, a
                                      * system function's menu, the spell quote text */
    /* The unit view's panel slots, named for the thread each usually runs. */
    THREAD_SLOT_TILE_INFO_PANEL = 10,    /* tile info or numeric display */
    THREAD_SLOT_UNIT_SUMMARY_PANEL = 11, /* unit summary */
    THREAD_SLOT_RIGHT_UNIT_STATS = 12,   /* right unit stats or attack caster stats */
    THREAD_SLOT_HOVERED_UNIT_STATS = 13, /* hovered unit stats or attack target stats */
    /* Passed to the thread_resolve_id functions, which return a slot below 16
     * unchanged and otherwise the first idle slot. */
    THREAD_SLOT_FIRST_IDLE = 16
} thread_slot_e;

/* The menu screens: formation and unit list, shop, deployment, post-battle,
 * save, debug, bar. */
typedef enum thread_screen_slot {
    THREAD_SCREEN_SLOT_TEXT = 1,              /* help, description and message text of a screen */
    THREAD_SCREEN_SLOT_MESSAGE = 2,           /* a message box or text window */
    THREAD_SCREEN_SLOT_MENU_CHILD = 5,        /* the child of the menu in slot 6 */
    THREAD_SCREEN_SLOT_MENU = 6,              /* the menu or notice a WORLD screen or the save screen runs */
    THREAD_SCREEN_SLOT_COMPARISON_STATUS = 7, /* the second (comparison) unit status banner */
    THREAD_SCREEN_SLOT_UNIT_STATUS = 8,       /* the unit status banner */
    THREAD_SCREEN_SLOT_CHARACTER_STATUS = 9,  /* the character status or equipment-and-ability panel */
    THREAD_SCREEN_SLOT_NUMERIC_PANEL = 12     /* the numeric stats panel */
} thread_screen_slot_e;

/* Slots one routine takes for a thread of its own while the slot's usual
 * thread is not running. */
typedef enum thread_borrowed_slot {
    THREAD_BORROWED_SLOT_CARD_BACKDROP = 2, /* the memory card screen's zooming backdrop */
    THREAD_BORROWED_SLOT_DEPLOYMENT = 6,    /* attack_deployment_entrypoint, or the map-data-load-complete thread
                                             * when a battle has no deployment */
    /* The AT list of the crystal pickup menu, once the unit view's panels have stopped. */
    THREAD_BORROWED_SLOT_AT_LIST_MENU_CHILD = 9, /* a wait-forever thread while the AT list's child runs */
    THREAD_BORROWED_SLOT_AT_LIST_MENU = 10,
    THREAD_BORROWED_SLOT_FADE_OUT = 13 /* the fade-out thread that follows the unit list overlay */
} thread_borrowed_slot_e;

/* tasks */
/* task_id values shared by the BATTLE and WORLD schedulers. Each named id is
 * set by one thread function (thread_set_current_task_id or a direct task_id
 * store) and is looked up with thread_find_running_by_task.
 * 1/2/3 are the dialogue text threads' handshake: a text thread runs as 1,
 * sets 2 to wait until another thread stores 1 to resume it, and stops when
 * another thread stores 3. Ids without a known setter stay literal. */
typedef enum thread_task {
    /* No task: thread_start and thread_exit_current store it, and
     * world_map_menu_start_group stores it into thread 12 when the location changes. */
    THREAD_TASK_NONE = 0,
    THREAD_TASK_RESUME = 1,
    THREAD_TASK_WAIT_FOR_RESUME = 2,
    THREAD_TASK_STOP_REQUEST = 3,
    THREAD_TASK_CAMERA = 4,
    THREAD_TASK_MAP_DARKNESS = 6,
    THREAD_TASK_MAP_LIGHT = 7,
    THREAD_TASK_EVENT_BLOCK = 8,
    THREAD_TASK_SPRITE_MOVE = 11,
    THREAD_TASK_COLOR_SCREEN = 12,
    THREAD_TASK_EARTHQUAKE = 13,
    THREAD_TASK_ADD_UNIT = 14,
    /* Unit-view panels: the ids the view thread sets pair with each panel thread. */
    THREAD_TASK_TILE_INFO_PANEL = 16,
    THREAD_TASK_HOVERED_UNIT_STATS = 17,
    THREAD_TASK_ATTACK_TARGET_STATS = 18,
    THREAD_TASK_ATTACK_CASTER_STATS = 19,
    THREAD_TASK_UNIT_SUMMARY_PANEL = 20,
    THREAD_TASK_STATUS_SCREEN = 21,
    /* g_world_map_menu_groups gives its three text windows the ids 0x20,
     * 0x21 and 0x22 (threads 9, 10 and 11); the window thread slides only the
     * second one with the main scroll. */
    THREAD_TASK_MENU_GROUP_SECOND_WINDOW = 33,
    THREAD_TASK_NUMERIC_DISPLAY_PANEL = 35,
    THREAD_TASK_MESSAGE_BOX = 38,
    THREAD_TASK_MENU_WINDOW_BUILD = 49,
    /* Text thread waiting for its next message; Change Dialog and
     * screen_text_resume_printing swap in new text and store 1. */
    THREAD_TASK_DIALOG_AWAIT_TEXT = 51,
    THREAD_TASK_LOAD_EVTCHR = 52,
    THREAD_TASK_BG_SOUND = 53,
    /* Dark Screen thread start; Remove Dark Screen stores it into the
     * 0x37 thread to end the hold. */
    THREAD_TASK_DARK_SCREEN = 54,
    THREAD_TASK_DARK_SCREEN_HOLD = 55,
    THREAD_TASK_CONDITION_DISPATCH = 56,
    THREAD_TASK_UNIT_STATUS_BANNER = 57,
    THREAD_TASK_RIGHT_UNIT_STATS = 58,
    THREAD_TASK_STATUS_PANEL = 59,
    THREAD_TASK_SHOW_GRAPHIC = 61,
    THREAD_TASK_MOUNTED_STATUS_SCREEN = 62,
    THREAD_TASK_HELP_MENU = 64,
    THREAD_TASK_PLAY_EFFECT = 65,
    THREAD_TASK_MINI_MENU = 66,
    THREAD_TASK_INFLICT_STATUS = 67,
    THREAD_TASK_EXECUTE_EVENT = 68,
    THREAD_TASK_UNIT_MESSAGE = 70
} thread_task_e;

/* lifecycle */
void thread_start(s32 thread_id, void (*function)(void));
void thread_resume(s32 thread_id);
void thread_suspend(s32 thread_id);
void thread_exit_current(void);
s32 thread_is_running(s32 thread_id);
void thread_reset_scheduler(void);
s32 thread_find_running_by_task(s32 task_id);
s32 thread_resolve_id(s32 requested_thread_id);
s32 thread_resolve_id_after_current(s32 requested_thread_id);

/* The menu callers' running-flag accessor. WORLD links this separately from thread_is_running. */
s32 menu_thread_is_running(s32 thread_id);

/* REQUIRE's own call through to thread_exit_current. */
void thread_exit_current_wrapper(void);

/* parameters and task ids */
void thread_set_parameters(s32 thread_id, s32 first, s32 second, s32 third);
void thread_set_four_parameters(s32 thread_id, s32 first, s32 second, s32 third, s32 fourth);
s32 thread_get_current_task_id(void);
void thread_set_current_task_id(s32 task_id);

/* Original assembly accessors: BATTLE adds the record offset with addu,
 * WORLD with the trapping add. */
s32 thread_get_current_parameter_1(void);
s32 thread_get_current_parameter_2(void);
s32 thread_get_current_parameter_3(void);
s32 thread_is_previous_running(void);
s32 thread_is_running_asm(s32 thread_id);

/* scheduling */
/* Yield to the next active context in the resident scene scheduler.
 * BATTLE scans 16 slots; WORLD scans 17. Each keeps its own frame hook. */
void thread_yield(void);
void thread_wait_frames(s32 count);
void thread_wait_until_inactive(s32 thread_id);
void thread_wait_forever(void);
void thread_wait_forever_by_frames(void);

/* context capture */
/* Named relocations preserve the assembled helpers' $at load sequence. */
#define THREAD_LOAD_GLOBAL(result, global)                                                                             \
    __asm__("lui $1, %%hi(%1)\naddu $1,$1,$zero\nlw %0, %%lo(%1)($1)"                                                  \
        : "=r"(result)                                                                                                 \
        : "i"(&(global)), "m"(global)                                                                                  \
        : "$1")

/* Read the running CPU's $gp when seeding a native thread context. */
void* thread_get_current_global_pointer(void);

/* main-stack calls */
/* Main-stack callback dispatch, bound to the resident BATTLE or WORLD scheduler.
 * It forwards a0-a3 unchanged and switches to thread 0's stack when necessary;
 * arguments beyond those registers are not copied across the stack switch.
 * The argument list stays unspecified because callback signatures differ. */
extern void (*g_thread_call_target)(void);

s32 thread_call_on_main_stack();

#endif
