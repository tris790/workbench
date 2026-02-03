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

/*
 * Fuzzy match result with scoring.
 * Higher scores indicate better matches (prefix, word boundaries, consecutive chars).
 */
typedef struct {
  b32 matches;   /* true if needle matches haystack */
  i32 score;     /* Match quality (higher is better) */
} fuzzy_match_result;

/*
 * Fuzzy match with scoring for ranking results.
 * Scoring rules:
 *   +100: Exact prefix match (haystack starts with needle)
 *   +50:  Match starts at word boundary (_ - . or after uppercase)
 *   +10:  Each consecutive character match
 *   +5:   Each character matched after a word boundary
 *   -1:   Each character skipped (preference for shorter strings)
 */
fuzzy_match_result FuzzyMatchScore(const char *needle, const char *haystack);

#endif /* FUZZY_MATCH_H */
