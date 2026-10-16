#ifndef PSX_LIBGPU_H
#define PSX_LIBGPU_H

#include "psx/libapi.h"
#include "psx/libetc.h"
#include "psx/libgte.h"
#include "psx/stddef.h"

#include "psx/tmd.h"
#include "psx/types.h"

/* Command byte in the first 32-bit GP0 command; packet lengths exclude the DMA tag. */
typedef enum {
    PSYQ_GPU_CODE_CLEAR_CACHE = 0x01,
    PSYQ_GPU_CODE_BLOCK_FILL = 0x02,
    PSYQ_GPU_CODE_POLY_F3 = 0x20,
    PSYQ_GPU_CODE_POLY_FT3 = 0x24,
    PSYQ_GPU_CODE_POLY_F4 = 0x28,
    PSYQ_GPU_CODE_POLY_FT4 = 0x2c,
    PSYQ_GPU_CODE_POLY_G3 = 0x30,
    PSYQ_GPU_CODE_POLY_GT3 = 0x34,
    PSYQ_GPU_CODE_POLY_G4 = 0x38,
    PSYQ_GPU_CODE_POLY_GT4 = 0x3c,
    PSYQ_GPU_CODE_LINE_F2 = 0x40,
    PSYQ_GPU_CODE_LINE_F3 = 0x48,
    PSYQ_GPU_CODE_LINE_F4 = 0x4c,
    PSYQ_GPU_CODE_LINE_G2 = 0x50,
    PSYQ_GPU_CODE_LINE_G3 = 0x58,
    PSYQ_GPU_CODE_LINE_G4 = 0x5c,
    PSYQ_GPU_CODE_TILE = 0x60,
    PSYQ_GPU_CODE_SPRT = 0x64,
    PSYQ_GPU_CODE_TILE1 = 0x68,
    PSYQ_GPU_CODE_TILE8 = 0x70,
    PSYQ_GPU_CODE_SPRT8 = 0x74,
    PSYQ_GPU_CODE_TILE16 = 0x78,
    PSYQ_GPU_CODE_SPRT16 = 0x7c,
    PSYQ_GPU_CODE_MOVE_IMAGE = 0x80,
} psyq_gpu_primitive_code_e;

typedef enum {
    PSYQ_GPU_CODE_RAW_TEXTURE = 0x01,
    PSYQ_GPU_CODE_SEMI_TRANSPARENT = 0x02,
} psyq_gpu_primitive_flag_e;

/* Polygon command layout flags. */
enum {
    PSYQ_GPU_POLYGON_TEXTURED = 0x04,
    PSYQ_GPU_POLYGON_QUAD = 0x08,
    PSYQ_GPU_POLYGON_GOURAUD = 0x10,
};

/* Semi-transparency mode in bits 5-6 of a texture page value (getTPage's abr
 * outside the GraphType 1 and 2 layouts); texture page 0 when used alone. */
typedef enum {
    PSYQ_GPU_TPAGE_ABR_AVERAGE = 0x00,     /* back / 2 + front / 2 */
    PSYQ_GPU_TPAGE_ABR_ADD = 0x20,         /* back + front */
    PSYQ_GPU_TPAGE_ABR_SUBTRACT = 0x40,    /* back - front */
    PSYQ_GPU_TPAGE_ABR_ADD_QUARTER = 0x60, /* back + front / 4 */
} psyq_gpu_tpage_abr_e;

#define PSYQ_GPU_FIXED_TILE_WORDS          2
#define PSYQ_GPU_FIXED_SPRITE_WORDS        3
#define PSYQ_GPU_PACKET_WORDS(packet_type) (sizeof(packet_type) / sizeof(u32) - 1)
#define PSYQ_GPU_COMMAND_WORD(code)        ((u32)(code) << 24)
#define PSYQ_GPU_POLYLINE_END              0x55555555U

/* GP0 command bytes, used with PSYQ_GPU_COMMAND_WORD. */
typedef enum {
    PSYQ_GPU_CODE_CPU_TO_VRAM = 0xa0,
    PSYQ_GPU_CODE_VRAM_TO_CPU = 0xc0,
    PSYQ_GPU_CODE_DRAW_MODE = 0xe1,
    PSYQ_GPU_CODE_TEXTURE_WINDOW = 0xe2,
    PSYQ_GPU_CODE_DRAW_AREA_TOP_LEFT = 0xe3,
    PSYQ_GPU_CODE_DRAW_AREA_BOTTOM_RIGHT = 0xe4,
    PSYQ_GPU_CODE_DRAW_OFFSET = 0xe5,
    PSYQ_GPU_CODE_DRAW_MASK = 0xe6
} psyq_gpu_attribute_code_e;

