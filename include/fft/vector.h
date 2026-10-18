#ifndef VECTOR_H
#define VECTOR_H

/* Resident coordinate setters for the SDK's short and full-width vectors. */

#include "psx/libgte.h"
#include "psx/types.h"

void vector_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z);
void vector_set_vector(VECTOR* vector, s32 x, s32 y, s32 z);

#endif
