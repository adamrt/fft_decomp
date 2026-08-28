#ifndef FFT_PSX_GPU_H
#define FFT_PSX_GPU_H

#include "psx/types.h"

/* GPU packet header: low 24 bits link to the next primitive; high 8 bits
 * contain the packet length. The SDK view continues with the first command
 * word (colour and GPU code), so it is 8 bytes; game code only uses it
 * through casts of primitive pointers. */
typedef struct {
    u32 addr : 24;
    u32 len : 8;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
} P_TAG;

#define setaddr(p, a)  (((P_TAG*)(p))->addr = (u32)(a))
#define getaddr(p)     ((u32)((P_TAG*)(p))->addr)
#define addPrim(ot, p) setaddr(p, getaddr(ot)), setaddr(ot, p)

typedef struct {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} RECT;

typedef struct {
    u32 tag;
    u32 code[15];
} DR_ENV;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd;
    u8 dfe;
    u8 isbg;
    u8 r0;
    u8 g0;
    u8 b0;
    DR_ENV dr_env;
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter;
    u8 isrgb24;
    u8 pad0;
    u8 pad1;
} DISPENV;

typedef struct {
    u32 mode;
    RECT* crect;
    u32* caddr;
    RECT* prect;
    u32* paddr;
} TIM_IMAGE;

typedef struct {
    u32 tag;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
    s16 x2;
    s16 y2;
    s16 x3;
    s16 y3;
} POLY_F4;

typedef struct {
    u32 tag;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
    s16 x2;
    s16 y2;
} POLY_F3;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
} POLY_G3;

/* Untextured Gouraud quad. */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} POLY_G4;
typedef char assert_poly_g4_size[sizeof(POLY_G4) == 36 ? 1 : -1];

typedef struct {
    u32 tag;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    s16 x0;
    s16 y0;
    u8 u0;
    u8 v0;
    u16 clut;
    s16 x1;
    s16 y1;
    u8 u1;
    u8 v1;
    u16 tpage;
    s16 x2;
    s16 y2;
    u8 u2;
    u8 v2;
    u16 pad1;
    s16 x3;
    s16 y3;
    u8 u3;
    u8 v3;
    u16 pad2;
} POLY_FT4;

/* Flat-shaded textured triangle packet. */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
} POLY_FT3;

typedef struct {
    u32 tag;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    s16 x0;
    s16 y0;
    u8 u0;
    u8 v0;
    u16 clut;
    u8 r1;
    u8 g1;
    u8 b1;
    u8 p1;
    s16 x1;
    s16 y1;
    u8 u1;
    u8 v1;
    u16 tpage;
    u8 r2;
    u8 g2;
    u8 b2;
    u8 p2;
    s16 x2;
    s16 y2;
    u8 u2;
    u8 v2;
    u16 pad2;
} POLY_GT3;

typedef struct {
    u32 tag;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    s16 x0;
    s16 y0;
    u8 u0;
    u8 v0;
    u16 clut;
    u8 r1;
    u8 g1;
    u8 b1;
    u8 p1;
    s16 x1;
    s16 y1;
    u8 u1;
    u8 v1;
    u16 tpage;
    u8 r2;
    u8 g2;
    u8 b2;
    u8 p2;
    s16 x2;
    s16 y2;
    u8 u2;
    u8 v2;
    u16 pad2;
    u8 r3;
    u8 g3;
    u8 b3;
    u8 p3;
    s16 x3;
    s16 y3;
    u8 u3;
    u8 v3;
    u16 pad3;
} POLY_GT4;

typedef struct {
    u32 tag;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    s16 x0;
    s16 y0;
    u8 u0;
    u8 v0;
    u16 clut;
    s16 w;
    s16 h;
} SPRT;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_MODE;

/* libgpu DR_MOVE (u32 code[5]); the VRAM-to-VRAM move command words are
 * spelled out so game code can fill the source/destination halfwords. */
typedef struct {
    u32 tag;
    u32 code[2]; /* 0x04: cache flush, move command */
    s16 x0;      /* 0x0c: source */
    s16 y0;
    s16 x1; /* 0x10: destination */
    s16 y1;
    s16 w; /* 0x14 */
    s16 h;
} DR_MOVE;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_AREA;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_OFFSET;

typedef struct {
    u32 tag;
    u32 code[1];
} DR_TPAGE;

/* Gouraud line. */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, p1;
    s16 x1, y1;
} LINE_G2;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LINE_F2;

typedef struct {
    u32 tag;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
    s16 x0;
    s16 y0;
    s16 w;
    s16 h;
} TILE;

