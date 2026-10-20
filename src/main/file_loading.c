/* Resident blocking disc reads and heap-backed load helpers. */

#include "fft/file.h"
#include "fft/memory.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "psx/libetc.h"
#include "psx/types.h"

/*
 * Start an asynchronous disc read through the shared file header.
 *
 * Overlay callers pass a byte count; the lower-level builder takes sectors.
 * Discard any partial sector rather than rounding the request up.
 */
s32 file_bytes_read_request(s32 sector, u32 size, void* destination) {
    return file_read_request_quiet(&g_file_cd_state, sector, size >> 11, destination);
}

/* Return the shared CD descriptor's load state: nonzero while a read through g_file_cd_state
 * is in progress. */
int file_load_state_get(void) {
    return g_file_cd_state.state;
}

/* Blocking read of a sound file into a new block of the music heap. size is in
 * bytes and only whole sectors are read. A full heap reports
 * SYSTEM_ERROR_SMD_LOAD and returns 0; the loader then only seeks, because
 * it skips the read for a null destination. */
suzuki_smd_header_t* file_smd_load(int sector, u32 size) {
    suzuki_smd_header_t* buffer = heap_smd_alloc(size);

    if (buffer == 0) {
        system_error_handle(SYSTEM_ERROR_SMD_LOAD);
    }
    while (file_read_request_quiet(&g_file_cd_state, sector, size >> 11, buffer) != 0) {
        VSync(0);
        system_frame_stub();
        file_load_poll(&g_file_cd_state);
    }
    if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        do {
            VSync(0);
            system_frame_stub();
            file_load_poll(&g_file_cd_state);
        } while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE);
    }
    return buffer;
}

/* Allocate size bytes from the game heap and read size >> 11 whole sectors,
 * blocking until the read ends. The caller must include sector rounding in
 * size; a partial final sector is not read. file_smd_load uses the same read
 * loop with the separate music heap. */
static void* heap_file_load(s32 sector, u32 size) {
    void* destination = heap_alloc(size);

    if (destination == 0) {
        system_error_handle(SYSTEM_ERROR_HEAP_ALLOC);
    }
    while (file_read_request_quiet(&g_file_cd_state, sector, (size >> 11), destination) != 0) {
        VSync(0);
        system_frame_stub();
        file_load_poll(&g_file_cd_state);
    }
    if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        do {
            VSync(0);
            system_frame_stub();
            file_load_poll(&g_file_cd_state);
        } while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE);
    }
    return destination;
}

/* Blocking read from a disc sector to destination without the "Now Loading"
 * message. size is in bytes and only whole sectors (size >> 11) are read. Waits
 * for the shared loader to be idle, then polls it once per frame until the read
 * ends. Always returns destination. */
void* file_buffer_load(int sector, u32 size, void* destination) {
    while (file_read_request_quiet(&g_file_cd_state, sector, size >> 11, destination) != 0) {
        VSync(0);
        system_frame_stub();
        file_load_poll(&g_file_cd_state);
    }
    if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        do {
            VSync(0);
            system_frame_stub();
            file_load_poll(&g_file_cd_state);
        } while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE);
    }
    return destination;
}

/* Blocking file_buffer_load that reports SYSTEM_ERROR_FILE_LOAD on a null result;
 * since that loader returns destination, only a null destination trips it. */
void* file_buffer_load_checked(int sector, int size, void* destination) {
    void* result = file_buffer_load(sector, size, destination);

    if (result == 0) {
        system_error_handle(SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}

/* Blocking load into a new game-heap block that also reports SYSTEM_ERROR_FILE_LOAD when no
 * block was free (heap_file_load has already reported SYSTEM_ERROR_HEAP_ALLOC). */
void* file_heap_load_checked(int sector, int size) {
    void* result = heap_file_load(sector, size);

    if (result == 0) {
        system_error_handle(SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}

/* Free a game-heap block through heap_free, dropping its result. */
void heap_block_release(void* pointer) {
    heap_free(pointer);
}