/* GP1 command bytes; the payload occupies the low 24 bits. */
typedef enum {
    PSYQ_GPU_GP1_RESET = 0x00,
    PSYQ_GPU_GP1_RESET_BUFFER = 0x01,
    PSYQ_GPU_GP1_ACK_IRQ = 0x02,
    PSYQ_GPU_GP1_DISPLAY_ENABLE = 0x03,
    PSYQ_GPU_GP1_DMA_DIRECTION = 0x04,
    PSYQ_GPU_GP1_DISPLAY_START = 0x05,
    PSYQ_GPU_GP1_HORIZONTAL_RANGE = 0x06,
    PSYQ_GPU_GP1_VERTICAL_RANGE = 0x07,
    PSYQ_GPU_GP1_DISPLAY_MODE = 0x08,
    PSYQ_GPU_GP1_VRAM_SIZE = 0x09,
    PSYQ_GPU_GP1_GET_INFO = 0x10,
    PSYQ_GPU_GP1_VRAM_SIZE_V1 = 0x20
} psyq_gpu_control_code_e;

typedef enum {
    PSYQ_GPU_INFO_DRAW_AREA_TOP_LEFT = 3,
    PSYQ_GPU_INFO_DRAW_AREA_BOTTOM_RIGHT = 4,
    PSYQ_GPU_INFO_DRAW_OFFSET = 5,
    PSYQ_GPU_INFO_VERSION = 7
} psyq_gpu_info_e;

#define PSYQ_GPU_PARAMETER_MASK    0x00ffffffU
#define PSYQ_GPU_RGB_MASK          0x00ffffffU
#define PSYQ_GPU_MASK_SET          0x00000001U
#define PSYQ_GPU_MASK_TEST         0x00000002U
#define PSYQ_GPU_DISPLAY_DISABLED  0x00000001U
#define PSYQ_GPU_VRAM_2MB          0x00000001U
#define PSYQ_GPU_DMA_REQUEST_OFF   0x00000000U
#define PSYQ_GPU_DMA_REQUEST_WRITE 0x00000002U
#define PSYQ_GPU_DMA_REQUEST_READ  0x00000003U

/* GPU status when GP1 is read, rather than a command written to GP1. */
#define PSYQ_GPU_STATUS_HEIGHT_480      0x00080000U
#define PSYQ_GPU_STATUS_READY_COMMAND   0x04000000U
#define PSYQ_GPU_STATUS_READ_DATA_READY 0x08000000U
#define PSYQ_GPU_STATUS_READY_DMA       0x10000000U
#define PSYQ_GPU_STATUS_ODD_LINE        0x80000000U

/* GPU DMA addresses and image-transfer policy. */
#define PSYQ_GPU_DMA_ADDRESS_MASK      0x00ffffffU
#define PSYQ_GPU_DMA_END               0x00ffffffU
#define PSYQ_GPU_DMA_IMAGE_BLOCK_WORDS 16U

/* GP1 display-mode payload. */
#define PSYQ_GPU_DISPLAY_WIDTH_320  0x01U
#define PSYQ_GPU_DISPLAY_WIDTH_512  0x02U
#define PSYQ_GPU_DISPLAY_WIDTH_640  0x03U
#define PSYQ_GPU_DISPLAY_HEIGHT_480 0x04U
#define PSYQ_GPU_DISPLAY_PAL        0x08U
#define PSYQ_GPU_DISPLAY_RGB24      0x10U
#define PSYQ_GPU_DISPLAY_INTERLACE  0x20U
#define PSYQ_GPU_DISPLAY_WIDTH_368  0x40U
#define PSYQ_GPU_DISPLAY_REVERSE    0x80U

/* GPU packet prefix: low 24 bits link to the next primitive; high 8 bits
 * contain the packet length. The next word stores color and command bytes,
 * making P_TAG 8 bytes. Primitive header macros access it through casts of
 * primitive pointers. */
typedef struct {
    u32 addr : 24;
    u32 len : 8;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
} P_TAG;

