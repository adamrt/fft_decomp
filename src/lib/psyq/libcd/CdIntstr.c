#include "psx/libcd.h"
/* Return a CD interrupt type's debug name, or the unknown name past the 7 known types. */
char* CdIntstr(u8 interrupt) {
    return interrupt < 7 ? CD_intstr[interrupt] : g_psyq_cd_unknown_name;
}
