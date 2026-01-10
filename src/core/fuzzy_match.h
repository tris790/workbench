/*
 * fuzzy_match.h - Fuzzy string matching utility
 *
 * C99, handmade hero style.
 */

#ifndef FUZZY_MATCH_H
#define FUZZY_MATCH_H

#include "types.h"

/*
 * Simple case-insensitive subsequence match.
 * Returns true if all characters in 'needle' appear in 'haystack' in order.
 */
b32 FuzzyMatch(const char *needle, const char *haystack);

#endif /* FUZZY_MATCH_H */
