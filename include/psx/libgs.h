#ifndef PSX_LIBGS_H
#define PSX_LIBGS_H

#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* GsSPRITE/GsLINE/GsBOXF.attribute: suppress display. */
#define GsDOFF      (1U << 31)
#define ZRESOLUTION 0x3fff
#define GsOFSGPU    4
#define GsINTER     1

/* Psy-Q packet work areas are byte buffers holding variable GPU packet layouts. */
typedef u8 PACKET;

/* Psy-Q ordering-table tag: a 24-bit link and eight-bit packet length. */
typedef struct {
    u32 p : 24;
    u8 num : 8;
} GsOT_TAG;
typedef char gs_ot_tag_size_must_be_4[(sizeof(GsOT_TAG) == 4) ? 1 : -1];

/* libgs ordering-table handle. Five 32-bit fields, 0x14 bytes; the 0x14 stride is what
 * the per-display-buffer descriptor arrays are indexed with. `org` points at
 * the OT root, `tag` at the last entry, and `offset`/`point` carry the z
 * origin and resolution the sort helpers bias against. */
typedef struct {
    u32 length;    /* 0x00; number of ordering-table entries is 1 << length */
    GsOT_TAG* org; /* 0x04 */
    u32 offset;    /* 0x08 */
    u32 point;     /* 0x0C */
    GsOT_TAG* tag; /* 0x10 */
} GsOT;

typedef char gs_ot_size_must_be_20[(sizeof(GsOT) == 20) ? 1 : -1];

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

typedef char gs_sprite_size_must_be_36[(sizeof(GsSPRITE) == 36) ? 1 : -1];

/* libgs flat-shaded line primitive descriptor, 0x10 bytes; 0x0F is implicit
 * padding. */
typedef struct {
    u32 attribute; /* 0x00 */
    s16 x0;        /* 0x04 */
    s16 y0;        /* 0x06 */
    s16 x1;        /* 0x08 */
    s16 y1;        /* 0x0A */
    u8 r;          /* 0x0C */
    u8 g;          /* 0x0D */
    u8 b;          /* 0x0E */
} GsLINE;

typedef char gs_line_size_must_be_16[(sizeof(GsLINE) == 16) ? 1 : -1];

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

typedef char gs_boxf_size_must_be_16[(sizeof(GsBOXF) == 16) ? 1 : -1];

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

typedef char gs_image_size_must_be_28[(sizeof(GsIMAGE) == 28) ? 1 : -1];

/* 2D screen offset (Psy-Q _GsPOSITION). */
typedef struct {
    s16 offx; /* 0x00 */
    s16 offy; /* 0x02 */
} _GsPOSITION;

extern RECT CLIP2;           /* clip rectangle relative to the draw buffer base */
extern s16 PSDBASEX[2];      /* draw-buffer bases, zero in GPU offset mode */
extern s16 PSDBASEY[2];      /* no other linked function reads these bases */
extern s16 PSDIDX;           /* active draw buffer */
extern u32 PSDCNT;           /* frame counter; GsSwapDispBuff skips zero */
extern _GsPOSITION POSITION; /* 2D offset */
extern DRAWENV GsDRAWENV;
extern DISPENV GsDISPENV;
extern s32 HWD0;               /* horizontal resolution */
extern s32 VWD0;               /* vertical resolution */
extern MATRIX GsLIGHTWSMATRIX; /* world-screen light matrix */
extern MATRIX GsIDMATRIX;
extern MATRIX GsIDMATRIX2;     /* identity scaled by the aspect ratio */
extern PACKET* GsOUT_PACKET_P; /* packet work area cursor */
extern s32 GsLIGHT_MODE;

void GsInitGraph(u16 x, u16 y, u16 intmode, u16 dith, u16 varmmode);
void GsInitGraph2(u16 x, u16 y, u16 intmode, u16 dith, u16 varmmode);
void GsInit3D(void);
void GsDefDispBuff(u16 x0, u16 y0, u16 x1, u16 y1);
void GsSwapDispBuff(void);
s32 GsGetActiveBuff(void);
void GsSetDrawBuffClip(void);
void GsSetDrawBuffOffset(void);
void GsSetClip2D(RECT* rectp);
void GsClearOt(u16 offset, u16 point, GsOT* otp);
void GsDrawOt(GsOT* ot);
void GsSetWorkBase(PACKET* base);
void GsSortSprite(GsSPRITE* sp, GsOT* ot, u16 pri);
void GsSortFastSprite(GsSPRITE* sp, GsOT* ot, u16 pri);
void GsSortFlipSprite(GsSPRITE* sp, GsOT* ot, u16 pri);
void GsSortBoxFill(GsBOXF* bp, GsOT* ot, u16 pri);
void GsSortLine(GsLINE* lp, GsOT* ot, u16 pri);
void GsSortPoly(void* pp, GsOT* ot, u16 pri);
void GsSortClear(u8 r, u8 g, u8 b, GsOT* otp);
void GsGetTimInfo(u32* tim, GsIMAGE* im);

/* driver internals */

extern const char g_psyq_gs_sort_sprite_overflow_message[];
extern TILE g_psyq_gs_clear_tile[2]; /* GsSortClear's tile for each draw buffer */

/* Frame-buffer coordinates used for display, clipping and geometry offsets. */
extern s16 PSDOFSX[2];
extern s16 PSDOFSY[2];

/* Offset the sorters add to 2D coordinates: zero in GPU offset mode, else
 * the GTE offset GsSetDrawBuffOffset programmed. */
extern s16 GsORGOFSX;
extern s16 GsORGOFSY;
extern s16 PSDGPU; /* GsOFSGPU: offsets go through the draw environment */

/* Internal matrix reset to zero by graphics initialization; no other linked
 * function accesses it. */
extern MATRIX _LC;

/* GsInit3D clipping limits; no other linked function reads them. */
extern s32 GsCLIP3far;
extern s32 GsCLIP3near;

PACKET* psyq_gs_sort_sprite_bg(u32* tag, GsOT* ot, u16 z, s32 len);
void psyq_gs_init_gpu(u16 x, u16 y, u16 intmode, u16 dith, u16 varmmode);
void psyq_gs_init_gte(void);
void psyq_gs_reset_screen_state(u16 width, u16 height);

#endif /* PSX_LIBGS_H */