#define setaddr(p, a)        (((P_TAG*)(p))->addr = (u32)(a))
#define getaddr(p)           ((u32)((P_TAG*)(p))->addr)
#define addPrim(ot, p)       setaddr(p, getaddr(ot)), setaddr(ot, p)
#define addPrims(ot, p0, p1) setaddr(p1, getaddr(ot)), setaddr(ot, p0)
#define catPrim(p0, p1)      setaddr(p0, p1)
#define termPrim(p)          setaddr(p, 0xffffffff)
#define nextPrim(p)          ((void*)((((P_TAG*)(p))->addr) | 0x80000000))
#define isendprim(p)         ((((P_TAG*)(p))->addr) == 0xffffff)

#define setlen(p, l)  (((P_TAG*)(p))->len = (u8)(l))
#define setcode(p, c) (((P_TAG*)(p))->code = (u8)(c))
#define getlen(p)     ((u8)((P_TAG*)(p))->len)
#define getcode(p)    (((P_TAG*)(p))->code)

#define setPolyF3(p)  (setlen(p, 4), setcode(p, 0x20))
#define setPolyFT3(p) (setlen(p, 7), setcode(p, 0x24))
#define setPolyG3(p)  (setlen(p, 6), setcode(p, 0x30))
#define setPolyGT3(p) (setlen(p, 9), setcode(p, 0x34))
#define setPolyF4(p)  (setlen(p, 5), setcode(p, 0x28))
#define setPolyFT4(p) (setlen(p, 9), setcode(p, 0x2c))
#define setPolyG4(p)  (setlen(p, 8), setcode(p, 0x38))
#define setPolyGT4(p) (setlen(p, 12), setcode(p, 0x3c))

#define setSprt8(p)     (setlen(p, 3), setcode(p, 0x74))
#define setSprt16(p)    (setlen(p, 3), setcode(p, 0x7c))
#define setSprt(p)      (setlen(p, 4), setcode(p, 0x64))
#define setTile1(p)     (setlen(p, 2), setcode(p, 0x68))
#define setTile8(p)     (setlen(p, 2), setcode(p, 0x70))
#define setTile16(p)    (setlen(p, 2), setcode(p, 0x78))
#define setTile(p)      (setlen(p, 3), setcode(p, 0x60))
#define setBlockFill(p) (setlen(p, 3), setcode(p, 0x02))
#define setDrawMove(p)  (setlen(p, 5), setcode(p, 0x01), (p)->code2 = 0x80000000)
#define setLineF2(p)    (setlen(p, 3), setcode(p, 0x40))
#define setLineG2(p)    (setlen(p, 4), setcode(p, 0x50))
#define setLineF3(p)    (setlen(p, 5), setcode(p, 0x48), (p)->pad = 0x55555555)
#define setLineF4(p)    (setlen(p, 6), setcode(p, 0x4c), (p)->pad = 0x55555555)

#define setSemiTrans(p, abe) ((abe) ? setcode(p, getcode(p) | 0x02) : setcode(p, getcode(p) & ~0x02))
#define setShadeTex(p, tge)  ((tge) ? setcode(p, getcode(p) | 0x01) : setcode(p, getcode(p) & ~0x01))

#define limitRange(x, l, h) ((x) = ((x) < (l) ? (l) : (x) > (h) ? (h) : (x)))

#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3f))

#define getTPage(tp, abr, x, y)                                                                                        \
    ((GetGraphType() == 1 || GetGraphType() == 2)                                                                      \
            ? ((((tp) & 0x3) << 9) | (((abr) & 0x3) << 7) | (((y) & 0x300) >> 3) | (((x) & 0x3ff) >> 6))               \
            : ((((tp) & 0x3) << 7) | (((abr) & 0x3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3ff) >> 6)                \
                  | (((y) & 0x200) << 2)))

#define _get_mode(dfe, dtd, tpage)                                                                                     \
    ((GetGraphType() == 1 || GetGraphType() == 2)                                                                      \
            ? (0xe1000000 | ((dtd) ? 0x0800 : 0) | ((dfe) ? 0x1000 : 0) | ((tpage) & 0x27ff))                          \
            : (0xe1000000 | ((dtd) ? 0x0200 : 0) | ((dfe) ? 0x0400 : 0) | ((tpage) & 0x9ff)))

#define copyVector(v0, v1) (v0)->vx = (v1)->vx, (v0)->vy = (v1)->vy, (v0)->vz = (v1)->vz

#define setVector(v, x, y, z) (v)->vx = (x), (v)->vy = (y), (v)->vz = (z)

#define setRGB0(p, r, g, b) (p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b)
#define setRGB1(p, r, g, b) (p)->r1 = (r), (p)->g1 = (g), (p)->b1 = (b)
#define setRGB2(p, r, g, b) (p)->r2 = (r), (p)->g2 = (g), (p)->b2 = (b)
#define setRGB3(p, r, g, b) (p)->r3 = (r), (p)->g3 = (g), (p)->b3 = (b)

