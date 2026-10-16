/* Whether a roster unit stands on one of the current squad's deployment tiles. */

#include "fft/deploy.h"
#include "psx/types.h"

/* Return 1 when the roster unit stands on one of the current squad's 5x5 deployment tiles.
 * ATTACK.OUT links this body twice, the second copy as deploy_has_roster_unit_on_tiles. */
s32 deploy_is_roster_unit_deployed(s32 roster_id) {
    s32 row;
    s32 column;

    for (row = 0; row < 5; row++) {
        for (column = 0; column < 5; column++) {
            if (g_deploy_roster_id_by_tile[row][column] == roster_id) {
                return 1;
            }
        }
    }
    return 0;
}
