/* Load ability effects and run their script instances, native callbacks and parent/child timelines. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/battle_map_mesh.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/effect.h"
#include "fft/effect_emitter_data.h"
#include "fft/effect_file.h"
#include "fft/effect_heap.h"
#include "fft/effect_particle.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/effect_timeline.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/music.h"
#include "fft/result.h"
#include "fft/sprite.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "fft/weather.h"
#include "psx/gte_inline.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

effect_context_t g_effect_context;
static u8* g_effect_frame_data_start;

/* g_effect_load_state: the steps of effect_file_load_update, which loads the ability's effect file
 * while its charge secondary effect plays. */
typedef enum playback_load_state {
    EFFECT_LOAD_START = 0,           /* start reading the file */
    EFFECT_LOAD_READING = 1,         /* wait for the read */
    EFFECT_LOAD_COUNTDOWN = 2,       /* wait out the 60 frames given to a charge secondary effect */
    EFFECT_LOAD_ENDING_SECONDARY = 3 /* wait for the charge secondary effect to end */
} playback_load_state_e;

/* Loaded effect sections and their saved starting positions. */
extern struct suzuki_sound_resource* g_effect_sound_section_ptr;
extern s32 g_effect_sound_data_base;

/* Per-channel call counts. */
extern u8 g_effect_sound_call_counts[];

extern effect_palette_section_t* g_effect_palette_ptr;
extern const u8* g_effect_script;
extern u8* g_effect_animation_sequences_start;
extern const effect_timing_curves_t* g_effect_timing_curves;

/* ID of the charge secondary effect started with the ability; zero for none. */
extern u8 g_effect_secondary_charge_id;

/* Secondary-animation id per charge animation set, indexed by the low 16 bits of the
 * caller's packed argument. */
extern u8 g_effect_secondary_by_charge_animation[];

/* Ability effect loading and playback. */
extern u32 g_heap_battle_end_address;

/* File ID (EFFECT/E###.BIN) of the requested effect. */
extern s16 g_effect_id;

/* Cleared when an ability effect is requested; nothing reads it. */
extern s32 g_effect_request_unread;

/* Action/reaction ability IDs index directly; event IDs map after those rows. */
extern s16 g_effect_ability_files[];

/* Disc extents for ability effects; sizes include the final sector.
 * Entry 0 deliberately uses the same file as Cure (entry 1). */
static file_extent_t g_disc_extents[EFFECT_FILE_ID_COUNT] = {
    { 61026, 45056 },
    { 61026, 45056 },
    { 61048, 47104 },
    { 61071, 47104 },
    { 61094, 94208 },
    { 61140, 55296 },
    { 61167, 55296 },
    { 61194, 55296 },
    { 61221, 47104 },
    { 61244, 49152 },
    { 61268, 49152 },
    { 61292, 45056 },
    { 61314, 47104 },
    { 61337, 47104 },
    { 61360, 47104 },
    { 61383, 96256 },
    { 61430, 30720 },
    { 61445, 36864 },
    { 61463, 49152 },
    { 61487, 53248 },
    { 61513, 47104 },
    { 61536, 53248 },
    { 61562, 47104 },
    { 61585, 83968 },
    { 61626, 47104 },
    { 61649, 51200 },
    { 61674, 51200 },
    { 61699, 86016 },
    { 61741, 49152 },
    { 61765, 45056 },
    { 61787, 45056 },
    { 61809, 47104 },
    { 61832, 47104 },
    { 61855, 90112 },
    { 61899, 47104 },
    { 61922, 90112 },
    { 61966, 47104 },
    { 71150, 0 },
    { 71150, 0 },
    { 61991, 47104 },
    { 62014, 24576 },
    { 62026, 53248 },
    { 71150, 0 },
    { 62053, 49152 },
    { 62077, 26624 },
    { 62090, 47104 },
    { 62113, 47104 },
    { 62136, 83968 },
    { 71150, 0 },
    { 62178, 45056 },
    { 62200, 45056 },
    { 62222, 45056 },
    { 62244, 47104 },
    { 62267, 47104 },
    { 62290, 45056 },
    { 62312, 45056 },
    { 62334, 45056 },
    { 62356, 45056 },
    { 62378, 47104 },
    { 62401, 47104 },
    { 62424, 30720 },
    { 62439, 47104 },
    { 62462, 47104 },
    { 62485, 47104 },
    { 71150, 0 },
    { 62509, 100352 },
    { 62558, 83968 },
    { 62599, 90112 },
    { 62643, 88064 },
    { 62686, 55296 },
    { 62713, 104448 },
    { 62764, 92160 },
    { 62809, 83968 },
    { 62850, 92160 },
    { 62895, 96256 },
    { 71150, 0 },
    { 62943, 90112 },
    { 62987, 102400 },
    { 63037, 83968 },
    { 63078, 94208 },
    { 63124, 94208 },
    { 63170, 45056 },
    { 63192, 45056 },
    { 63214, 45056 },
    { 63236, 45056 },
    { 63258, 45056 },
    { 63280, 45056 },
    { 63302, 45056 },
    { 63324, 53248 },
    { 63350, 53248 },
    { 63376, 53248 },
    { 63402, 53248 },
    { 63428, 53248 },
    { 63454, 55296 },
    { 63481, 55296 },
    { 63508, 45056 },
    { 63530, 45056 },
    { 63552, 45056 },
    { 63574, 49152 },
    { 63598, 45056 },
    { 63620, 45056 },
    { 63642, 45056 },
    { 63664, 45056 },
    { 63686, 45056 },
    { 63708, 45056 },
    { 63730, 45056 },
    { 63752, 45056 },
    { 63774, 45056 },
    { 63796, 45056 },
    { 63818, 45056 },
    { 63840, 45056 },
    { 63862, 47104 },
    { 63885, 47104 },
    { 63908, 47104 },
    { 63931, 47104 },
    { 63954, 47104 },
    { 63977, 47104 },
    { 64000, 47104 },
    { 64023, 47104 },
    { 64046, 47104 },
    { 64069, 47104 },
    { 64092, 47104 },
    { 64115, 45056 },
    { 64137, 47104 },
    { 64160, 47104 },
    { 64183, 47104 },
    { 64206, 47104 },
    { 64229, 45056 },
    { 64251, 47104 },
    { 64274, 47104 },
    { 64297, 45056 },
    { 64319, 45056 },
    { 64341, 45056 },
    { 64363, 47104 },
    { 64386, 61440 },
    { 64416, 47104 },
    { 64439, 49152 },
    { 64463, 49152 },
    { 64487, 53248 },
    { 64513, 49152 },
    { 64537, 49152 },
    { 64561, 47104 },
    { 64584, 86016 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 64637, 28672 },
    { 71150, 0 },
    { 64652, 45056 },
    { 64674, 47104 },
    { 64697, 47104 },
    { 64720, 45056 },
    { 64742, 51200 },
    { 64767, 53248 },
    { 71150, 0 },
    { 64794, 55296 },
    { 64821, 51200 },
    { 64846, 47104 },
    { 64869, 51200 },
    { 64894, 51200 },
    { 64919, 49152 },
    { 64943, 49152 },
    { 64967, 49152 },
    { 64991, 49152 },
    { 65015, 47104 },
    { 65038, 49152 },
    { 65062, 96256 },
    { 65109, 30720 },
    { 65124, 88064 },
    { 65167, 47104 },
    { 65190, 47104 },
    { 65213, 47104 },
    { 65236, 47104 },
    { 65259, 47104 },
    { 65282, 47104 },
    { 65305, 47104 },
    { 65328, 47104 },
    { 65351, 47104 },
    { 65374, 47104 },
    { 65397, 47104 },
    { 65420, 47104 },
    { 65443, 45056 },
    { 65465, 45056 },
    { 65487, 45056 },
    { 65509, 49152 },
    { 65533, 51200 },
    { 65558, 45056 },
    { 65580, 45056 },
    { 65602, 45056 },
    { 65624, 45056 },
    { 65646, 45056 },
    { 65668, 45056 },
    { 65690, 45056 },
    { 65712, 45056 },
    { 65734, 45056 },
    { 65756, 45056 },
    { 65778, 49152 },
    { 65802, 49152 },
    { 65826, 49152 },
    { 71150, 0 },
    { 65851, 49152 },
    { 71150, 0 },
    { 65876, 30720 },
    { 65891, 45056 },
    { 65913, 47104 },
    { 65936, 47104 },
    { 65959, 47104 },
    { 65982, 45056 },
    { 66004, 45056 },
    { 66026, 45056 },
    { 66048, 45056 },
    { 66070, 45056 },
    { 66092, 45056 },
    { 66114, 45056 },
    { 66136, 45056 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 66224, 45056 },
    { 71150, 0 },
    { 66247, 81920 },
    { 66287, 88064 },
    { 66330, 88064 },
    { 66373, 49152 },
    { 66397, 49152 },
    { 66421, 86016 },
    { 66463, 45056 },
    { 66485, 88064 },
    { 66528, 83968 },
    { 66569, 30720 },
    { 66584, 30720 },
    { 66599, 83968 },
    { 66640, 90112 },
    { 66684, 94208 },
    { 66730, 106496 },
    { 66782, 45056 },
    { 66804, 88064 },
    { 66847, 55296 },
    { 66874, 88064 },
    { 66917, 53248 },
    { 66943, 59392 },
    { 66972, 53248 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 67003, 53248 },
    { 67029, 65536 },
    { 67061, 53248 },
    { 67087, 94208 },
    { 67133, 94208 },
    { 67179, 45056 },
    { 67201, 47104 },
    { 67224, 47104 },
    { 67247, 47104 },
    { 67270, 47104 },
    { 67293, 45056 },
    { 67315, 47104 },
    { 67338, 47104 },
    { 67361, 47104 },
    { 67384, 45056 },
    { 67406, 47104 },
    { 67429, 45056 },
    { 67451, 47104 },
    { 67474, 49152 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 67512, 81920 },
    { 67552, 81920 },
    { 67592, 81920 },
    { 67632, 83968 },
    { 67673, 83968 },
    { 67714, 81920 },
    { 67754, 81920 },
    { 67794, 81920 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 67844, 47104 },
    { 67867, 30720 },
    { 67882, 28672 },
    { 67896, 30720 },
    { 67911, 47104 },
    { 67934, 47104 },
    { 67957, 47104 },
    { 67980, 47104 },
    { 68003, 49152 },
    { 71150, 0 },
    { 68028, 28672 },
    { 68042, 45056 },
    { 68064, 45056 },
    { 68086, 47104 },
    { 68109, 45056 },
    { 68131, 28672 },
    { 68145, 28672 },
    { 68159, 28672 },
    { 68173, 30720 },
    { 68188, 30720 },
    { 68203, 28672 },
    { 68217, 30720 },
    { 68232, 30720 },
    { 68247, 30720 },
    { 68262, 28672 },
    { 68276, 30720 },
    { 68291, 28672 },
    { 68305, 32768 },
    { 68321, 28672 },
    { 68335, 28672 },
    { 68349, 36864 },
    { 68367, 28672 },
    { 68381, 34816 },
    { 68398, 30720 },
    { 68413, 47104 },
    { 68436, 34816 },
    { 68453, 30720 },
    { 68468, 30720 },
    { 68483, 32768 },
    { 68499, 32768 },
    { 68515, 30720 },
    { 68530, 28672 },
    { 68544, 30720 },
    { 68559, 30720 },
    { 68574, 30720 },
    { 68589, 28672 },
    { 68603, 32768 },
    { 68619, 30720 },
    { 68634, 47104 },
    { 68657, 55296 },
    { 68684, 20480 },
    { 68694, 28672 },
    { 68708, 28672 },
    { 68722, 30720 },
    { 68737, 28672 },
    { 68751, 30720 },
    { 68766, 28672 },
    { 68780, 30720 },
    { 68795, 28672 },
    { 68809, 49152 },
    { 68833, 47104 },
    { 68856, 47104 },
    { 68879, 47104 },
    { 68902, 47104 },
    { 68925, 47104 },
    { 68948, 61440 },
    { 68978, 49152 },
    { 69002, 61440 },
    { 69032, 28672 },
    { 69046, 28672 },
    { 69060, 36864 },
    { 69078, 30720 },
    { 69093, 28672 },
    { 69107, 28672 },
    { 69121, 30720 },
    { 69136, 38912 },
    { 69155, 38912 },
    { 69174, 49152 },
    { 69198, 98304 },
    { 69246, 86016 },
    { 69288, 28672 },
    { 69302, 28672 },
    { 69316, 30720 },
    { 69331, 30720 },
    { 69346, 30720 },
    { 69361, 28672 },
    { 69375, 30720 },
    { 69390, 47104 },
    { 69413, 30720 },
    { 69428, 36864 },
    { 69446, 81920 },
    { 69486, 83968 },
    { 69527, 47104 },
    { 69550, 30720 },
    { 69565, 83968 },
    { 69606, 49152 },
    { 69630, 55296 },
    { 69657, 92160 },
    { 69702, 83968 },
    { 69743, 83968 },
    { 69784, 83968 },
    { 69825, 30720 },
    { 69840, 94208 },
    { 69886, 67584 },
    { 69919, 47104 },
    { 69942, 51200 },
    { 69967, 57344 },
    { 69995, 45056 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 70053, 61440 },
    { 70083, 63488 },
    { 70114, 63488 },
    { 70145, 69632 },
    { 70179, 61440 },
    { 70209, 51200 },
    { 70234, 53248 },
    { 70260, 49152 },
    { 70284, 51200 },
    { 70309, 45056 },
    { 70331, 51200 },
    { 70356, 71680 },
    { 70391, 71680 },
    { 70426, 71680 },
    { 70461, 61440 },
    { 70491, 71680 },
    { 70526, 71680 },
    { 70561, 53248 },
    { 70587, 55296 },
    { 70614, 53248 },
    { 70640, 47104 },
    { 70663, 53248 },
    { 70689, 55296 },
    { 70716, 53248 },
    { 70742, 63488 },
    { 70773, 61440 },
    { 70803, 61440 },
    { 70833, 63488 },
    { 70864, 63488 },
    { 70895, 49152 },
    { 70919, 61440 },
    { 70949, 32768 },
    { 70965, 32768 },
    { 70981, 51200 },
    { 71006, 49152 },
    { 71030, 63488 },
    { 71061, 47104 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71150, 0 },
    { 71106, 45056 },
    { 71128, 45056 },
    { 71150, 0 },
};