#define setRECT(r, _x, _y, _w, _h) (r)->x = (_x), (r)->y = (_y), (r)->w = (_w), (r)->h = (_h)

#define setXY0(p, _x0, _y0) (p)->x0 = (_x0), (p)->y0 = (_y0)
#define setUV0(p, _u0, _v0) (p)->u0 = (_u0), (p)->v0 = (_v0)
#define setWH(p, _w, _h)    (p)->w = _w, (p)->h = _h

#define setXY2(p, _x0, _y0, _x1, _y1) (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1)

#define setXY4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3)                                                              \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1), (p)->x2 = (_x2), (p)->y2 = (_y2),              \
    (p)->x3 = (_x3), (p)->y3 = (_y3)

#define setXYWH(p, _x0, _y0, _w, _h)                                                                                   \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x0) + (_w), (p)->y1 = (_y0), (p)->x2 = (_x0),                        \
    (p)->y2 = (_y0) + (_h), (p)->x3 = (_x0) + (_w), (p)->y3 = (_y0) + (_h)

#define setUV3(p, _u0, _v0, _u1, _v1, _u2, _v2)                                                                        \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), (p)->u2 = (_u2), (p)->v2 = (_v2)

#define setUV4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3)                                                              \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), (p)->u2 = (_u2), (p)->v2 = (_v2),              \
    (p)->u3 = (_u3), (p)->v3 = (_v3)

#define setUVWH(p, _u0, _v0, _w, _h)                                                                                   \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u0) + (_w), (p)->v1 = (_v0), (p)->u2 = (_u0),                        \
    (p)->v2 = (_v0) + (_h), (p)->u3 = (_u0) + (_w), (p)->v3 = (_v0) + (_h)

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

typedef DR_MODE DR_TWIN;

/* VRAM upload packet; SetDrawLoad sets the tag and commands, leaving p untouched. */
typedef struct {
    u32 tag;
    u32 code[4];
    u32 p[12];
} DR_LOAD;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_PRIO;

/* Psy-Q 3.5 VRAM-to-VRAM move packet. */
typedef struct {
    u32 tag;
    u32 code;  /* 0x04: cache flush */
    u32 code2; /* 0x08: move command */
    s16 sx;    /* 0x0c: source */
    s16 sy;
    s16 x0; /* 0x10: destination */
    s16 y0;
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

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, x1, y1, x2, y2;
    u32 pad;
} LINE_F3;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 r2, g2, b2, p2;
    s16 x2, y2;
    u32 pad;
} LINE_G3;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, x1, y1, x2, y2, x3, y3;
    u32 pad;
} LINE_F4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 r2, g2, b2, p2;
    s16 x2, y2;
    u8 r3, g3, b3, p3;
    s16 x3, y3;
    u32 pad;
} LINE_G4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
} SPRT_8;

typedef SPRT_8 SPRT_16;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
} TILE_1;

typedef TILE_1 TILE_8;
typedef TILE_1 TILE_16;

/* Psy-Q decoded TMD primitive. Some fields are unused for a given primitive mode. */
typedef struct {
    u32 id;
    u8 r0, g0, b0, p0;
    u8 r1, g1, b1, p1;
    u8 r2, g2, b2, p2;
    u8 r3, g3, b3, p3;
    u16 tpage, clut;
    u8 u0, v0, u1, v1, u2, v2, u3, v3;
    SVECTOR x0, x1, x2, x3;
    SVECTOR n0, n1, n2, n3;
    SVECTOR* v_ofs;
    SVECTOR* n_ofs;
    u16 vert0, vert1, vert2, vert3;
    u16 norm0, norm1, norm2, norm3;
} TMD_PRIM;

typedef char assert_tmd_prim_size[sizeof(TMD_PRIM) == 120 ? 1 : -1];

#define FNT_MAX_ID   8
#define FNT_MAX_SPRT 1024

