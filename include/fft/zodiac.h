#ifndef FFT_ZODIAC_H
#define FFT_ZODIAC_H

/* Unshifted zodiac IDs. Party and battle records store these in the high
 * nibble; ordinary birthdays never produce Serpentarius. */
typedef enum zodiac_sign {
    ZODIAC_SIGN_ARIES = 0,
    ZODIAC_SIGN_TAURUS = 1,
    ZODIAC_SIGN_GEMINI = 2,
    ZODIAC_SIGN_CANCER = 3,
    ZODIAC_SIGN_LEO = 4,
    ZODIAC_SIGN_VIRGO = 5,
    ZODIAC_SIGN_LIBRA = 6,
    ZODIAC_SIGN_SCORPIO = 7,
    ZODIAC_SIGN_SAGITTARIUS = 8,
    ZODIAC_SIGN_CAPRICORN = 9,
    ZODIAC_SIGN_AQUARIUS = 10,
    ZODIAC_SIGN_PISCES = 11,
    ZODIAC_SIGN_SERPENTARIUS = 12,
} zodiac_sign_e;

enum {
    ZODIAC_SIGN_ORDINARY_COUNT = 12,
    ZODIAC_SIGN_COUNT = 13,
};

/* The compatibility table is indexed by absolute zodiac-ID difference, not
 * by a particular sign. Value 3 is resolved from sex and monster flags. */
typedef enum zodiac_compatibility {
    ZODIAC_COMPATIBILITY_NEUTRAL = 0,
    ZODIAC_COMPATIBILITY_BAD = 1,
    ZODIAC_COMPATIBILITY_GOOD = 2,
    ZODIAC_COMPATIBILITY_GENDER_DEPENDENT = 3,
    ZODIAC_COMPATIBILITY_WORST = 4,
    ZODIAC_COMPATIBILITY_BEST = 5,
} zodiac_compatibility_e;

#endif
