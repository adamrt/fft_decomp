#include "psx/libgs.h"

/* Center the 2D offset on the screen, apply it to the draw buffer, and set the default near and
 * far 3D clip distances and lighting mode 0. */
void GsInit3D(void) {
    POSITION.offx = HWD0 / 2;
    POSITION.offy = VWD0 / 2;
    GsSetDrawBuffOffset();
    GsCLIP3near = 0xA;
    GsLIGHT_MODE = 0;
    GsCLIP3far = ZRESOLUTION;
}