extern void FntLoad(int, int);
extern int FntOpen(int, int, int, int, int, int);
extern int FntPrint(char*, ...);
extern u32* FntFlush(int);
extern void SetDumpFnt(int);
extern s32 OpenTIM(u32*);
extern TIM_IMAGE* ReadTIM(TIM_IMAGE*);
extern u16 LoadTPage(u32*, int, int, int, int, int, int);
extern u16 LoadClut2(u32*, int, int);
extern void AddPrim(void*, void*);
void AddPrims(void* ot, void* first, void* last);
extern void SetSemiTrans(void*, int);
extern void SetShadeTex(void*, int);
extern void SetPolyF3(POLY_F3*);
extern void SetPolyFT3(POLY_FT3*);
extern void SetPolyG3(POLY_G3*);
extern void SetPolyGT3(POLY_GT3*);
extern void SetPolyF4(POLY_F4*);
extern void SetPolyFT4(POLY_FT4*);
extern void SetPolyG4(POLY_G4*);
extern void SetPolyGT4(POLY_GT4*);
extern void SetSprt8(SPRT_8*);
extern void SetSprt16(SPRT_16*);
extern void SetSprt(SPRT*);
extern void SetTile1(TILE_1*);
extern void SetTile8(TILE_8*);
extern void SetTile16(TILE_16*);
extern void SetTile(TILE*);
extern void SetLineF2(LINE_F2*);
extern void SetLineG2(LINE_G2*);
extern void SetLineF3(LINE_F3*);
extern void SetLineG3(LINE_G3*);
extern void SetLineF4(LINE_F4*);
extern void SetLineG4(LINE_G4*);
extern void SetBlockFill(TILE*);
extern void SetDrawMove(DR_MOVE*);
extern u16 GetTPage(int tp, int abr, int x, int y);
extern u16 GetClut(int x, int y);
extern void SetDrawMode(DR_MODE*, int, int, int, RECT*);
extern void SetDrawArea(DR_AREA*, RECT*);
extern void SetDispMask(int);
extern int ResetGraph(int);
extern int SetGraphDebug(int);
extern int GetGraphType(void);
extern psyq_interrupt_callback_t DrawSyncCallback(psyq_interrupt_callback_t callback);
extern int DrawSync(int);
extern int ClearImage(RECT*, u8, u8, u8);
extern u32* ClearOTag(u32*, int);
extern u32* ClearOTagR(u32*, int);
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
void SetDrawOffset(DR_OFFSET* destination, u16* position);

void DrawPrim(void* p);

void DumpTPage(u16 tpage);
void DumpClut(u16 clut);
int GetGraphDebug(void);
int SetGraphReverse(int reversed);
int SetGraphQueue(int enabled);
void DumpDrawEnv(DRAWENV* env);
void DumpDispEnv(DISPENV* env);
void SetDrawEnv(DR_ENV* packet, DRAWENV* env);
void DrawOTagEnv(u32* ot, DRAWENV* env);
DISPENV* GetDispEnv(DISPENV* env);
int GetODE(void);
void SetTexWindow(DR_TWIN* packet, RECT* window);
void SetPriority(DR_PRIO* packet, int test_mask, int set_mask);
void* NextPrim(void* primitive);
int IsEndPrim(void* primitive);
void CatPrim(void* primitive, void* next);
void TermPrim(void* primitive);
void SetDrawTPage(DR_TPAGE* p, int dfe, int dtd, int tpage);
void SetDrawLoad(DR_LOAD* primitive, RECT* rect);
int MargePrim(void* first, void* second);
int OpenTMD(u32* tmd, int object_index);
TMD_PRIM* ReadTMD(TMD_PRIM* primitive);

/* driver internals */

/* TIM identifier and palette-present field. */
#define PSYQ_TIM_ID         0x10U
#define PSYQ_TIM_HAS_CLUT   0x08U
#define PSYQ_TIM_CLUT_SHIFT 3

/* TIM palette and pixel blocks carry a byte length, VRAM rectangle and payload. */
typedef struct {
    u32 size_bytes;
    RECT rect;
    u32 data[0]; /* Variable-length 32-bit payload follows the 12-byte header. */
} psyq_tim_block_t;

typedef int (*psyq_gpu_operation_t)(void*, u32);
typedef struct {
    const char* name;
    int (*enqueue_three)(psyq_gpu_operation_t, void*, u32);
    int (*enqueue_four)(psyq_gpu_operation_t, void*, int, u32);
    psyq_gpu_operation_t clear;
    void (*control)(u32);
    int (*commands)(u32*, int);
    psyq_gpu_operation_t ordering_table;
    psyq_gpu_operation_t store;
    psyq_gpu_operation_t load;
    int (*process_queue)(void);
    u32 (*cached_control)(int);
    int (*clear_ot_reverse)(u32*, int);
    u32 (*information)(int);
    int (*reset)(int);
    u32 (*status)(void);
    int (*sync)(int);
} psyq_gpu_dispatch_t;

