#ifndef PSX_STDDEF_H
#define PSX_STDDEF_H

/* Standard definitions the PS1 toolchain's headers would provide. */

#define NULL ((void*)0)

/* GCC 2.x has no __builtin_offsetof. unsigned long also holds host offsets
 * when the editor parses these headers outside the PS1 toolchain. */
#ifndef offsetof
#    define offsetof(type, member) ((unsigned long)&((type*)0)->member)
#endif

#endif