extern effect_file_header_t* g_effect_file_data_ptrs[EFFECT_FILE_ID_COUNT];
extern u8 g_effect_load_state; /* playback_load_state_e */
extern s16 g_effect_load_countdown;
extern s32 g_effect_playback_phase; /* effect_playback_phase_e */

/* State-record selection and dispatch. */
extern s16 g_effect_current_record_index;
extern s16 g_effect_root_record_index;

/* Heads of the allocated and available record lists, chained through effect_record_t.next_index.
 * The available head is unsigned; the allocated head matches retail only when declared s16. */
extern s16 g_effect_allocated_record_head;
extern u16 g_effect_available_record_head;
extern effect_script_result_e (*g_effect_script_instructions[120])(effect_record_t*);
extern effect_callback_t g_effect_native_callbacks[EFFECT_NATIVE_CALLBACK_COUNT];
extern effect_callback_t g_effect_callback_slots[];

/* Written by effect-script opcode 05; no game source reads it. */
extern u16 g_effect_script_opcode_05_value;

static void script_instance_free(s16 record_index);

static s32 script_block_run(s16 index);

static s16 script_instance_start(const u8* script, s16 start_pc, s16 parent_index);

static s32 parent_timing_curve_evaluate(effect_record_t* record);

static s32 child_timing_curve_evaluate(effect_record_t* record);

static effect_callback_t native_callback_get(s16 callback_id);

/* Upload effect file `index`'s two CLUTs to the effect CLUT rows and its texture to
 * VRAM x 0x180, 0x80 bytes a row or 0x100 for a wide image. The section's address is
 * kept in g_effect_palette_ptr, where the effect heap later starts. */
static void palette_init(s16 index) {
    RECT rect;
    effect_file_header_t* base = g_effect_file_data_ptrs[index];
    effect_palette_section_t* data = (effect_palette_section_t*)((u8*)base + base->palette_and_texture);
    s32 size;
    s32 raw_size;
    s32 height;

    g_effect_palette_ptr = data;
    battle_map_palette_load_data(data->primary_clut, BATTLE_MAP_CLUT_ROW_EFFECT, 0, 1);
    battle_map_palette_load_data(data->secondary_clut, BATTLE_MAP_CLUT_ROW_EFFECT_SECOND, 0, 1);
    /* The image size is a 24-bit little-endian byte count; the rows follow from the width. */
    raw_size = data->image_size[0] + (data->image_size[1] << 8);
    raw_size = (data->image_size[2] << 16) + raw_size;
    height = raw_size;
    size = height;
    if (data->wide_image != 0) {
        height = size / 0x100;
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = height;
        LoadImage(&rect, data->pixels);
    } else {
        height = size;
        height = height / 0x80;
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x40;
        rect.h = height;
        LoadImage(&rect, data->pixels);
    }
}

/* Rewind g_effect_frame_data to its saved start for EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT.
 * Nothing stores the start pointer or selects that phase, so retail never runs it. */
static void frame_data_pointer_reset(void) {
    g_effect_frame_data = g_effect_frame_data_start;
}

/* Rewind g_effect_animation_sequences to its saved start for
 * EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT, a path retail never takes (see
 * frame_data_pointer_reset). */
static void animation_sequences_pointer_reset(void) {
    g_effect_animation_sequences = g_effect_animation_sequences_start;
}

/* Point g_effect_frame_data at the sprite group table of effect file effect_id's frame
 * section, past the section's 4-byte group count. */
static void frame_data_pointer_set(s16 effect_id) {
    effect_file_header_t* header = g_effect_file_data_ptrs[effect_id];
    effect_sprite_section_t* section = (effect_sprite_section_t*)((u8*)header + header->frame_data);

    g_effect_frame_data = (u8*)section->groups;
}

/* Point g_effect_animation_sequences 4 bytes into effect file effect_id's animation
 * sequence section, past its leading 32-bit value. */
static void animation_sequences_pointer_set(s16 effect_id) {
    effect_file_header_t* header = g_effect_file_data_ptrs[effect_id];

    g_effect_animation_sequences = (u8*)header + header->animation_sequences + 4;
}

/* Bind effect file effect_id's script section as g_effect_script. */
static void script_pointer_set(s16 effect_id) {
    effect_file_header_t* header = g_effect_file_data_ptrs[effect_id];

    g_effect_script = (const u8*)header + header->script;
}

/* Bind effect file effect_id's animation curve table as
 * g_effect_animation_curve_data. */
static void animation_curves_bind(s16 effect_id) {
    effect_file_header_t* header = g_effect_file_data_ptrs[effect_id];

    g_effect_animation_curve_data = (const effect_animation_curve_table_t*)((u8*)header + header->animation_curve);
}

/* Bind effect file effect_id's particle-system section (gravity, inertia threshold and the emitter
 * records) as g_effect_geometry_table. */
static void emitters_bind(s16 effect_id) {
    effect_file_header_t* header = g_effect_file_data_ptrs[effect_id];

    g_effect_geometry_table = (const effect_geometry_table_t*)((u8*)header + header->particle_system);
}

/* Bind effect file effect_id's timeline section as g_effect_timeline and its embedded child
 * timeline as g_effect_child_timeline. */
static void timeline_sections_bind(s16 effect_id) {
    effect_file_header_t* header = g_effect_file_data_ptrs[effect_id];
    effect_timeline_t* timeline;

    timeline = (effect_timeline_t*)((u8*)header + header->timeline);
    g_effect_timeline = timeline;
    g_effect_child_timeline = &timeline->child;
}

/* Replace the effect sounds: unregister the sound resource of the previous effect
 * file, then register this file's sound section and take its id as the base for
 * the effect's sound ids. A file without a sound section clears both globals. */
static void sound_section_load(s16 index) {
    effect_file_header_t* base;
    s32 offset;

    if (g_effect_sound_section_ptr != 0) {
        suzuki_sound_remove_resource(g_effect_sound_section_ptr);
    }

    base = g_effect_file_data_ptrs[index];
    offset = base->sound_section;
    if (offset != 0) {
        g_effect_sound_section_ptr = (suzuki_sound_resource_t*)((u8*)base + offset);
        suzuki_sound_append_resource(g_effect_sound_section_ptr);
        g_effect_sound_data_base = SUZUKI_SOUND_ID(g_effect_sound_section_ptr->id, 0);
    } else {
        g_effect_sound_section_ptr = 0;
        g_effect_sound_data_base = 0;
    }
}

/* Bind effect file effect_id's flags section as g_effect_flags_section. */
static void flags_section_pointer_init(s16 effect_id) {
    effect_file_header_t* header = g_effect_file_data_ptrs[effect_id];

    g_effect_flags_section = (const effect_flags_section_t*)((u8*)header + header->flags_section);
}

