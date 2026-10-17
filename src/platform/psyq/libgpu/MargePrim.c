/* LIBGPU 80023fd4-8002400c. */
#include "psx/libgpu.h"

int MargePrim(void* first, void* second) {
    int length = getlen(first) + getlen(second) + 1;
    int result;

    if (length <= 32) {
        setlen(first, length);
        *(u32*)second = 0;
        result = 0;
    } else {
        result = -1;
    }
    return result;
}
