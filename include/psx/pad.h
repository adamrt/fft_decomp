#ifndef FFT_PSX_PAD_H
#define FFT_PSX_PAD_H

typedef enum psx_pad_button {
    PSX_PAD_L2 = 0x0001,
    PSX_PAD_R2 = 0x0002,
    PSX_PAD_L1 = 0x0004,
    PSX_PAD_R1 = 0x0008,
    PSX_PAD_TRIANGLE = 0x0010,
    PSX_PAD_CIRCLE = 0x0020,
    PSX_PAD_CROSS = 0x0040,
    PSX_PAD_SQUARE = 0x0080,
    PSX_PAD_SELECT = 0x0100,
    PSX_PAD_START = 0x0800,
    PSX_PAD_UP = 0x1000,
    PSX_PAD_RIGHT = 0x2000,
    PSX_PAD_DOWN = 0x4000,
    PSX_PAD_LEFT = 0x8000,
    PSX_PAD_DPAD_MASK = 0xf000, /* UP | RIGHT | DOWN | LEFT */
} psx_pad_button_e;

/* libpad internal. */
void PAD_dr(void);

#endif
