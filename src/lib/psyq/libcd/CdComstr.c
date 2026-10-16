#include "psx/libcd.h"
/* Return a CD command's debug name, or the unknown name past the 28 known commands. */
char* CdComstr(u8 command) {
    return command < 28 ? CD_comstr[command] : g_psyq_cd_unknown_name;
}
