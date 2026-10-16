#include "psx/libgs.h"
#include "psx/types.h"

/* Emits a texture-page packet and SPRT primitive for the sprite, then links
 * the packet into the ordering table. */
void GsSortFastSprite(GsSPRITE* sp, GsOT* ot, u16 pri) {
    SPRT* packet;
    DR_TPAGE* draw_mode;
    GsOT_TAG* tag;
    u32 attr = sp->attribute;
    u32 shade;
    u32 sprite_code;
    u32 mode; /* semi-transparency (attribute bit 30) in the code byte */
    u32 red;
    u8 green;
    u8 blue;
    u32 dr_mode; /* the DR_MODE packet code itself */
    u32 tpage_value;
    u16 sprite_y;
    u16 sprite_x;
    u16 offset_x;
    u16 offset_y;
    u32 packet_addr; /* the packet's address as the OT tag's 24-bit link field */
    s32 sum_x;
    s32 sum_y;

    if (!(attr & GsDOFF) && sp->w != 0 && sp->h != 0) {
        draw_mode = (DR_TPAGE*)GsOUT_PACKET_P;
        /* Share one DMA tag: the 32-bit mode command occupies this primitive view's tag slot. */
        packet = (SPRT*)draw_mode->code;
        dr_mode = (PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MODE) | 0x200);
        packet_addr = (u32)draw_mode & PSYQ_GPU_DMA_ADDRESS_MASK;
        sprite_y = sp->y;
        sprite_x = sp->x;
        tpage_value = sp->tpage & 0x1F;
        draw_mode->code[0] = tpage_value | (((attr >> 17) & 0x180) | dr_mode) | ((attr >> 23) & 0x60);
        offset_x = GsORGOFSX;
        offset_y = GsORGOFSY;
        sum_x = sprite_x + offset_x;
        sum_y = sprite_y + offset_y;
        blue = sp->b;
        green = sp->g;
        red = sp->r;
        /* 32-bit GPU commands retain the packed 32-bit stores. */
        *(u32*)&packet->x0 = (sum_x & 0xFFFF) | (sum_y << 16);
        mode = (attr >> 5) & PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_SEMI_TRANSPARENT);
        shade = (attr << 18) & PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_RAW_TEXTURE);
        sprite_code = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_SPRT);
        *(u32*)&packet->r0 = mode | (shade | sprite_code) | (blue << 16) | (green << 8) | red;
        *(u32*)&packet->u0 = sp->u | (sp->v << 8) | (sp->cy << 22) | ((sp->cx << 12) & 0x3F0000);
        *(u32*)&packet->w = sp->w | (sp->h << 16);
        tag = &ot->org[pri] - ot->offset;
        /* Whole GPU tag transfers match; bitfield access adds masking and a read-modify-write. */
        draw_mode->tag = *(u32*)tag + ((PSYQ_GPU_PACKET_WORDS(SPRT) + 1) << 24);
        *(u32*)tag = packet_addr;
        GsOUT_PACKET_P = (PACKET*)(packet + 1);
    }
}
