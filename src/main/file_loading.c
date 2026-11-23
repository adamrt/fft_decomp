/* Resident blocking disc reads and heap-backed load helpers. */

#include "fft/sys/file.h"
#include "fft/sys/memory.h"
#include "fft/sys/suzuki.h"
#include "fft/sys/system.h"
#include "psx/libetc.h"
#include "psx/types.h"

/*
 * Start an asynchronous disc read through the shared file header.
 *
 * Overlay callers pass a byte count; the lower-level builder takes sectors.
 * Discard any partial sector rather than rounding the request up.
 */
s32 file_request_read_bytes(s32 sector, u32 size, void* destination) {
    return file_request_read_quiet(&g_file_cd_state, sector, size >> 11, destination);
}

/* Return the shared CD descriptor's load state: nonzero while a read through g_file_cd_state
 * is in progress. */
int file_get_load_state(void) {
    return g_file_cd_state.state;
}

/* Blocking read of a sound file into a new block of the music heap. size is in
 * bytes and only whole sectors are read. A full heap reports
 * SYSTEM_ERROR_SMD_LOAD and returns 0; the loader then only seeks, because
 * it skips the read for a null destination. */
suzuki_smd_header_t* file_load_smd(int sector, u32 size) {
    suzuki_smd_header_t* buffer = heap_smd_alloc(size);

    if (buffer == 0) {
        system_handle_error(SYSTEM_ERROR_SMD_LOAD);
    }
    while (file_request_read_quiet(&g_file_cd_state, sector, size >> 11, buffer) != 0) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        do {
            VSync(0);
            system_frame_stub();
            file_poll_load(&g_file_cd_state);
        } while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE);
    }
    return buffer;
}

/* Allocate size bytes from the game heap and read size >> 11 whole sectors,
 * blocking until the read ends. The caller must include sector rounding in
 * size; a partial final sector is not read. file_load_smd uses the same read
 * loop with the separate music heap. */
static void* load_into_heap(s32 sector, u32 size) {
    void* destination = heap_alloc(size);

    if (destination == 0) {
        system_handle_error(SYSTEM_ERROR_HEAP_ALLOC);
    }
    while (file_request_read_quiet(&g_file_cd_state, sector, (size >> 11), destination) != 0) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        do {
            VSync(0);
            system_frame_stub();
            file_poll_load(&g_file_cd_state);
        } while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE);
    }
    return destination;
}

/* Blocking read from a disc sector to destination without the "Now Loading"
 * message. size is in bytes and only whole sectors (size >> 11) are read. Waits
 * for the shared loader to be idle, then polls it once per frame until the read
 * ends. Always returns destination. */
void* file_load_into_buffer(int sector, u32 size, void* destination) {
    while (file_request_read_quiet(&g_file_cd_state, sector, size >> 11, destination) != 0) {
        VSync(0);
        system_frame_stub();
        file_poll_load(&g_file_cd_state);
    }
    if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        do {
            VSync(0);
            system_frame_stub();
            file_poll_load(&g_file_cd_state);
        } while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE);
    }
    return destination;
}

/* Blocking file_load_into_buffer that reports SYSTEM_ERROR_FILE_LOAD on a null result;
 * since that loader returns destination, only a null destination trips it. */
void* file_load_into_buffer_checked(int sector, int size, void* destination) {
    void* result = file_load_into_buffer(sector, size, destination);

    if (result == 0) {
        system_handle_error(SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}

/* Blocking load into a new game-heap block that also reports SYSTEM_ERROR_FILE_LOAD when no
 * block was free (load_into_heap has already reported SYSTEM_ERROR_HEAP_ALLOC). */
void* file_load_into_heap_checked(int sector, int size) {
    void* result = load_into_heap(sector, size);

    if (result == 0) {
        system_handle_error(SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}

/* Free a game-heap block through heap_free, dropping its result. */
void heap_release_block(void* pointer) {
    heap_free(pointer);
}
