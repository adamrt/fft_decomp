#ifndef FFT_PSX_GS_H
#define FFT_PSX_GS_H

#include "psx/types.h"

/* libgs ordering-table handle. Five words, 0x14 bytes; the 0x14 stride is what
 * the per-display-buffer descriptor arrays are indexed with. `org` points at
 * the OT root, `tag` at the last entry, and `offset`/`point` carry the z
 * origin and resolution the sort helpers bias against. */
typedef struct {
    s32 log2n;  /* 0x00; the SDK spells this field `length`, but every
                 * accepted source here uses log2n, which is what it holds */
    u32* org;   /* 0x04 */
    s32 offset; /* 0x08 */
    s32 point;  /* 0x0C */
    u32* tag;   /* 0x10 */
} GsOT;

typedef char gs_ot_size_must_be_0x14[(sizeof(GsOT) == 0x14) ? 1 : -1];

/* libgs sprite descriptor (Psy-Q GsSPRITE), 0x24 bytes; 0x17 is implicit
 * padding. As a stack local GCC rounds it to 0x28, which is what the wldcore
 * drawing frames show. */
typedef struct {
    u32 attribute; /* 0x00 */
    s16 x;         /* 0x04 */
    s16 y;         /* 0x06 */
    u16 w;         /* 0x08 */
    u16 h;         /* 0x0A */
    u16 tpage;     /* 0x0C */
    u8 u;          /* 0x0E */
    u8 v;          /* 0x0F */
    s16 cx;        /* 0x10 */
    s16 cy;        /* 0x12 */
    u8 r;          /* 0x14 */
    u8 g;          /* 0x15 */
    u8 b;          /* 0x16 */
    s16 mx;        /* 0x18 */
    s16 my;        /* 0x1A */
    s16 scalex;    /* 0x1C */
    s16 scaley;    /* 0x1E */
    s32 rotate;    /* 0x20 */
} GsSPRITE;

typedef char gs_sprite_size_must_be_0x24[(sizeof(GsSPRITE) == 0x24) ? 1 : -1];

/* libgs flat-shaded line primitive descriptor, 0x10 bytes; 0x0F is implicit
 * padding. */
typedef struct {
    s32 attribute; /* 0x00 */
    s16 x0;        /* 0x04 */
    s16 y0;        /* 0x06 */
    s16 x1;        /* 0x08 */
    s16 y1;        /* 0x0A */
    u8 r;          /* 0x0C */
    u8 g;          /* 0x0D */
    u8 b;          /* 0x0E */
} GsLINE;

typedef char gs_line_size_must_be_0x10[(sizeof(GsLINE) == 0x10) ? 1 : -1];

/* libgs filled-rectangle descriptor (Psy-Q GsBOXF), 0x10 bytes; 0x0F is
 * implicit padding. */
typedef struct {
    u32 attribute; /* 0x00 */
    s16 x;         /* 0x04 */
    s16 y;         /* 0x06 */
    u16 w;         /* 0x08 */
    u16 h;         /* 0x0A */
    u8 r;          /* 0x0C */
    u8 g;          /* 0x0D */
    u8 b;          /* 0x0E */
} GsBOXF;

typedef char gs_boxf_size_must_be_0x10[(sizeof(GsBOXF) == 0x10) ? 1 : -1];

/* libgs TIM image descriptor (Psy-Q GsIMAGE), 0x1C bytes, as GsGetTimInfo
 * fills it: pixel mode, then VRAM rectangle and data pointer for the pixels
 * and for the CLUT. Widths are in 16-bit VRAM units. */
typedef struct {
    u32 pmode;  /* 0x00 */
    s16 px;     /* 0x04 */
    s16 py;     /* 0x06 */
    u16 pw;     /* 0x08 */
    u16 ph;     /* 0x0A */
    u32* pixel; /* 0x0C */
    s16 cx;     /* 0x10 */
    s16 cy;     /* 0x12 */
    u16 cw;     /* 0x14 */
    u16 ch;     /* 0x16 */
    u32* clut;  /* 0x18 */
} GsIMAGE;

typedef char gs_image_size_must_be_0x1c[(sizeof(GsIMAGE) == 0x1C) ? 1 : -1];

#endif /* FFT_PSX_GS_H */