/* Bind effect file effect_id's timing-curve section as
 * g_effect_timing_curves; a zero section offset binds the file header
 * itself. */
static void timing_curves_pointer_set(s16 effect_id) {
    effect_file_header_t* header = g_effect_file_data_ptrs[effect_id];

    g_effect_timing_curves = (const effect_timing_curves_t*)((u8*)header + header->timing_curves);
}

/* Time scale of the parent timeline's frame record->timeline.frame: one of the
 * 4-bit values the effect file's timing-curve section packs two to a byte, low
 * nibble first. */
static s32 parent_timing_curve_evaluate(effect_record_t* record) {
    s16 packed_index = record->timeline.frame;
    s32 packed_value;
    s32 offset = packed_index / 2;

    packed_value = g_effect_timing_curves->parent_timeline[offset];
    if (packed_index & 1) {
        packed_value >>= 4;
    }
    return packed_value & 0xf;
}

/* Time scale of the child timeline's frame record->timeline.frame: one of the
 * 4-bit values the effect file's timing-curve section packs two to a byte, low
 * nibble first. */
static s32 child_timing_curve_evaluate(effect_record_t* record) {
    s16 packed_index = record->timeline.frame;
    s32 offset = packed_index / 2;
    s32 packed_value = g_effect_timing_curves->child_timeline[offset];

    if (packed_index & 1) {
        packed_value >>= 4;
    }
    return packed_value & 0xf;
}

/* Look up a native overlay callback by its resident catalog index. */
static effect_callback_t native_callback_get(s16 callback_id) {
    return g_effect_native_callbacks[callback_id];
}

/* Read effect file effect_id's disc sector and byte length from g_disc_extents; a
 * length of 0 means the file is missing. */
static void disc_extent_load(s32 effect_id, s32* lba, s32* byte_length) {
    u32 stack_padding[2];

    *lba = g_disc_extents[effect_id].sector;
    *byte_length = g_disc_extents[effect_id].size;
}

/* Report an effect stub and return zero. */
s32 effect_report_0x320_return_zero(void) {
    system_report_stub(1, 0x320);
    return 0;
}

/* Report an effect stub. */
void effect_report_0x320_1(void) {
    system_report_stub(1, 0x320);
}

/* Calls the empty system_report_stub with (1, 0x320) from a 0x100-byte frame it
 * does not use. No code calls it. */
void effect_report_0x320_2(void) {
    s32 unused[0x40];
    system_report_stub(1, 0x320);
}

/* Seek the CD to the effect file of g_effect_id: the read request
 * has no destination, so the loader only seeks. An effect whose file has no size
 * is replaced by the Cure effect, as in effect_file_load_update's first
 * state, which requests the same file with a destination. Nothing in the game
 * calls it. */
void effect_file_seek_request(void) {
    s32 lba;
    s32 byte_length;

    disc_extent_load(g_effect_id, &lba, &byte_length);
    if (byte_length == 0) {
        g_effect_id = EFFECT_FILE_CURE;
        disc_extent_load(EFFECT_FILE_CURE, &lba, &byte_length);
    }
    file_bytes_read_request(lba, byte_length, 0);
}

/*
 * Effect-file load state machine: state 0 starts loading the ability's effect
 * file (falling back to effect 1 when its size is zero) 0x3500 bytes into the
 * event overlay area, state 1 polls the load, state 2 waits out the countdown
 * (60 frames when a secondary effect is active), finalizes the secondary
 * effect and lowers the music when the flags section sets EFFECT_FLAG_QUIET_MUSIC, and
 * state 3 waits for the secondary effect phase to end. Returns nonzero while
 * work remains. A load state outside 0-3 leaves `result` unset: the test then
 * reads whatever the caller left in $a0.
 */
s32 effect_file_load_update(void) {
    s32 result;
    s32 lba;
    s32 byte_length;
    u8* destination = g_heap_companion_overlay_load_address + 0x3500;

    if (g_effect_id >= 0) {
        switch (g_effect_load_state) {
        case EFFECT_LOAD_START:
            if (g_effect_secondary_charge_id != 0) {
                g_effect_load_countdown = 60;
            } else {
                g_effect_load_countdown = 0;
            }
            disc_extent_load(g_effect_id, &lba, &byte_length);
            if (byte_length == 0) {
                g_effect_id = EFFECT_FILE_CURE;
                disc_extent_load(EFFECT_FILE_CURE, &lba, &byte_length);
            }
            result = file_bytes_read_request(lba, byte_length, destination);
            if (result == 0) {
                g_effect_load_state = EFFECT_LOAD_READING;
                result = 1;
            }
            break;
        case EFFECT_LOAD_READING:
            result = file_load_state_get();
            if (result == 0) {
                g_effect_load_state = EFFECT_LOAD_COUNTDOWN;
                result = 1;
            }
            break;
        case EFFECT_LOAD_COUNTDOWN:
            if (g_effect_load_countdown != 0) {
                result = 1;
                break;
            }
            if (g_effect_secondary_charge_id != 0) {
                effect_secondary_finalize(g_effect_secondary_charge_id);
            }
            g_effect_load_state = EFFECT_LOAD_ENDING_SECONDARY;
            flags_section_pointer_init(g_effect_id);
            /* An `lw` of the section's leading 32-bit value, not a byte test. */
            if (g_effect_flags_section->flags & EFFECT_FLAG_QUIET_MUSIC) {
                music_volume_set(0x40, 0xF0);
            }
            result = 1;
            break;
        case EFFECT_LOAD_ENDING_SECONDARY:
            result = effect_secondary_phase_get(g_effect_secondary_charge_id) != EFFECT_SECONDARY_PHASE_ENDED;
            break;
        }
        if (g_effect_load_countdown > 0) {
            g_effect_load_countdown--;
        }
        return result != 0;
    }
    return 0;
}

/*
 * Select the effect file for an ability and start its charge secondary effect.
 *
 * `packed` carries the ability's charge animation set in its low 16 bits and
 * its element mask in its high 16 bits. `ability` is an ability id or, from
 * ABILITY_ID_COUNT up, one of the effects past the abilities. A thrown ball
 * (ABILITY_ID_THROW_BALL) takes the Fire, Ice or Bolt effect by the ball's
 * element; a ball of any other element stores no effect id, so
 * g_effect_id keeps the previous one. A negative table entry
 * means the ability has no effect file: only the two fixed secondary
 * animations for the equipment-break and Charge ability ranges are started,
 * and nothing is retained.
 */
void effect_ability_animation_set(u32 packed, s32 ability, const effect_context_t* source) {
    g_effect_load_state = EFFECT_LOAD_START;
    if (ability < ABILITY_ID_COUNT) {
        if (ability == ABILITY_ID_THROW_BALL) {
            s32 ball = effect_element_index_get(packed >> 16);
            if (ball == EFFECT_ELEMENT_INDEX_FIRE)
                g_effect_id = EFFECT_FILE_FIRE;
            else if (ball == EFFECT_ELEMENT_INDEX_WATER)
                g_effect_id = EFFECT_FILE_ICE;
            else if (ball == EFFECT_ELEMENT_INDEX_LIGHTNING)
                g_effect_id = EFFECT_FILE_BOLT;
        } else {
            g_effect_id = g_effect_ability_files[ability];
        }
    } else {
        /* Event IDs continue after the action/reaction ability rows. */
        g_effect_id = g_effect_ability_files[ability - ABILITY_ID_COUNT + ABILITY_ID_SUPPORT_FIRST];
    }

    if (g_effect_id >= 0) {
        g_effect_id &= EFFECT_ABILITY_FILE_MASK;
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_REQUESTED;
        if (g_effect_secondary_by_charge_animation[packed & 0xffff] != 0) {
            g_effect_secondary_charge_id
                = effect_secondary_init(g_effect_secondary_by_charge_animation[packed & 0xffff], packed >> 16, source);
        } else {
            g_effect_secondary_charge_id = 0;
        }
        g_effect_sound_call_counts[3] = 0;
        g_effect_sound_call_counts[2] = 0;
        g_effect_sound_call_counts[1] = 0;
        g_effect_sound_call_counts[0] = 0;
        g_effect_context = *source;
        g_effect_request_unread = 0;
        return;
    }

    if (ability >= ABILITY_ID_BATTLE_SKILL_HEAD_BREAK && ability <= ABILITY_ID_BATTLE_SKILL_MIND_BREAK)
        effect_secondary_init(EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK, 0, source);
    if ((ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST)
        || ability == ABILITY_ID_BASIC_SKILL_ACCUMULATE)
        effect_secondary_init(EFFECT_SECONDARY_ANIMATION_RED_ORBS, 0, source);
}

/* Returns 1 when the ability has no effect file (effect id 0xffff), 0 for
 * Vertical Jump8, all Charge abilities and Accumulate. The target's range
 * starts at Vertical Jump8 rather than Charge +1. */
u32 effect_ability_has_no_file(s32 ability_id) {
    u32 result;

    if (ability_id >= ABILITY_ID_JUMP_VERTICAL_8 && ability_id < ABILITY_ID_MATH_FIRST) {
        return 0;
    }
    if (ability_id == ABILITY_ID_BASIC_SKILL_ACCUMULATE) {
        result = 0;
    } else {
        result = g_effect_ability_files[ability_id] < 0;
    }
    return result;
}

/* 1 when the ability's entry in g_effect_ability_files carries
 * EFFECT_ABILITY_FLAG_ITEM. A negative entry (an ability without an effect
 * file) is complemented before the test. */
s32 effect_is_item_ability(s32 ability_id) {
    s32 entry = g_effect_ability_files[ability_id];
    if (entry < 0) {
        entry = ~entry;
    }
    return (entry & EFFECT_ABILITY_FLAG_ITEM) > 0;
}

/* Releases a requested effect: stage_update starts it on its
 * next frame. */
void effect_playback_start(void) {
    if (g_effect_playback_phase == EFFECT_PLAYBACK_PHASE_REQUESTED) {
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_INIT_PALETTE;
    }
}

/* Drops a requested effect that has not been released. */
void effect_playback_request_cancel(void) {
    if (g_effect_playback_phase == EFFECT_PLAYBACK_PHASE_REQUESTED) {
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
    }
}

/* The current effect playback phase (effect_playback_phase_e). */
s32 effect_playback_phase_get(void) {
    return g_effect_playback_phase;
}

