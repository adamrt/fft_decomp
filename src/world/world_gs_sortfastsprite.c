#include "fft/world.h"
#include "psx/gs.h"
#include "psx/types.h"

/* Emits a texture-page packet and SPRT primitive for the sprite, then links
 * the packet into the ordering table. */
void world_gs_sortfastsprite(const GsSPRITE* sp, GsOT* ot, u16 pri) {
    u32* packet;
    u32* tag;
    u32 attr;
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
    u32 packet_addr; /* the packet's address as the OT tag's 24-bit link field */
    s32 sum_x;
    s32 sum_y;

    attr = sp->attribute;
    if ((s32)attr >= 0 && sp->w != 0 && sp->h != 0) {
        packet = g_world_gs_out_packet_p;
        dr_mode = 0xE1000200;
        packet_addr = (u32)packet & 0xFFFFFF;
        sprite_y = sp->y;
        sprite_x = sp->x;
        tpage_value = sp->tpage & 0x1F;
        packet[1] = tpage_value | (((attr >> 17) & 0x180) | dr_mode) | ((attr >> 23) & 0x60);
        sum_x = sprite_x + *(u16*)&g_world_gs_offset_x;
        sum_y = sprite_y + *(u16*)&g_world_gs_offset_y;
        blue = sp->b;
        green = sp->g;
        red = sp->r;
        packet[3] = (sum_x & 0xFFFF) | (sum_y << 16);
        mode = (attr >> 5) & 0x02000000;
        shade = (attr << 18) & 0x01000000;
        sprite_code = 0x64000000;
        packet[2] = mode | (shade | sprite_code) | (blue << 16) | (green << 8) | red;
        packet[4] = sp->u | (sp->v << 8) | (sp->cy << 22) | ((sp->cx << 12) & 0x3F0000);
        packet[5] = sp->w | (sp->h << 16);
        tag = &ot->org[pri] - ot->offset;
        packet[0] = *tag + 0x05000000;
        *tag = packet_addr;
        g_world_gs_out_packet_p = packet + 6;
    }
}
