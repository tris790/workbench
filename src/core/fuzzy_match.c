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