typedef struct {
    u8 graph_type;
    u8 queue_enabled;
    u8 debug_level;
    u8 reversed;
    s16 vram_width;
    s16 vram_height;
    s32 completion_pending;
    void (*draw_sync_callback)(void);
    DRAWENV draw;
    DISPENV display;
} psyq_gpu_environment_t;

extern psyq_gpu_environment_t g_psyq_gpu_environment;

extern psyq_gpu_dispatch_t* g_psyq_gpu_dispatch;
extern psyq_gpu_dispatch_t g_psyq_gpu_dispatch_table;
extern int (*GPU_printf)(const char*, ...);
extern volatile u32* g_psyq_gpu_gp0_port;
extern volatile u32* g_psyq_gpu_gp1_port;
extern volatile u32* g_psyq_gpu_dma_madr;
extern volatile u32* g_psyq_gpu_dma_bcr;
extern volatile u32* g_psyq_gpu_dma_chcr;
extern volatile u32* g_psyq_gpu_otc_dma_madr;
extern volatile u32* g_psyq_gpu_otc_dma_bcr;
extern volatile u32* g_psyq_gpu_otc_dma_chcr;
extern volatile u32* g_psyq_gpu_dma_dpcr;
extern u8 g_psyq_gpu_gp1_cache[256];
extern u32* g_psyq_gpu_tim_cursor;
/* font */
typedef struct {
    u16 palette[16];
    u8 _unused_020[480];
    u32 pixels[32][16];
} psyq_gpu_font_asset_t;

typedef char assert_gpu_font_asset_pixels_offset[offsetof(psyq_gpu_font_asset_t, pixels) == 512 ? 1 : -1];
typedef char assert_gpu_font_asset_size[sizeof(psyq_gpu_font_asset_t) == 2560 ? 1 : -1];

typedef struct {
    TILE tile;
    DR_MODE mode;
    int character_limit;
    SPRT_8* sprites;
    char* text;
    int text_count;
    int wrap_disabled;
} psyq_font_window_t;

extern int g_psyq_gpu_font_window_count;
extern int g_psyq_gpu_default_font_window;
extern psyq_gpu_font_asset_t g_psyq_gpu_font_asset;
extern psyq_font_window_t Font[FNT_MAX_ID];
extern int g_psyq_gpu_font_allocated_characters;
extern const char* g_psyq_gpu_font_hex_digits;
extern char g_psyq_gpu_font_text_pool[FNT_MAX_SPRT];
extern SPRT_8 g_psyq_gpu_font_sprite_pool[FNT_MAX_SPRT];
extern u16 g_psyq_gpu_font_tpage;
extern u16 g_psyq_gpu_font_clut;
extern int g_psyq_gpu_sync_deadline;
extern int g_psyq_gpu_sync_poll_count;
extern const int g_psyq_gpu_vram_width_lookup[5];
extern const int g_psyq_gpu_vram_height_lookup[5];
extern volatile int _qin;
extern volatile int _qout;
extern int g_psyq_gpu_reset_interrupt_mask;
typedef struct {
    psyq_gpu_operation_t operation;
    void* source;
    u32 argument;
} psyq_gpu_operation_snapshot_t;

typedef struct {
    psyq_gpu_operation_snapshot_t request;
    u32 packet_words[21];
} psyq_gpu_queue_entry_t;

typedef char assert_gpu_operation_snapshot_size[sizeof(psyq_gpu_operation_snapshot_t) == 12 ? 1 : -1];
typedef char assert_gpu_queue_entry_size[sizeof(psyq_gpu_queue_entry_t) == 96 ? 1 : -1];

enum {
    PSYQ_GPU_QUEUE_CAPACITY = 64,
    PSYQ_GPU_QUEUE_INDEX_MASK = PSYQ_GPU_QUEUE_CAPACITY - 1,
};

/* The DMA interrupt callback (_exeque) and the foreground share the queue. */
extern volatile psyq_gpu_queue_entry_t _que[PSYQ_GPU_QUEUE_CAPACITY];
extern int g_psyq_gpu_enqueue_interrupt_mask;
extern int g_psyq_gpu_queue_interrupt_mask;

u32 _status(void);
void _ctl(u32 command);
u32 _getctl(int command);
int _cwb(u32* commands, int count);
void _cwc(u32* ot);
u32 _param(int command);
void psyq_gpu_memset(void* destination, int value, int size);
int psyq_gpu_decode_tim(u32* tim, TIM_IMAGE* image);
int _addque(psyq_gpu_operation_t operation, void* source, u32 argument);
int _addque2(psyq_gpu_operation_t operation, void* source, int copy_size, u32 argument);
void set_alarm(void);
int _reset(int mode);
int psyq_gpu_detect_type(int mode);

