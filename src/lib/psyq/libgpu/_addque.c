/* LIBGPU 80026454-80026478. */
#include "psx/libgpu.h"
/* Queue a GPU operation with no copied parameter block (_addque2 with copy_size 0). */
int _addque(psyq_gpu_operation_t operation, void* source, u32 argument) {
    return _addque2(operation, source, 0, argument);
}
