#ifndef WSH_TOKENIZER_H
#define WSH_TOKENIZER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  WSH_TOKEN_WORD,
  WSH_TOKEN_PIPE,         // |
  WSH_TOKEN_REDIRECT_OUT, // >
  WSH_TOKEN_REDIRECT_APP, // >>
  WSH_TOKEN_REDIRECT_IN,  // <
  WSH_TOKEN_BACKGROUND,   // &
  WSH_TOKEN_SEMICOLON,    // ;
  WSH_TOKEN_END
} WSH_TokenType;

#define WSH_TOKENIZE_OK 0
#define WSH_TOKENIZE_ERR_UNCLOSED_QUOTE 1
#define WSH_TOKENIZE_ERR_NOMEM 2

typedef struct {
  WSH_TokenType type;
  char *value;
  size_t start_pos; // Index in original string
  size_t len;
} WSH_Token;

typedef struct {
  WSH_Token *tokens;
  size_t count;
  size_t capacity;
} WSH_TokenList;

void WSH_TokenList_Init(WSH_TokenList *list);
void WSH_TokenList_Free(WSH_TokenList *list);
void WSH_TokenList_Add(WSH_TokenList *list, WSH_TokenType type,
                       const char *value, size_t start, size_t len);

// Main tokenization function
// Returns 0 on success, non-zero on error (e.g. unclosed quote)
int WSH_Tokenize(const char *input, WSH_TokenList *out_list);

#endif