extern const char g_psyq_gpu_tim_id_format[];
extern const char g_psyq_gpu_tim_mode_format[];
extern const char g_psyq_gpu_tim_address_format[];
extern const char g_psyq_gpu_rectangle_operation_format[];
extern const char g_psyq_gpu_clear_image_name[];
extern const char g_psyq_gpu_bad_rectangle_format[];
extern const char g_psyq_gpu_rectangle_format[];
extern const char g_psyq_gpu_load_image_name[];
extern const char g_psyq_gpu_store_image_name[];
extern const char g_psyq_gpu_move_image_name[];
extern const char g_psyq_gpu_draw_otag_format[];
extern const char g_psyq_gpu_draw_sync_format[];
extern const char g_psyq_gpu_draw_sync_callback_format[];
extern const char g_psyq_gpu_graph_debug_format[];
extern const char g_psyq_gpu_tpage_format[];
extern const char g_psyq_gpu_clut_format[];
extern const char g_psyq_gpu_draw_clip_format[];
extern const char g_psyq_gpu_draw_offset_format[];
extern const char g_psyq_gpu_texture_window_format[];
extern const char g_psyq_gpu_dithering_format[];
extern const char g_psyq_gpu_draw_display_enable_format[];
extern const char g_psyq_gpu_display_rectangle_format[];
extern const char g_psyq_gpu_screen_rectangle_format[];
extern const char g_psyq_gpu_interlace_format[];
extern const char g_psyq_gpu_rgb24_format[];
extern const char g_psyq_gpu_display_mask_format[];
extern const char g_psyq_gpu_clear_otag_format[];
extern const char g_psyq_gpu_clear_otag_reverse_format[];
extern const char g_psyq_gpu_graph_reverse_format[];
extern const char g_psyq_gpu_graph_queue_format[];
extern const char g_psyq_gpu_reset_addresses_format[];
extern const char g_psyq_gpu_reset_graph_format[];
extern const char g_psyq_gpu_put_draw_env_format[];
extern const char g_psyq_gpu_draw_otag_env_format[];
extern const char g_psyq_gpu_put_disp_env_format[];
extern u32 g_psyq_gpu_ot_terminator;

typedef struct {
    u32 tag;
    u32 command;
    u32 source;
    u32 destination;
    u32 dimensions;
} psyq_gpu_move_packet_t;

extern psyq_gpu_move_packet_t g_psyq_gpu_move_packet;

u32 get_mode(int dfe, int dtd, u16 tpage);
u32 get_cs(s16 x, s16 y);
u32 get_ce(s16 x, s16 y);
u32 get_ofs(s16 x, s16 y);
u32 get_tw(RECT* rect);
int get_dx(RECT* rect);
int get_alarm(void);
int _otc(u32* ot, int count);
void checkRECT(const char* name, RECT* rect);

typedef struct {
    u32 vertex_offset, vertex_count;
    u32 normal_offset, normal_count;
    u32 primitive_offset, primitive_count;
    u32 scale;
} psyq_tmd_object_t;

typedef struct {
    u32 id, flags, object_count;
    psyq_tmd_object_t objects[0]; /* Variable-length object table follows the 12-byte header. */
} psyq_tmd_header_t;

typedef union psyq_tmd_packet psyq_tmd_packet_t;

extern SVECTOR* g_psyq_gpu_tmd_vertices;
extern SVECTOR* g_psyq_gpu_tmd_normals;
extern psyq_tmd_packet_t* g_psyq_gpu_tmd_cursor;
extern int g_psyq_gpu_tmd_primitive_count;
extern const char g_psyq_gpu_tmd_decode_message[];
extern const char g_psyq_gpu_tmd_object_format[];
extern const char g_psyq_gpu_tmd_vertices_format[];
extern const char g_psyq_gpu_tmd_normals_format[];
extern const char g_psyq_gpu_tmd_primitives_format[];
int psyq_gpu_open_tmd_object(
    psyq_tmd_header_t* header, int object_index, psyq_tmd_packet_t** primitive, SVECTOR** vertices, SVECTOR** normals);
int psyq_gpu_decode_tmd_primitive(const psyq_tmd_packet_t* packet, TMD_PRIM* primitive);

extern volatile psyq_gpu_operation_snapshot_t _qlog;
extern const char g_psyq_gpu_timeout_state_format[];
extern const char g_psyq_gpu_timeout_operation_format[];
int _exeque(void);
int _sync(int mode);