/* Runs the loaded effect file one frame and returns 1 while it plays.
 *
 * Once effect_playback_start releases a requested effect, the file's two CLUT
 * rows and its texture go to VRAM; the memory from that last section of the
 * file on then becomes the effect heap, the section pointers are bound and
 * the root record starts the script. Each later frame runs every allocated
 * record's script until it yields, freeing the records whose scripts end, and
 * the effect is over when none is left. Phase 5 has no case and would return
 * `result` unset, but nothing stores 5 or RESTART_SCRIPT (6), so the restart
 * path is dead in retail. */
static s32 stage_update(void) {
    s32 result;
    s32 id = g_effect_allocated_record_head;
    s32 next;

    switch (g_effect_playback_phase) {
    case EFFECT_PLAYBACK_PHASE_IDLE:
        result = 0;
        break;
    case EFFECT_PLAYBACK_PHASE_REQUESTED:
        result = 1;
        break;
    case EFFECT_PLAYBACK_PHASE_INIT_PALETTE:
        battle_map_freeze();
        unit_view_hide_tile_depth();
        palette_init(g_effect_id);
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_START_SCRIPT;
        g_effect_script_opcode_05_value = 0;
        system_frame_pacing_reset();
        result = 1;
        break;
    case EFFECT_PLAYBACK_PHASE_START_SCRIPT:
        effect_heap_init(
            (effect_heap_node_t*)g_effect_palette_ptr, g_heap_battle_end_address - (u32)g_effect_palette_ptr);
        effect_subsystems_reset();
        effect_camera_tracks_seed();
        system_min_vsync_interval_clear();
        frame_data_pointer_set(g_effect_id);
        animation_sequences_pointer_set(g_effect_id);
        script_pointer_set(g_effect_id);
        animation_curves_bind(g_effect_id);
        emitters_bind(g_effect_id);
        timeline_sections_bind(g_effect_id);
        sound_section_load(g_effect_id);
        flags_section_pointer_init(g_effect_id);
        timing_curves_pointer_set(g_effect_id);
        if (g_effect_context.math_skill_flag != 0) {
            if (g_effect_flags_section->spawn_delay_override != 0) {
                g_effect_timeline->spawn_delay = g_effect_flags_section->spawn_delay_override;
            } else {
                g_effect_timeline->spawn_delay = 100;
            }
        }
        g_effect_root_record_index = script_instance_start(g_effect_script, 0, 0);
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_RUNNING;
        battle_map_effect_data_state_set(BATTLE_MAP_EFFECT_DATA_BEGIN);
        result = 1;
        break;
    case EFFECT_PLAYBACK_PHASE_RUNNING:
        g_effect_otag = gfx_battle_otag_get();
        effect_render_frame_start();
        while (id != 0) {
            next = g_effect_state_records[id].next_index;
            g_effect_current_record_index = id;
            g_effect_state_records[id].frame_counter++;
            if (script_block_run(id) == 0) {
                script_instance_free(id);
            }
            id = next;
        }
        if (g_effect_allocated_record_head != 0) {
            result = 1;
        } else {
            if (g_effect_flags_section->flags & EFFECT_FLAG_QUIET_MUSIC) {
                music_volume_restore(0x78);
            }
            if (g_effect_sound_section_ptr != 0) {
                suzuki_sound_remove_resource(g_effect_sound_section_ptr);
                g_effect_sound_section_ptr = 0;
            }
            battle_map_effect_data_state_set(BATTLE_MAP_EFFECT_DATA_END);
            battle_map_unfreeze();
            unit_view_show_tile_depth();
            system_min_vsync_interval_clear();
            g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
            result = 0;
        }
        break;
    case EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT:
        effect_heap_init(
            (effect_heap_node_t*)g_effect_palette_ptr, g_heap_battle_end_address - (u32)g_effect_palette_ptr);
        effect_subsystems_reset();
        frame_data_pointer_reset();
        animation_sequences_pointer_reset();
        g_effect_root_record_index = script_instance_start(g_effect_script, 0, 0);
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_RUNNING;
        return 1;
    }
    return result;
}

/* The effect system's frame: the effect file, the secondary effects and the
 * effect camera moves. Returns 1 while an effect of either kind runs. */
s32 effect_playback_update(void) {
    s32 stage_result = stage_update();
    s32 related_result = effect_secondary_update();

    effect_camera_update_tracks();

    if (stage_result != 0 || related_result != 0) {
        return 1;
    }
    return 0;
}

/* Chain state records 1-16 into the available list (record 0 is the sentinel), empty the allocated
 * list and set playback idle. */
void effect_script_instances_init(void) {
    s32 i;

    g_effect_state_records[0].next_index = 0;
    g_effect_available_record_head = 1;
    for (i = 1; i < EFFECT_STATE_RECORD_CAPACITY; i++) {
        g_effect_state_records[i].record_index = i;
        g_effect_state_records[i].next_index = i + 1;
        g_effect_state_records[i].last_native_callback = 0;
    }
    g_effect_state_records[i].next_index = 0;
    g_effect_state_records[i].record_index = i;
    g_effect_allocated_record_head = 0;
    g_effect_current_record_index = 0;
    g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
}

/* Allocate one effect record and link it into the in-use list. */
static s16 script_instance_alloc(void) {
    u16* available = &g_effect_available_record_head;
    s16* allocated;
    s16 index = *available;
    u16 next_available = g_effect_state_records[index].next_index;
    u16 next_allocated;

    allocated = &g_effect_allocated_record_head;
    next_allocated = *allocated;
    *available = next_available;
    g_effect_state_records[index].next_index = next_allocated;
    *allocated = index;
    g_effect_state_records[index].flags |= EFFECT_RECORD_FLAG_ALLOCATED;
    return index;
}

/* Tear down effect state record record_index and return it to the free list.
 *
 * Frees every work record on its list, releases the four handler work slots,
 * clears the spawning record's reference to it, then unlinks it from the
 * allocated list; the inverse of script_instance_alloc. The
 * separate `particle` copy keeps the call's work argument loaded at the loop
 * edges, as in the target, and the list-head pointers keep the target's
 * materialized global address. */
static void script_instance_free(s16 record_index) {
    effect_work_record_t* work;
    effect_work_record_t* particle;
    s16 head;
    s16 i;
    s16 parent;
    s16 cur;

    particle = work = g_effect_state_records[record_index].work_head;
    head = g_effect_allocated_record_head;
    /* Retail bug: work->next is read after the record has gone back to the free
     * chain, so it points at the old free head rather than the next record of
     * this list. The walk only ends correctly on an empty list (or a one-record
     * list with no free records), so an effect's records must be gone before
     * its script ends. */
    while (work != 0) {
        effect_particle_record_free(record_index, particle);
        particle = work = work->next;
    }
    for (i = 0; i < EFFECT_RECORD_CALLBACK_SLOT_COUNT; i++) {
        if (g_effect_state_records[record_index].work_slots[i] != 0) {
            effect_heap_free_block(g_effect_state_records[record_index].work_slots[i]);
            g_effect_state_records[record_index].work_slots[i] = 0;
        }
        g_effect_state_records[record_index].callbacks[i] = 0;
    }
    parent = g_effect_state_records[record_index].parent_index;
    if (parent != 0 && (g_effect_state_records[parent].flags & EFFECT_RECORD_FLAG_ALLOCATED)) {
        for (i = 0; i < EFFECT_SCRIPT_CHILD_SLOT_COUNT; i++) {
            if (g_effect_state_records[parent].child_indices[i] == record_index) {
                g_effect_state_records[parent].child_indices[i] = 0;
            }
        }
    }
    if (head == record_index) {
        u16* available;

        g_effect_allocated_record_head = g_effect_state_records[record_index].next_index;
        available = &g_effect_available_record_head;
        g_effect_state_records[record_index].next_index = *available;
        *available = head;
    } else {
        u16* available_after_unlink;

        while ((cur = g_effect_state_records[head].next_index) != record_index) {
            head = cur;
        }
        g_effect_state_records[head].next_index = g_effect_state_records[cur].next_index;
        available_after_unlink = &g_effect_available_record_head;
        g_effect_state_records[cur].next_index = *available_after_unlink;
        *available_after_unlink = cur;
    }
    g_effect_state_records[record_index].flags &= ~EFFECT_RECORD_FLAG_ALLOCATED;
}

/* Clears a record's child-timeline state and sets the effect target it plays
 * on. */
static void child_timeline_reset(effect_record_t* record, s16 target_index) {
    s32 index;

    for (index = 0; index < EFFECT_CHILD_TRACK_RESET_SLOT_COUNT; index++) {
        record->timeline.state.child.step[index] = 0;
        record->timeline.state.child.remaining.reset_slots[index] = 0;
        record->timeline.state.child.keyframe.reset_slots[index] = 0;
    }
    record->timeline.frame = 0;
    record->timeline.target_index = target_index;
}

/* Clear all 26 16-bit values in each bank, including the extra reset slots. */
static void parent_timeline_reset(effect_record_t* record) {
    s32 index;

    for (index = 0; index < EFFECT_PARENT_TRACK_RESET_SLOT_COUNT; index++) {
        record->timeline.state.parent.step.reset_slots[index] = 0;
        record->timeline.state.parent.remaining.reset_slots[index] = 0;
        record->timeline.state.parent.keyframe.reset_slots[index] = 0;
    }
    record->timeline.state.parent.spawn_countdown = 0;
    record->timeline.frame = 0;
    record->timeline.state.parent.spawned_count = 0;
}

/* Allocate an effect state record for a script and reset its per-handler state.
 * Returns the record index, or 0 when none is available. */
static s16 script_instance_start(const u8* script, s16 start_pc, s16 parent_index) {
    s16 index = script_instance_alloc();
    s16 i;
    effect_record_t* record;

    if (index == 0) {
        return 0;
    }
    record = &g_effect_state_records[index];
    record->parent_index = parent_index;
    record->script = script;
    record->pc = start_pc;
    for (i = 0; i < EFFECT_SCRIPT_COUNTER_COUNT; i++) {
        record->counters[i] = 0;
        record->child_indices[i] = 0;
    }
    for (i = 0; i < EFFECT_RECORD_CALLBACK_SLOT_COUNT; i++) {
        record->callbacks[i] = 0;
        record->callback_phase[i] = EFFECT_CALLBACK_PHASE_IDLE;
        record->work_slots[i] = 0;
    }
    record->flags = EFFECT_RECORD_FLAG_ALLOCATED;
    record->work_head = 0;
    record->work_count = 0;
    record->frame_counter = -1;
    child_timeline_reset(record, 0);
    parent_timeline_reset(record);
    return index;
}

