/*
 * fuzzy_match.c - Fuzzy string matching implementation
 */

#include "fuzzy_match.h"

b32 FuzzyMatch(const char *needle, const char *haystack) {
  if (!needle || !haystack)
    return false;
  if (needle[0] == '\0')
    return true;

  const char *n = needle;
  const char *h = haystack;

  while (*h) {
    /* Match current needle char (case insensitive) */
    char nc = (*n >= 'A' && *n <= 'Z') ? *n + 32 : *n;
    char hc = (*h >= 'A' && *h <= 'Z') ? *h + 32 : *h;

    if (nc == hc) {
      n++;
      if (*n == '\0')
        return true;
    }
    h++;
  }
  return *n == '\0';
}

/* Helper to convert to lowercase */
static inline char ToLower(char c) {
  return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

/* Check if character is a word separator */
static inline b32 IsWordSeparator(char c) {
  return c == '_' || c == '-' || c == '.' || c == ' ' || c == '/' || c == '\\';
}

fuzzy_match_result FuzzyMatchScore(const char *needle, const char *haystack) {
  fuzzy_match_result result = {false, 0};
  
  if (!needle || !haystack)
    return result;
  if (needle[0] == '\0') {
    result.matches = true;
    return result;
  }

  /* Check for exact prefix match first */
  b32 is_prefix = true;
  const char *np = needle;
  const char *hp = haystack;
  while (*np) {
    if (ToLower(*np) != ToLower(*hp)) {
      is_prefix = false;
      break;
    }
    np++;
    hp++;
  }
  if (is_prefix) {
    result.matches = true;
    result.score = 1000; /* Big bonus for exact prefix */
    /* Slight preference for shorter strings */
    result.score -= (i32)(hp - haystack);
    return result;
  }

  const char *n = needle;
  const char *h = haystack;
  const char *last_match_pos = NULL;
  i32 consecutive_matches = 0;
  i32 chars_skipped = 0;

  while (*h && *n) {
    char nc = ToLower(*n);
    char hc = ToLower(*h);

    if (nc == hc) {
      /* Check if at word boundary */
      b32 at_boundary = (h == haystack) || IsWordSeparator(h[-1]);
      
      if (at_boundary) {
        result.score += 50;
        consecutive_matches = 0; /* Reset consecutive count at boundary */
      }
      
      /* Check if consecutive match */
      if (last_match_pos && h == last_match_pos + 1) {
        consecutive_matches++;
        result.score += 10 * consecutive_matches; /* Increasing bonus */
      } else {
        consecutive_matches = 1;
        result.score += 5; /* Base match bonus */
      }
      
      last_match_pos = h;
      n++;
    } else {
      chars_skipped++;
    }
    h++;
  }

  /* Check if all needle characters were matched */
  if (*n != '\0')
    return result; /* No match - score stays 0, matches stays false */

  result.matches = true;
  result.score -= chars_skipped; /* Penalty for skipping characters */
  
  return result;
}