typedef char assert_rect_size[sizeof(RECT) == 8 ? 1 : -1];
typedef char assert_dr_env_size[sizeof(DR_ENV) == 64 ? 1 : -1];
typedef char assert_drawenv_size[sizeof(DRAWENV) == 92 ? 1 : -1];
typedef char assert_dispenv_size[sizeof(DISPENV) == 20 ? 1 : -1];
typedef char assert_tim_image_size[sizeof(TIM_IMAGE) == 20 ? 1 : -1];
typedef char assert_poly_f3_size[sizeof(POLY_F3) == 20 ? 1 : -1];
typedef char assert_poly_g3_size[sizeof(POLY_G3) == 28 ? 1 : -1];
typedef char assert_poly_f4_size[sizeof(POLY_F4) == 24 ? 1 : -1];
typedef char assert_poly_ft4_size[sizeof(POLY_FT4) == 40 ? 1 : -1];
typedef char assert_poly_gt3_size[sizeof(POLY_GT3) == 40 ? 1 : -1];
typedef char assert_poly_gt4_size[sizeof(POLY_GT4) == 52 ? 1 : -1];
typedef char assert_sprt_size[sizeof(SPRT) == 20 ? 1 : -1];
typedef char assert_dr_mode_size[sizeof(DR_MODE) == 12 ? 1 : -1];
typedef char assert_dr_offset_size[sizeof(DR_OFFSET) == 12 ? 1 : -1];
typedef char assert_dr_tpage_size[sizeof(DR_TPAGE) == 8 ? 1 : -1];
typedef char assert_p_tag_size[sizeof(P_TAG) == 8 ? 1 : -1];
typedef char assert_tile_size[sizeof(TILE) == 16 ? 1 : -1];
typedef char assert_line_g2_size[sizeof(LINE_G2) == 20 ? 1 : -1];
typedef char assert_line_f2_size[sizeof(LINE_F2) == 16 ? 1 : -1];

extern void FntLoad(int, int);
extern int FntOpen(int, int, int, int, int, int);
extern int FntPrint(char*, ...);
extern void FntFlush(int);
extern void SetDumpFnt(int);
extern int OpenTIM(void*);
extern int ReadTIM(TIM_IMAGE*);
extern void LoadTPage(u32*, int, int, int, int, int, int);
extern void LoadClut2(u32*, int, int);
extern void AddPrim(void*, void*);
extern void SetSemiTrans(void*, int);
extern void SetShadeTex(void*, int);
extern void SetPolyF3(POLY_F3*);
extern void SetPolyFT3(void*);
extern void SetPolyG3(void*);
extern void SetPolyGT3(void*);
extern void SetPolyF4(POLY_F4*);
extern void SetPolyFT4(POLY_FT4*);
extern void SetPolyG4(void*);
extern void SetPolyGT4(void*);
extern void SetSprt8(void*);
extern void SetSprt16(void*);
extern void SetSprt(void*);
extern void SetTile1(void*);
extern void SetTile8(void*);
extern void SetTile16(void*);
extern void SetTile(void*);
extern void SetLineF2(void*);
extern void SetLineG2(void*);
extern void SetLineF3(void*);
extern void SetLineG3(void*);
extern void SetLineF4(void*);
extern void SetLineG4(void*);
extern void SetBlockFill(void*);
extern void SetDrawMove(void*);
extern u16 GetTPage(int tp, int abr, int x, int y);
extern u16 GetClut(int x, int y);
extern DR_MODE* SetDrawMode(DR_MODE*, int, int, int, RECT*);
extern DR_AREA* SetDrawArea(DR_AREA*, RECT*);
extern void SetDispMask(int);
extern void ResetGraph(int);
extern void SetGraphDebug(int);
extern int GetGraphType(void);
extern void DrawSyncCallback(void*);
extern int DrawSync(int);
extern void ClearImage(RECT*, u8, u8, u8);
extern void ClearOTag(void*, int);
extern void ClearOTagR(u32*, int);
extern s32 LoadImage(RECT*, u32*);
extern s32 StoreImage(RECT*, u32*);
extern s32 MoveImage(RECT*, s32, s32);
extern void DrawOTag(u32* ot);
extern DRAWENV* SetDefDrawEnv(DRAWENV*, int, int, int, int);
extern DISPENV* SetDefDispEnv(DISPENV*, int, int, int, int);
extern DRAWENV* GetDrawEnv(DRAWENV*);
extern DRAWENV* PutDrawEnv(DRAWENV*);
extern DISPENV* PutDispEnv(DISPENV*);

u16 LoadClut(u32* clut, s32 x, s32 y);
void SetDrawOffset(void* destination, void* position);

void DrawPrim(void* p);

#endif