/* One DMA tag covers either a block fill or a tile with drawing-state commands. */
typedef struct {
    u32 tag;
    u32 draw_mask;
    u32 fill_color;
    u32 xy;
    u32 wh;
} psyq_gpu_clear_fill_packet_t;

typedef struct {
    u32 tag;
    u32 draw_area_top_left;
    u32 draw_area_bottom_right;
    u32 draw_offset;
    u32 draw_mask;
    u32 tile_color;
    u32 xy;
    u32 wh;
} psyq_gpu_clear_tile_packet_t;

typedef union {
    psyq_gpu_clear_fill_packet_t fill;
    psyq_gpu_clear_tile_packet_t tile;
} psyq_gpu_clear_packet_t;

typedef struct {
    DR_AREA area;
    u32 draw_offset;
} psyq_gpu_clear_restore_packet_t;

extern psyq_gpu_clear_packet_t g_psyq_gpu_clear_packet;
extern psyq_gpu_clear_restore_packet_t g_psyq_gpu_clear_restore;
int _clr(RECT* rect, u32 color);
int _dws(RECT* rect, u32* data);
int _drs(RECT* rect, u32* data);

/* Input layouts used by the retail TMD converter; lengths include its header. */
typedef enum {
    PSYQ_TMD_PACKET_F3_LIT = 0x20000304,
    PSYQ_TMD_PACKET_G3_LIT = 0x30000406,
    PSYQ_TMD_PACKET_FT3_LIT = 0x24000507,
    PSYQ_TMD_PACKET_GT3_LIT = 0x34000609,
    PSYQ_TMD_PACKET_F3 = 0x21010304,
    PSYQ_TMD_PACKET_G3 = 0x31010506,
    PSYQ_TMD_PACKET_FT3 = 0x25010607,
    PSYQ_TMD_PACKET_GT3 = 0x35010809,
    PSYQ_TMD_PACKET_F4_LIT = 0x28000405,
    PSYQ_TMD_PACKET_G4_LIT = 0x38000508,
    PSYQ_TMD_PACKET_FT4_LIT = 0x2c000709,
    PSYQ_TMD_PACKET_GT4_LIT = 0x3c00080c,
    PSYQ_TMD_PACKET_F4 = 0x29010305,
    PSYQ_TMD_PACKET_G4 = 0x39010608,
    PSYQ_TMD_PACKET_FT4 = 0x2d010709,
    PSYQ_TMD_PACKET_GT4 = 0x3d010a0c,
} psyq_tmd_packet_id_e;

union psyq_tmd_packet {
    u32 id;
    TMD_P_F3 f3_lit;
    TMD_P_G3 g3_lit;
    TMD_P_TF3 ft3_lit;
    TMD_P_TG3 gt3_lit;
    TMD_P_NF3 f3;
    TMD_P_NG3 g3;
    TMD_P_TNF3 ft3;
    TMD_P_TNG3 gt3;
    TMD_P_F4 f4_lit;
    TMD_P_G4 g4_lit;
    TMD_P_TF4 ft4_lit;
    TMD_P_TG4 gt4_lit;
    TMD_P_NF4 f4;
    TMD_P_NG4 g4;
    TMD_P_TNF4 ft4;
    TMD_P_TNG4 gt4;
};

extern const char g_psyq_gpu_tmd_f3_lit_name[];
extern const char g_psyq_gpu_tmd_g3_lit_name[];
extern const char g_psyq_gpu_tmd_ft3_lit_name[];
extern const char g_psyq_gpu_tmd_gt3_lit_name[];
extern const char g_psyq_gpu_tmd_f3_name[];
extern const char g_psyq_gpu_tmd_g3_name[];
extern const char g_psyq_gpu_tmd_ft3_name[];
extern const char g_psyq_gpu_tmd_gt3_name[];
extern const char g_psyq_gpu_tmd_f4_lit_name[];
extern const char g_psyq_gpu_tmd_g4_lit_name[];
extern const char g_psyq_gpu_tmd_ft4_lit_name[];
extern const char g_psyq_gpu_tmd_gt4_lit_name[];
extern const char g_psyq_gpu_tmd_f4_name[];
extern const char g_psyq_gpu_tmd_g4_name[];
extern const char g_psyq_gpu_tmd_ft4_name[];
extern const char g_psyq_gpu_tmd_gt4_name[];
extern const char g_psyq_gpu_unsupported_tmd_format[];

#endif