/* Handler for opcodes 2e-77: calls the empty stub reporter and returns no defined value. In retail
 * $v0 still holds this handler's address, which script_block_run treats as 'continue'
 * without the pc moving, so an invalid opcode hangs the script loop. */
effect_script_result_e effect_script_opcode_2e_invalid(effect_record_t* unused_record) {
    system_report_stub(1, 0x22);
    /* Retail leaves the dispatch result undefined after an invalid opcode. */
}

/* Opcode 00: jump to the script offset in the second 16-bit value and end the
 * record's run for this frame. */
effect_script_result_e effect_script_opcode_00_jump_and_yield(effect_record_t* record) {
    record->pc = effect_get_current_script_instruction(record)->immediate.value;
    return EFFECT_SCRIPT_RESULT_YIELD;
}

/* Opcode 01: jump to the script offset in the second 16-bit value. */
effect_script_result_e effect_script_opcode_01_jump(effect_record_t* record) {
    record->pc = effect_get_current_script_instruction(record)->immediate.value;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 02: start a child record at the script offset in the second 16-bit value, parented to the
 * running record, and keep its index in the child slot named by the high nibble of the second
 * byte; slots above 3 are not kept. */
effect_script_result_e effect_script_opcode_02_spawn_child(effect_record_t* record) {
    effect_script_parameters_t parameters;
    s16 slot;
    s16 child;
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    parameters = instruction->header.parameters;
    slot = parameters.child.slot;
    child = script_instance_start(record->script, instruction->immediate.value, g_effect_current_record_index);
    if (slot < EFFECT_SCRIPT_CHILD_SLOT_COUNT) {
        record->child_indices[slot] = child;
    }
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 03: free the child record in the child slot named by bits 4-5 of
 * the instruction's second byte. */
effect_script_result_e effect_script_opcode_03_free_child(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    s16 child = record->child_indices[parameters.free_child.slot];
    if (child != 0)
        script_instance_free(child);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 04: end the script; script_block_run's caller then frees the
 * record. */
effect_script_result_e effect_script_opcode_04_end(effect_record_t* unused_record) {
    return EFFECT_SCRIPT_RESULT_ABORT;
}

/* Opcode 05: store the high nibble of the instruction's second byte.
 * Retail clears this global before the script runs but never reads it. */
effect_script_result_e effect_script_opcode_05_store_high_nibble(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    effect_script_parameters_t parameters = instruction->header.parameters;
    g_effect_script_opcode_05_value = parameters.opcode_05.high_nibble;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 06: put the native callback whose catalog id is the second 16-bit value in the
 * callback slot named by the top six bits of the instruction's second byte.
 * A particle keyframe whose action names the slot binds it to the record. */
effect_script_result_e effect_script_opcode_06_register_native_callback(effect_record_t* record) {
    const effect_script_instruction_t* command = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = command->header.parameters;
    u32 slot = parameters.callback.slot;
    g_effect_callback_slots[slot] = native_callback_get(command->immediate.value);
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 07: call the native callback whose catalog id is the second 16-bit value with
 * the record index and the top six bits of the instruction's second byte as
 * its callback slot, and remember it in the record. */
effect_script_result_e effect_script_opcode_07_call_native_callback(effect_record_t* record) {
    const effect_script_instruction_t* command = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = command->header.parameters;
    u32 slot = parameters.callback.slot;
    effect_callback_t handler = native_callback_get(command->immediate.value);
    record->last_native_callback = handler;
    handler(record->record_index, slot, 0, 0);
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 08: move the camera to the position in the three 16-bit values that
 * follow (x, y, z in world units). */
effect_script_result_e effect_script_opcode_08_set_camera_position(effect_record_t* record) {
    VECTOR coords;

    setVector(&coords, effect_get_current_script_instruction(record)->vector.x * ONE,
        effect_get_current_script_instruction(record)->vector.y * ONE,
        effect_get_current_script_instruction(record)->vector.z * ONE);
    camera_real_coords_set(&coords);
    record->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 09: save the camera position (opcode 0a restores it). */
effect_script_result_e effect_script_opcode_09_save_camera_position(effect_record_t* record) {
    camera_real_coords_save();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0a: restore the camera position opcode 09 saved. */
effect_script_result_e effect_script_opcode_0a_restore_camera_position(effect_record_t* record) {
    camera_real_coords_restore();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0b: set the camera rotation from the three 16-bit values that follow. */
effect_script_result_e effect_script_opcode_0b_set_camera_rotation(effect_record_t* record) {
    SVECTOR rotation;

    setVector(&rotation, effect_get_current_script_instruction(record)->vector.x,
        effect_get_current_script_instruction(record)->vector.y,
        effect_get_current_script_instruction(record)->vector.z);
    camera_rotation_set(&rotation);
    record->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0c: save the camera rotation (opcode 0d restores it). */
effect_script_result_e effect_script_opcode_0c_save_camera_rotation(effect_record_t* record) {
    camera_rotation_save();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0d: restore the camera rotation opcode 0c saved. */
effect_script_result_e effect_script_opcode_0d_restore_camera_rotation(effect_record_t* record) {
    camera_rotation_restore();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0e: set the camera zoom from the three 16-bit values that follow (ONE is
 * no zoom). */
effect_script_result_e effect_script_opcode_0e_set_camera_zoom(effect_record_t* record) {
    VECTOR zoom;

    setVector(&zoom, effect_get_current_script_instruction(record)->vector.x,
        effect_get_current_script_instruction(record)->vector.y,
        effect_get_current_script_instruction(record)->vector.z);
    camera_zoom_set(&zoom);
    record->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0f: save the camera zoom (opcode 24 restores it). */
effect_script_result_e effect_script_opcode_0f_save_camera_zoom(effect_record_t* record) {
    camera_zoom_save();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 10: set the counter selected by the top two bits of the
 * instruction's second byte to the second 16-bit value. */
effect_script_result_e effect_script_opcode_10_set_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    parameters = instruction->header.parameters;
    record->counters[parameters.counter.slot] = instruction->immediate.value;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 11: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte equals the second 16-bit value;
 * otherwise step over the 6-byte instruction. */
effect_script_result_e effect_script_opcode_11_jump_if_counter_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] == instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 12: jump to the offset in the third 16-bit value when the counter selected by the top two
 * bits of the second byte is at least the second 16-bit value; otherwise step over the 6-byte
 * instruction. */
effect_script_result_e effect_script_opcode_12_jump_if_counter_at_least(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] >= instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 13: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte exceeds the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_script_opcode_13_jump_if_counter_greater(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] > instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 14: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte is at most the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_script_opcode_14_jump_if_counter_at_most(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] <= instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 15: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte is below the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_script_opcode_15_jump_if_counter_less(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] < instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 16: jump to the offset in the third 16-bit value when the record has exactly as many
 * particles as the second 16-bit value; otherwise step over the 6-byte instruction. Retail scripts
 * test 0 to end once the last particle is gone. */
effect_script_result_e effect_script_opcode_16_jump_if_work_count_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (record->work_count == instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 17: jump to the offset in the third 16-bit value when the record has
 * more particles than the second 16-bit value says; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_script_opcode_17_jump_if_work_count_greater(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (instruction->branch.comparison < record->work_count) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 18: jump to the offset in the third 16-bit value when the record has
 * fewer particles than the second 16-bit value says; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_script_opcode_18_jump_if_work_count_less(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (record->work_count < instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 19: jump to the offset in the third 16-bit value when the number of
 * occupied child slots equals the second 16-bit value; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_script_opcode_19_jump_if_child_count_equal(effect_record_t* record) {
    s16 i;
    s16 count;
    s16 target = effect_get_current_script_instruction(record)->branch.comparison;

    count = 0;
    for (i = 0; i < EFFECT_SCRIPT_CHILD_SLOT_COUNT; i++) {
        if (record->child_indices[i] != 0) {
            count++;
        }
    }
    if (count == target) {
        record->pc = effect_get_current_script_instruction(record)->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1a: jump to the offset in the second 16-bit value when the child slot named by the second
 * byte's high nibble holds a record; otherwise step over the 4-byte instruction. */
effect_script_result_e effect_script_opcode_1a_jump_if_child_active(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    parameters = instruction->header.parameters;
    if (record->child_indices[parameters.child.slot] != 0) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1b: jump to the offset in the second 16-bit value when the child slot
 * named by the second byte's high nibble is empty; otherwise step over the
 * 4-byte instruction. */
effect_script_result_e effect_script_opcode_1b_jump_if_child_inactive(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    parameters = instruction->header.parameters;
    if (record->child_indices[parameters.child.slot] == 0) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1c: jump to the offset in the third 16-bit value when the counter
 * selected by the second byte's top two bits differs from the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_script_opcode_1c_jump_if_counter_not_equal(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] != instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1d: jump to the offset in the second 16-bit value once the record's
 * timeline frame reaches the child timeline's duration; otherwise step over
 * the 4-byte instruction. */
effect_script_result_e effect_script_opcode_1d_jump_after_child_timeline_duration(effect_record_t* record) {
    if (record->timeline.frame >= g_effect_child_timeline->duration) {
        const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1e: jump to the offset in the second 16-bit value once the record's
 * timeline frame reaches the end of the parent timeline (phase 1, a spawn
 * delay for each target after the first, the last child's duration and phase
 * 2); otherwise step over the 4-byte instruction. */
effect_script_result_e effect_script_opcode_1e_jump_after_total_duration(effect_record_t* record) {
    effect_timeline_t* timing = g_effect_timeline;

    if (record->timeline.frame >= timing->phase1_duration + timing->phase2_duration
            + (g_effect_context.target_count - 1) * timing->spawn_delay + timing->child.duration) {
        const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

        record->pc = instruction->immediate.value;
    } else {
        record->pc += 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1f: jump when the target count equals the header's six-bit selector.
 * Retail scripts use zero to end an effect with no targets. */
effect_script_result_e effect_script_opcode_1f_jump_if_target_count_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (g_effect_context.target_count == parameters.targets.count) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc += 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 20: add 1 to the counter selected by the top two bits of the
 * instruction's second byte. */
effect_script_result_e effect_script_opcode_20_increment_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;

    parameters = effect_get_current_script_instruction(record)->header.parameters;
    record->counters[parameters.counter.slot] += 1;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 21: subtract 1 from the counter selected by the top two bits of the
 * instruction's second byte. */
effect_script_result_e effect_script_opcode_21_decrement_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;

    parameters = effect_get_current_script_instruction(record)->header.parameters;
    record->counters[parameters.counter.slot] -= 1;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 22: add the second 16-bit value to the counter selected by the top two
 * bits of the instruction's second byte. */
effect_script_result_e effect_script_opcode_22_add_immediate_to_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    u16 amount;

    parameters = instruction->header.parameters;
    amount = instruction->immediate.value;
    record->counters[parameters.counter.slot] += amount;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 23: subtract the second 16-bit value from the counter selected by the
 * top two bits of the instruction's second byte. */
effect_script_result_e effect_script_opcode_23_subtract_immediate_from_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    u16 amount;

    parameters = instruction->header.parameters;
    amount = instruction->immediate.value;
    record->counters[parameters.counter.slot] -= amount;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 24: restore the camera zoom opcode 0f saved. */
effect_script_result_e effect_script_opcode_24_restore_camera_zoom(effect_record_t* record) {
    camera_zoom_restore();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/*
 * Effect-script opcode 0x25: update every particle on the record's work list.
 *
 * Loads the camera matrix into the GTE and integrates the list's motion. Each
 * particle then ends its life once it is within a non-zero homing threshold
 * of its anchor on all three axes, runs its kind handler, spawns its mid-life
 * child and advances its curve frame. When its countdown (or, for lifetime
 * -1, its animation) ends it spawns its death child and is freed.
 *
 * The loop's locals are declared in its body: the block note ahead of the
 * exit test stops GCC from rolling the test to the loop end, which keeps the
 * target's top-tested loop. The death child is tested with an if/else chain
 * whose arms are identical (cross-jumping merges them): `mode == 1 || mode ==
 * 2` is folded into an unsigned range test that the target does not contain.
 */
effect_script_result_e effect_script_opcode_25_update_all_particles(effect_record_t* record) {
    effect_work_record_t* work = record->work_head;

    gte_SetRotMatrix(&g_camera_matrix);
    gte_SetTransMatrix(&g_camera_matrix);
    effect_particle_integrate_motion(record->work_head);

    for (;;) {
        effect_work_record_t* node;
        s32 threshold;
        s32 delta;

        if (work == 0) {
            break;
        }
        node = work;
        threshold
            = (node->flags & EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_MASK) >> EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_SHIFT;
        work = node->next;
        if (threshold != 0) {
            delta = (node->motion.position.x >> 12) - node->target.x;
            if (delta < threshold && -threshold < delta) {
                delta = (node->motion.position.y >> 12) - node->target.y;
                if (delta < threshold && -threshold < delta) {
                    delta = (node->motion.position.z >> 12) - node->target.z;
                    if (delta < threshold && -threshold < delta) {
                        node->lifetime = EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN;
                    }
                }
            }
        }

        g_effect_particle_render_handlers[(u16)node->kind_flags >> 12](record, node);

        if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) == EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_1
            || (node->flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) == EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_2) {
            effect_emitter_particles_spawn(
                record->record_index, record->frame_counter, node->child_emitter_mid_life, node);
        }

        node->curve_frame++;
        if (node->curve_frame == EFFECT_ANIMATION_CURVE_FRAMES) {
            node->curve_frame = 0;
        }

        if (node->lifetime == EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN) {
            if (node->data->frame_timer != 0) {
                continue;
            }
        } else if (--node->lifetime != 0) {
            continue;
        }

        if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) == EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_1) {
            effect_emitter_particles_spawn(
                record->record_index, record->frame_counter, node->child_emitter_on_death, node);
        } else if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK)
            == EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_2) {
            effect_emitter_particles_spawn(
                record->record_index, record->frame_counter, node->child_emitter_on_death, node);
        }
        effect_particle_record_free(record->record_index, node);
    }

    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 26: spawn this frame's particles of the emitter named by bits 10-13
 * of the 16-bit instruction header, at the record's frame counter. */
effect_script_result_e effect_script_opcode_26_spawn_emitter_particles(effect_record_t* record) {
    /* A 16-bit header cache preserves the address operands; a direct field read changes them. */
    u16 header;

    header = effect_get_current_script_header(record);

    effect_emitter_particles_spawn(record->record_index, record->frame_counter, (header >> 10) & 0xf, 0);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 27: take the gravity vector and the inertia threshold from the
 * effect file's particle-system section. */
effect_script_result_e effect_script_opcode_27_init_particle_physics(effect_record_t* record) {
    effect_gravity_set(&g_effect_geometry_table->gravity);
    effect_inertia_threshold_set(g_effect_geometry_table->inertia_threshold);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/*
 * Dispatch one keyframe's action flags to a single effect target: bits 4-6
 * and 8-15 trigger unit reactions (bit 3 selects the caster entry), bits 0-2
 * register a callback slot.
 */
static void keyframe_actions_run(u16 flags, u8 target, effect_record_t* state) {
    s32 runtime_id;
    u32 animation;
    u16 slot;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        runtime_id = -1;
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id;
        }
    } else {
        runtime_id = -1;
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[target].data.unit.runtime_id;
        }
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) && runtime_id != -1) {
        result_runtime_display_prepare(runtime_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) && runtime_id != -1) {
        unit_runtime_display_update(runtime_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) && runtime_id != -1) {
        action_start_target_response_by_runtime_id(runtime_id);
    }
    animation = (flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0 && runtime_id != -1) {
        animation_runtime_sequence_set(runtime_id, animation);
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_INIT;
        state->callbacks[slot] = g_effect_callback_slots[slot];
    }
}

/* Pick the sound id effect sound channel channel plays next, by the channel's mode and how many
 * times it has been used; each call counts one use. A mode above 4 has no case: retail then
 * returns whatever the caller left in $a2. */
static s32 sound_id_select(s32 channel) {
    s32 count = g_effect_sound_call_counts[channel]++;
    s32 sound;

    switch (g_effect_flags_section->channels[channel].mode) {
    case EFFECT_SOUND_MODE_FIRST:
        sound = g_effect_flags_section->channels[channel].sound_ids[0];
        break;
    case EFFECT_SOUND_MODE_ALTERNATE:
        sound = g_effect_flags_section->channels[channel].sound_ids[count & 1];
        break;
    case EFFECT_SOUND_MODE_FIRST_THEN_SECOND:
        if (count == 0) {
            sound = g_effect_flags_section->channels[channel].sound_ids[0];
        } else {
            sound = g_effect_flags_section->channels[channel].sound_ids[1];
        }
        break;
    case EFFECT_SOUND_MODE_FIRST_THEN_ALTERNATE:
        if (count != 0) {
            sound = g_effect_flags_section->channels[channel].sound_ids[1 + (count & 1)];
        } else {
            sound = g_effect_flags_section->channels[channel].sound_ids[0];
        }
        break;
    case EFFECT_SOUND_MODE_CYCLE:
        sound = g_effect_flags_section->channels[channel].sound_ids[count % 3];
        break;
    }
    return sound;
}

/* Opcode 28: advance the record's child timeline one frame for its target: the particle channels,
 * the surroundings, caster, target and screen color tracks and the sound tracks. The root record
 * also drives the camera keyframes, and EFFECT_FLAG_TIMING_CURVE_CHILD sets the time scale. A
 * reflected hit instead starts the Reflect animation on a unit target and jumps to the child
 * timeline's end. */
effect_script_result_e effect_script_opcode_28_step_child_timeline(effect_record_t* record) {
    battle_map_background_gradient_colors_t colors;
    effect_context_t init;
    effect_record_timeline_t* timeline;
    effect_keyframe_table_t* table;
    effect_palette_track_t* track;
    effect_background_track_t* screen;
    effect_child_sound_track_t* sound;
    s32 i;
    s16 k;
    s16 surroundings_keyframe;
    s16 caster_keyframe;
    s16 target_keyframe;
    s16 screen_keyframe;
    s16 sound_keyframe;
    s32 index;
    s32 preset;
    s32 sel;
    u16* actions;
    u16 slot;
    s16 step;
    s16 red;
    s16 green;
    s16 blue;
    u8 target;
    s32 mode;
    s32 kind;
    s32 sound_index;

    target = record->timeline.target_index;
    timeline = &record->timeline;

    if (g_effect_context.targets[target].data.unit.result_animation != EFFECT_HIT_REFLECTED) {
        for (i = 0; i < EFFECT_PARTICLE_CHANNEL_COUNT; i++) {
            table = &g_effect_child_timeline->particle[i];
            if (timeline->state.child.remaining.tracks.particle[i] == 0) {
                /* Keyframe 0 is the start: enter keyframe 1 and run its actions. */
                if (timeline->state.child.keyframe.tracks.particle[i] == 0) {
                    timeline->state.child.remaining.tracks.particle[i] = table->frame_start[1] - table->frame_start[0];
                    timeline->state.child.step[i] = 0;
                    timeline->state.child.keyframe.tracks.particle[i]++;
                    keyframe_actions_run(table->action[1], target, record);
                }
                /* Pass through keyframes of zero length, running each one's actions. */
                while (timeline->state.child.remaining.tracks.particle[i] == 0) {
                    k = timeline->state.child.keyframe.tracks.particle[i];
                    timeline->state.child.keyframe.tracks.particle[i] = k + 1;
                    timeline->state.child.remaining.tracks.particle[i]
                        = table->frame_start[k + 1] - table->frame_start[k];
                    timeline->state.child.step[i] = 0;
                    keyframe_actions_run(table->action[k + 1], target, record);
                }
            }

            /* emitter holds the emitter index plus one; 0 means the keyframe spawns nothing. */
            index = table->emitter[timeline->state.child.keyframe.tracks.particle[i] - 1];
            if (index != 0) {
                step = timeline->state.child.step[i]++;
                actions = table->action;
                sel = actions[timeline->state.child.keyframe.tracks.particle[i]] & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK;
                if (sel != 0) {
                    slot = sel - 1;
                    /* With two frames left in the keyframe, the callback runs in its destroy phase. */
                    if (timeline->state.child.remaining.tracks.particle[i] == 2) {
                        record->callback_phase[slot] = EFFECT_CALLBACK_PHASE_DESTROY;
                    }
                    record->callbacks[slot](record->record_index, slot, index - 1, step);
                } else {
                    effect_emitter_particles_spawn(record->record_index, step, index - 1, 0);
                }
            }

            timeline->state.child.remaining.tracks.particle[i]--;
        }

        track = &g_effect_child_timeline->surroundings;
        surroundings_keyframe = timeline->state.child.keyframe.tracks.surroundings;
        if (surroundings_keyframe < track->count - 1) {
            if (timeline->state.child.remaining.tracks.surroundings == 0) {
                timeline->state.child.keyframe.tracks.surroundings = surroundings_keyframe + 1;
                index = surroundings_keyframe;
                /* Track durations count units of 8 frames; 0 still takes one frame. */
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                timeline->state.child.remaining.tracks.surroundings = i;
                preset = track->preset[index];
                if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                    red = track->color[index].r;
                    green = track->color[index].g;
                    blue = track->color[index].b;
                    battle_map_color_field(
                        preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index], red, green, blue);
                    for (i = 0; i < g_effect_context.spectator_count; i++) {
                        sprite_palette_apply_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], g_effect_context.spectator_ids[i], red, green, blue);
                    }
                    /* The darkness color has 8-bit channels; the offsets are 5-bit palette steps. */
                    if (!(g_weather_flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
                        battle_map_darkness_start_blend(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], red * 8, green * 8, blue * 8);
                    }
                }
            }
            timeline->state.child.remaining.tracks.surroundings--;
        }

        track = &g_effect_child_timeline->caster;
        caster_keyframe = timeline->state.child.keyframe.tracks.caster;
        if (caster_keyframe < track->count - 1) {
            if (timeline->state.child.remaining.tracks.caster == 0) {
                timeline->state.child.keyframe.tracks.caster = caster_keyframe + 1;
                index = caster_keyframe;
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                timeline->state.child.remaining.tracks.caster = i;
                preset = track->preset[index];
                if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                    red = track->color[index].r;
                    green = track->color[index].g;
                    blue = track->color[index].b;
                    if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
                        sprite_palette_apply_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id,
                            red, green, blue);
                    }
                }
            }
            timeline->state.child.remaining.tracks.caster--;
        }

        track = &g_effect_child_timeline->target;
        target_keyframe = timeline->state.child.keyframe.tracks.target;
        if (target_keyframe < track->count - 1) {
            if (timeline->state.child.remaining.tracks.target == 0) {
                timeline->state.child.keyframe.tracks.target = target_keyframe + 1;
                index = target_keyframe;
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                timeline->state.child.remaining.tracks.target = i;
                preset = track->preset[index];
                if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                    red = track->color[index].r;
                    green = track->color[index].g;
                    blue = track->color[index].b;
                    if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
                        sprite_palette_apply_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], g_effect_context.targets[target].data.unit.runtime_id, red, green,
                            blue);
                    }
                }
            }
            timeline->state.child.remaining.tracks.target--;
        }

        screen = &g_effect_child_timeline->screen;
        screen_keyframe = timeline->state.child.keyframe.tracks.screen;
        if (screen_keyframe < screen->count - 1) {
            if (timeline->state.child.remaining.tracks.screen == 0) {
                timeline->state.child.keyframe.tracks.screen = screen_keyframe + 1;
                index = screen_keyframe;
                i = screen->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                timeline->state.child.remaining.tracks.screen = i;
                preset = screen->preset[index];
                mode = preset & EFFECT_COLOR_KEYFRAME_MODE_MASK;
                if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                    red = screen->color[index].r * 2;
                    green = screen->color[index].g * 2;
                    blue = screen->color[index].b * 2;
                    battle_map_background_modify(mode, screen->duration[index], red, green, blue);
                    /* Unlike the surroundings track, this one moves the darkness only when weather is ignored. */
                    if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                        battle_map_darkness_start_blend(mode, screen->duration[index], red, green, blue);
                    }
                } else {
                    colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r = screen->color[index].r;
                    colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g = screen->color[index].g;
                    colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b = screen->color[index].b;
                    colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r = screen->second_color[index].r;
                    colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g = screen->second_color[index].g;
                    colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b = screen->second_color[index].b;
                    battle_map_background_set(screen->duration[index], &colors);
                    if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                        battle_map_darkness_set(
                            screen->duration[index], &colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP]);
                    }
                }
            }
            timeline->state.child.remaining.tracks.screen--;
        }

        for (i = 0; i < EFFECT_SOUND_TRACK_COUNT; i++) {
            sound = &g_effect_child_timeline->sound[i];
            sound_keyframe = timeline->state.child.keyframe.tracks.sound[i];
            if (sound_keyframe < sound->count) {
                if (timeline->state.child.remaining.tracks.sound[i] == 0) {
                    timeline->state.child.keyframe.tracks.sound[i] = sound_keyframe + 1;
                    index = sound_keyframe;
                    timeline->state.child.remaining.tracks.sound[i] = sound->duration[index];
                    kind = sound->channel[index];
                    /* Channel values 0 and 1 play nothing; 2 and up pick effect sound channel kind - 2. */
                    if (kind >= 2) {
                        sound_index = sound_id_select(kind - 2);
                        if (sound_index != 0) {
                            suzuki_sound_play_sfx(sound_index + g_effect_sound_data_base);
                        }
                    }
                }
                timeline->state.child.remaining.tracks.sound[i]--;
            }
        }

        if (record->parent_index == 0) {
            effect_camera_start_child_keyframe_moves(timeline->frame);
        }
        if (g_effect_flags_section->flags & EFFECT_FLAG_TIMING_CURVE_CHILD) {
            system_min_vsync_interval_set(child_timing_curve_evaluate(record));
        }
        timeline->frame++;
    } else {
        /* A reflected hit shows Reflect on a unit target and skips to the child timeline's end. */
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            init.targets[EFFECT_CASTER_ENTRY] = g_effect_context.targets[EFFECT_CASTER_ENTRY];
            init.targets[0] = g_effect_context.targets[target];
            effect_secondary_init(EFFECT_SECONDARY_ANIMATION_REFLECT, g_effect_timeline->spawn_delay, &init);
        }
        record->timeline.frame = g_effect_child_timeline->duration;
    }

    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/*
 * Dispatch one keyframe's action flags to every live effect target: bits 4-6
 * and 8-15 trigger unit reactions, and bit 3 restricts each of them to the
 * caster entry instead of the whole target list; bits 0-2 register a callback
 * slot.
 */
static void keyframe_actions_all_targets_run(u16 flags, effect_record_t* state) {
    s32 runtime_id;
    u16 animation;
    u16 slot;
    s32 i;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        runtime_id = -1;
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id;
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                result_runtime_display_prepare(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    result_runtime_display_prepare(g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                unit_runtime_display_update(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    unit_runtime_display_update(g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                action_start_target_response_by_runtime_id(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    action_start_target_response_by_runtime_id(g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    animation = (flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                animation_runtime_sequence_set(runtime_id, animation);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    animation_runtime_sequence_set(g_effect_context.targets[i].data.unit.runtime_id, animation);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_INIT;
        state->callbacks[slot] = g_effect_callback_slots[slot];
    }
}

/* Advances one particle channel of the parent timeline by a frame: enters the
 * keyframes that have come due, running each one's 16-bit action value for every
 * effect target, then spawns the current keyframe's emitter, through the
 * handler of the action's callback slot when it names one. The handler is
 * told to destroy its work on the call made with two frames of the keyframe
 * left. index, remaining and step are the channel's keyframe, the frames left
 * in it and the frames it has run. */
static void emitter_step(
    effect_record_t* state, effect_keyframe_table_t* table, s16* index, s16* remaining, s16* step) {
    s16 k;
    s32 sel;
    u16* actions;
    u16 slot;
    s32 emitter;

    if (table->last_index >= *index) {
        while (*remaining == 0) {
            k = *index;
            *index = k + 1;
            /* Integer-first pointer addition preserves the native shared-address operands. */
            keyframe_actions_all_targets_run(*(k + 1 + table->action), state);
            *remaining = table->frame_start[k + 1] - table->frame_start[k];
            *step = 0;
            if (table->last_index < *index) {
                return;
            }
        }
        if (table->last_index >= *index) {
            emitter = table->emitter[*index - 1];
            if (emitter != 0) {
                actions = table->action;
                sel = actions[*index] & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK;
                slot = sel - 1;
                if (sel != 0) {
                    if (*remaining == 2) {
                        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_DESTROY;
                    }
                    state->callbacks[slot](state->record_index, slot, emitter - 1, *step);
                } else {
                    effect_emitter_particles_spawn(state->record_index, *step, emitter - 1, 0);
                }
                *step = *step + 1;
            }
            *remaining = *remaining - 1;
        }
    }
}

/* Advance the surroundings color track one frame.
 *
 * When an APPLY keyframe starts, its color is applied to the field, to the
 * palette of every spectator unit (those that are neither a target nor the
 * caster) and, unless the map ignores weather, to the darkness color (offsets
 * scaled to 8-bit channels). The
 * loop counter reuses the frame-count variable, which keeps both in $s0 as in
 * the target. */
static void surroundings_palette_track_advance(effect_palette_track_t* track, s16* keyframe, s16* remaining_frames) {
    s16 current;
    s32 index;
    s32 i;
    s32 preset;
    s16 red;
    s16 green;
    s16 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            i = track->duration[index];
            if (i != 0) {
                i *= 8;
            } else {
                i = 1;
            }
            *remaining_frames = i;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                red = track->color[index].r;
                green = track->color[index].g;
                blue = track->color[index].b;
                battle_map_color_field(
                    preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index], red, green, blue);
                for (i = 0; i < g_effect_context.spectator_count; i++) {
                    sprite_palette_apply_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                        track->duration[index], g_effect_context.spectator_ids[i], red, green, blue);
                }
                if (!(g_weather_flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
                    battle_map_darkness_start_blend(
                        preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index], red * 8, green * 8, blue * 8);
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}

/* Advance the caster color track one frame: an APPLY keyframe recolors the
 * caster. */
static void caster_palette_track_advance(
    effect_palette_track_t* track, s16* keyframe, s16* remaining_frames, s32 caster_runtime_id) {
    s16 current;
    s32 index;
    s32 timer;
    s32 preset;
    s8 red;
    s8 green;
    s8 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            timer = track->duration[index];
            if (timer != 0) {
                timer *= 8;
            } else {
                timer = 1;
            }
            *remaining_frames = timer;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                red = track->color[index].r;
                green = track->color[index].g;
                blue = track->color[index].b;
                sprite_palette_apply_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                    track->duration[index], caster_runtime_id, red, green, blue);
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}

/* Advance the target color track of the parent timeline one frame: an APPLY
 * keyframe recolors every unit target of the effect.
 *
 * The frame timer and the loop counter are separate locals here: the target
 * keeps the timer in $v0 and the counter in $s1. */
static void target_palette_track_advance(effect_palette_track_t* track, s16* keyframe, s16* remaining_frames) {
    s16 current;
    s32 index;
    s32 i;
    s32 timer;
    s32 preset;
    s16 red;
    s16 green;
    s16 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            timer = track->duration[index];
            if (timer != 0) {
                timer *= 8;
            } else {
                timer = 1;
            }
            *remaining_frames = timer;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                red = track->color[index].r;
                green = track->color[index].g;
                blue = track->color[index].b;
                for (i = 0; i < g_effect_context.target_count; i++) {
                    if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                        sprite_palette_apply_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], g_effect_context.targets[i].data.unit.runtime_id, red, green, blue);
                    }
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}

/* Advance the screen color track one frame.
 *
 * A starting APPLY keyframe modulates the background gradient by its doubled
 * color (and the darkness color when the map ignores weather); otherwise both
 * colors become the new gradient stops, with the first also driving the
 * darkness color when the map ignores weather. */
static void map_background_track_advance(effect_background_track_t* track, s16* keyframe, s16* remaining_frames) {
    battle_map_background_gradient_colors_t colors;
    s16 current;
    s32 index;
    s16 duration;
    u8 preset;
    s32 mode;
    s32 red;
    s32 green;
    s32 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            duration = track->duration[index];
            if (duration != 0) {
                duration *= 8;
            } else {
                duration = 1;
            }
            *remaining_frames = duration;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                mode = preset & EFFECT_COLOR_KEYFRAME_MODE_MASK;
                red = track->color[index].r * 2;
                green = track->color[index].g * 2;
                blue = track->color[index].b * 2;
                battle_map_background_modify(mode, track->duration[index], red, green, blue);
                if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                    battle_map_darkness_start_blend(mode, track->duration[index], red, green, blue);
                }
            } else {
                colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r = track->color[index].r;
                colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g = track->color[index].g;
                colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b = track->color[index].b;
                colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r = track->second_color[index].r;
                colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g = track->second_color[index].g;
                colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b = track->second_color[index].b;
                battle_map_background_set(track->duration[index], &colors);
                if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                    battle_map_darkness_set(track->duration[index], &colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP]);
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}

/*
 * Advances one sound track of the parent timeline by a frame. The 0x1e-byte
 * track holds nine 16-bit durations at 0x00, nine channel bytes at 0x12
 * (0 and 1 none, otherwise a sound channel of the flags section + 2) and its
 * entry count at 0x1c; entering an entry plays the channel's next sound.
 * Sound ids are relative to g_effect_sound_data_base, the effect sound
 * section's id set by sound_section_load.
 */
static void parent_sound_track_advance(
    effect_sound_track_t* schedule, s16* entry_index, s16* countdown, u8* unused_sound_state) {
    s16 index = *entry_index;
    s32 effect_kind;
    s32 sound_index;

    if (index < schedule->entry_count) {
        if (*countdown == 0) {
            *entry_index = index + 1;
            *countdown = schedule->durations[index];
            effect_kind = schedule->channels[index];

            if (effect_kind >= 2) {
                sound_index = sound_id_select(effect_kind - 2);

                if (sound_index != 0) {
                    suzuki_sound_play_sfx(g_effect_sound_data_base + sound_index);
                }
            }
        }
        *countdown -= 1;
    }
}

/* Opcode 29: advance the record's parent timeline by one frame. The second
 * 16-bit value is the script offset its child records start at.
 *
 * Before phase1_duration the phase 1 particle channels, color tracks and
 * sound tracks run (and the time scale follows the timing curve under
 * EFFECT_FLAG_TIMING_CURVE_PHASE1); afterwards one child record per effect
 * target is started, spawn_delay frames apart. Phase 2 runs from
 * phase1_duration + (target_count - 1) * spawn_delay + child_duration. The
 * camera tracks advance every frame. */
effect_script_result_e effect_script_opcode_29_step_parent_timeline(effect_record_t* record) {
    effect_record_timeline_t* timeline;
    s32 i;
    s16 child;
    u8 unused_stack[16]; /* Unread locals that size the target's 0x48-byte frame. */

    timeline = &record->timeline;
    if (timeline->frame >= g_effect_timeline->phase1_duration) {
        while (timeline->state.parent.spawn_countdown == 0
            && timeline->state.parent.spawned_count < g_effect_context.target_count) {
            const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

            child = script_instance_start(record->script, instruction->immediate.value, record->record_index);
            g_effect_state_records[child].timeline.target_index = timeline->state.parent.spawned_count;
            timeline->state.parent.spawned_count++;
            timeline->state.parent.spawn_countdown += g_effect_timeline->spawn_delay;
        }
        timeline->state.parent.spawn_countdown--;
    } else {
        for (i = 0; i < EFFECT_PARTICLE_CHANNEL_COUNT; i++) {
            emitter_step(record, &g_effect_timeline->particle_channels[0][i],
                &timeline->state.parent.keyframe.tracks.particle[0][i],
                &timeline->state.parent.remaining.tracks.particle[0][i],
                &timeline->state.parent.step.tracks.particle[0][i]);
        }
        surroundings_palette_track_advance(&g_effect_timeline->color_tracks[0].surroundings,
            &timeline->state.parent.keyframe.tracks.surroundings[0],
            &timeline->state.parent.remaining.tracks.surroundings[0]);
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            caster_palette_track_advance(&g_effect_timeline->color_tracks[0].caster,
                &timeline->state.parent.keyframe.tracks.caster[0], &timeline->state.parent.remaining.tracks.caster[0],
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
        }
        target_palette_track_advance(&g_effect_timeline->color_tracks[0].target,
            &timeline->state.parent.keyframe.tracks.target[0], &timeline->state.parent.remaining.tracks.target[0]);
        map_background_track_advance(&g_effect_timeline->color_tracks[0].screen,
            &timeline->state.parent.keyframe.tracks.screen[0], &timeline->state.parent.remaining.tracks.screen[0]);
        parent_sound_track_advance(&g_effect_timeline->sound_tracks[0][0],
            &timeline->state.parent.keyframe.tracks.sound[0][0], &timeline->state.parent.remaining.tracks.sound[0][0],
            &timeline->state.parent.step.tracks.sound[0][0][1]);
        parent_sound_track_advance(&g_effect_timeline->sound_tracks[0][1],
            &timeline->state.parent.keyframe.tracks.sound[1][0], &timeline->state.parent.remaining.tracks.sound[1][0],
            &timeline->state.parent.step.tracks.sound[1][0][1]);
        parent_sound_track_advance(&g_effect_timeline->sound_tracks[0][2],
            &timeline->state.parent.keyframe.tracks.sound[2][0], &timeline->state.parent.remaining.tracks.sound[2][0],
            &timeline->state.parent.step.tracks.sound[2][0][1]);
        if (g_effect_flags_section->flags & EFFECT_FLAG_TIMING_CURVE_PHASE1) {
            system_min_vsync_interval_set(parent_timing_curve_evaluate(record));
        }
    }
    i = g_effect_timeline->phase1_duration + (g_effect_context.target_count - 1) * g_effect_timeline->spawn_delay
        + g_effect_timeline->child.duration;
    if (timeline->frame >= i) {
        for (i = 0; i < EFFECT_PARTICLE_CHANNEL_COUNT; i++) {
            emitter_step(record, &g_effect_timeline->particle_channels[1][i],
                &timeline->state.parent.keyframe.tracks.particle[1][i],
                &timeline->state.parent.remaining.tracks.particle[1][i],
                &timeline->state.parent.step.tracks.particle[1][i]);
        }
        surroundings_palette_track_advance(&g_effect_timeline->color_tracks[1].surroundings,
            &timeline->state.parent.keyframe.tracks.surroundings[1],
            &timeline->state.parent.remaining.tracks.surroundings[1]);
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            caster_palette_track_advance(&g_effect_timeline->color_tracks[1].caster,
                &timeline->state.parent.keyframe.tracks.caster[1], &timeline->state.parent.remaining.tracks.caster[1],
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
        }
        target_palette_track_advance(&g_effect_timeline->color_tracks[1].target,
            &timeline->state.parent.keyframe.tracks.target[1], &timeline->state.parent.remaining.tracks.target[1]);
        map_background_track_advance(&g_effect_timeline->color_tracks[1].screen,
            &timeline->state.parent.keyframe.tracks.screen[1], &timeline->state.parent.remaining.tracks.screen[1]);
        parent_sound_track_advance(&g_effect_timeline->sound_tracks[1][0],
            &timeline->state.parent.keyframe.tracks.sound[0][1], &timeline->state.parent.remaining.tracks.sound[0][1],
            &timeline->state.parent.step.tracks.sound[0][1][1]);
        parent_sound_track_advance(&g_effect_timeline->sound_tracks[1][1],
            &timeline->state.parent.keyframe.tracks.sound[1][1], &timeline->state.parent.remaining.tracks.sound[1][1],
            &timeline->state.parent.step.tracks.sound[1][1][1]);
        parent_sound_track_advance(&g_effect_timeline->sound_tracks[1][2],
            &timeline->state.parent.keyframe.tracks.sound[2][1], &timeline->state.parent.remaining.tracks.sound[2][1],
            &timeline->state.parent.step.tracks.sound[2][1][1]);
    }
    effect_camera_start_parent_keyframe_moves(timeline->frame);
    timeline->frame++;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 2a: clear the child-timeline state that opcode 28 steps and select effect target 0. */
effect_script_result_e effect_script_opcode_2a_reset_child_timeline(effect_record_t* record) {
    child_timeline_reset(record, 0);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 2b: clear the parent-timeline state (frame, spawn counters and
 * keyframe tracks) that opcode 29 steps. */
effect_script_result_e effect_script_opcode_2b_reset_parent_timeline(effect_record_t* record) {
    parent_timeline_reset(record);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 2c: nothing. */
effect_script_result_e effect_script_opcode_2c_nop(effect_record_t* record) {
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 2d: nothing. */
effect_script_result_e effect_script_opcode_2d_nop(effect_record_t* record) {
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Run record `index`'s script from its pc, one opcode handler after another,
 * until a handler returns YIELD or ABORT. This runner returns 1 on YIELD
 * (resume next frame) and 0 on ABORT (the caller frees the record). */
static s32 script_block_run(s16 index) {
    effect_record_t* rec;
    /* A narrow result preserves the retail sign extension before the tests. */
    s16 result;

    rec = &g_effect_state_records[index];
    for (;;) {
        result = g_effect_script_instructions[effect_get_current_script_header(rec) & EFFECT_SCRIPT_OPCODE_MASK](rec);
        if (result == EFFECT_SCRIPT_RESULT_ABORT) {
            return 0;
        }
        if (result == EFFECT_SCRIPT_RESULT_YIELD) {
            return 1;
        }
    }
}
