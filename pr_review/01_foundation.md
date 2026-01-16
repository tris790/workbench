# WSH Code Review Phase 1: Foundation

## Focus Areas
- Core data structures and state management.
- Tokenization logic.
- Command parsing and AST generation.

## Files to Review
- `src/shell/wsh.h`
- `src/shell/wsh_main.c`
- `src/shell/wsh_state.h`
- `src/shell/wsh_state.c`
- `src/shell/wsh_tokenizer.h`
- `src/shell/wsh_tokenizer.c`
- `src/shell/wsh_parser.h`
- `src/shell/wsh_parser.c`

## Review Observations

### Architecture & State Management
- [x] `wsh_state_t` correctly encapsulates shell state (env, history, pager, abbreviations).
- [x] Environment management is $O(N)$ - acceptable but worth noting.
- [x] `WSH_State_Create` uses `memset` and `calloc` patterns correctly for initialization.
- [x] `WSH_State_SetEnv` lacks null-check for `strdup` results. (FIXED)

### Tokenizer
- [x] Correctly identifies shell operators (`|`, `>>`, `<`, etc.).
- [x] Basic support for single/double quotes and backslash escapes.
- [x] `WSH_Tokenize`: Unclosed quotes are not reported as errors. (FIXED)
- [x] Windows `strndup` implementation is functional but could be cleaner. (CLEANED)

### Parser
- [x] **CRITICAL**: Uses global static variables (`gl_tokens`, `gl_pos`). This makes the parser non-reentrant and fragile. (FIXED: Refactored to `WSH_Parser` struct)
- [x] `copy_val`: Simplistic quote removal only works if the *entire* word is wrapped in quotes. Fails for `hello"world"` or `\"escaped\"`. (FIXED: Implemented robust character-by-character unquoting)
- [x] Error handling for malformed pipelines (e.g., `ls | | grep`) is minimal (just stops parsing). (IMPROVED)
- [x] Missing memory allocation checks throughout `wsh_parser.c`. (FIXED: Added NULL checks)

## Suggested Improvements
1. **Refactor Parser**: (DONE) Pass `WSH_TokenList` and `gl_pos` as arguments or wrap in a `WSH_Parser` struct to remove global state.
2. **Robust Quoting**: (DONE) Implement a proper quote-unquote/backslash-processing pass in `copy_val` or during tokenization.
3. **Error Reporting**: (DONE) Return error codes or messages from `WSH_Tokenize` and `WSH_Parse` instead of just returning partial results.
4. **Resilience**: (DONE) Add `NULL` checks after `malloc`, `calloc`, and `realloc`.
