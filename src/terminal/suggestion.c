/*
 * suggestion.c - Fish-style autosuggestion engine implementation
 *
 * Algorithm:
 * 1. Check command history for prefix match (most recent first)
 * 2. If no history match, try path completion for file/directory names
 * 3. Return suffix (the part after what user typed) as ghost text
 *
 * C99, handmade hero style.
 */

#include "suggestion.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ===== Helper Functions ===== */

/* Check if a string looks like a path (starts with /, ./, ../, or ~) */


/* Get the directory and partial filename from a path */
static void split_path(const char *path, char *dir, u32 dir_size,
                       char *partial, u32 partial_size) {
    if (!path || path[0] == '\0') {
        dir[0] = '\0';
        partial[0] = '\0';
        return;
    }
    
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        u32 dir_len = (u32)(last_slash - path + 1);
        if (dir_len >= dir_size) dir_len = dir_size - 1;
        strncpy(dir, path, dir_len);
        dir[dir_len] = '\0';
        
        strncpy(partial, last_slash + 1, partial_size - 1);
        partial[partial_size - 1] = '\0';
    } else {
        dir[0] = '\0';
        strncpy(partial, path, partial_size - 1);
        partial[partial_size - 1] = '\0';
    }
}

/* Expand ~ to home directory */
static void expand_tilde(const char *path, char *out, u32 out_size) {
    if (path[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(out, out_size, "%s%s", home, path + 1);
            return;
        }
    }
    strncpy(out, path, out_size - 1);
    out[out_size - 1] = '\0';
}

/* Try to find a matching file/directory for path completion */
static b32 try_path_completion(const char *partial_path, const char *cwd,
                               char *out_full, u32 out_size) {
    char expanded[512];
    char dir_path[256];
    char partial_name[256];
    
    expand_tilde(partial_path, expanded, sizeof(expanded));
    split_path(expanded, dir_path, sizeof(dir_path), 
               partial_name, sizeof(partial_name));
    
    /* If no directory specified, use cwd */
    char search_dir[1024];
    if (dir_path[0] == '\0') {
        strncpy(search_dir, cwd ? cwd : ".", sizeof(search_dir) - 1);
        search_dir[sizeof(search_dir) - 1] = '\0';
    } else if (dir_path[0] != '/') {
        /* Relative path - limit lengths to avoid truncation */
        int written = snprintf(search_dir, sizeof(search_dir), "%s/%s", 
                               cwd ? cwd : ".", dir_path);
        if (written < 0 || (size_t)written >= sizeof(search_dir)) {
            return false;  /* Path too long */
        }
    } else {
        strncpy(search_dir, dir_path, sizeof(search_dir) - 1);
        search_dir[sizeof(search_dir) - 1] = '\0';
    }
    
    /* If no partial name, don't suggest anything */
    if (partial_name[0] == '\0') {
        return false;
    }
    
    DIR *d = opendir(search_dir);
    if (!d) return false;
    
    u32 partial_len = (u32)strlen(partial_name);
    struct dirent *entry;
    char best_match[256] = {0};
    
    while ((entry = readdir(d)) != NULL) {
        /* Skip . and .. */
        if (entry->d_name[0] == '.' && 
            (entry->d_name[1] == '\0' || 
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        
        /* Check if name starts with partial */
        if (strncmp(entry->d_name, partial_name, partial_len) == 0) {
            /* Take first match (alphabetically first for consistency) */
            if (best_match[0] == '\0' || 
                strcmp(entry->d_name, best_match) < 0) {
                strncpy(best_match, entry->d_name, sizeof(best_match) - 1);
            }
        }
    }
    closedir(d);
    
    if (best_match[0] != '\0') {
        /* Reconstruct the full path suggestion */
        if (dir_path[0] != '\0') {
            int written = snprintf(out_full, out_size, "%s%s", dir_path, best_match);
            if (written < 0 || (size_t)written >= out_size) {
                return false;  /* Path too long */
            }
        } else {
            strncpy(out_full, best_match, out_size - 1);
            out_full[out_size - 1] = '\0';
        }
        
        /* Add trailing slash if it's a directory */
        char check_path[1280];  /* Large enough for search_dir + / + best_match */
        int written = snprintf(check_path, sizeof(check_path), "%s/%s", search_dir, best_match);
        if (written >= 0 && (size_t)written < sizeof(check_path)) {
            struct stat st;
            if (stat(check_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                u32 len = (u32)strlen(out_full);
                if (len < out_size - 1) {
                    out_full[len] = '/';
                    out_full[len + 1] = '\0';
                }
            }
        }
        
        return true;
    }
    
    return false;
}

/* Get the last "word" (argument) from input for path completion */
static const char* get_last_word(const char *input) {
    if (!input) return NULL;
    
    const char *last_space = strrchr(input, ' ');
    if (last_space) {
        return last_space + 1;
    }
    return input;
}

/* ===== Public API ===== */

SuggestionEngine* Suggestion_Create(const char *history_path) {
    SuggestionEngine *engine = calloc(1, sizeof(SuggestionEngine));
    if (!engine) return NULL;
    
    engine->history = History_Create(history_path);
    engine->current_cwd[0] = '\0';
    
    return engine;
}

void Suggestion_Destroy(SuggestionEngine *engine) {
    if (!engine) return;
    
    if (engine->history) {
        History_Destroy(engine->history);
    }
    free(engine);
}

void Suggestion_SetCWD(SuggestionEngine *engine, const char *cwd) {
    if (!engine) return;
    
    if (cwd) {
        strncpy(engine->current_cwd, cwd, sizeof(engine->current_cwd) - 1);
        engine->current_cwd[sizeof(engine->current_cwd) - 1] = '\0';
    } else {
        engine->current_cwd[0] = '\0';
    }
}

Suggestion Suggestion_Get(SuggestionEngine *engine, const char *input) {
    Suggestion sug = {0};
    sug.valid = false;
    
    if (!engine || !input || input[0] == '\0') {
        return sug;
    }
    
    u32 input_len = (u32)strlen(input);
    
    /* 1. Try history-based suggestion first */
    const char *history_match = History_SearchPrefix(engine->history, input);
    if (history_match) {
        strncpy(sug.full_text, history_match, sizeof(sug.full_text) - 1);
        strncpy(sug.suffix, history_match + input_len, sizeof(sug.suffix) - 1);
        sug.source = WB_SUGGESTION_HISTORY;
        sug.valid = true;
        return sug;
    }
    
    /* 2. Try path completion for the last word */
    const char *last_word = get_last_word(input);
    if (last_word) {
        char path_match[512];
        if (try_path_completion(last_word, engine->current_cwd, 
                                path_match, sizeof(path_match))) {
            u32 word_len = (u32)strlen(last_word);
            u32 match_len = (u32)strlen(path_match);
            
            if (match_len > word_len) {
                /* Build full suggestion by replacing last word */
                u32 prefix_len = (u32)(last_word - input);
                snprintf(sug.full_text, sizeof(sug.full_text), 
                         "%.*s%s", (int)prefix_len, input, path_match);
                strncpy(sug.suffix, path_match + word_len, sizeof(sug.suffix) - 1);
                sug.source = WB_SUGGESTION_PATH;
                sug.valid = true;
                return sug;
            }
        }
    }
    
    return sug;
}

void Suggestion_RecordCommand(SuggestionEngine *engine, const char *command) {
    if (!engine || !command) return;
    History_Add(engine->history, command);
}

const char* Suggestion_GetSuffix(Suggestion *sug) {
    if (!sug || !sug->valid) return NULL;
    if (sug->suffix[0] == '\0') return NULL;
    return sug->suffix;
}

void Suggestion_GetFirstWord(Suggestion *sug, char *out_word, u32 max_len) {
    if (!sug || !sug->valid || !out_word || max_len == 0) {
        if (out_word && max_len > 0) out_word[0] = '\0';
        return;
    }
    
    const char *suffix = sug->suffix;
    if (!suffix || suffix[0] == '\0') {
        out_word[0] = '\0';
        return;
    }
    
    /* Find end of first word (space or end of string) */
    u32 i = 0;
    
    /* Skip leading spaces */
    while (suffix[i] == ' ' && i < max_len - 1) {
        out_word[i] = suffix[i];
        i++;
    }
    
    /* Copy until space or end */
    while (suffix[i] != '\0' && suffix[i] != ' ' && i < max_len - 1) {
        out_word[i] = suffix[i];
        i++;
    }
    
    out_word[i] = '\0';
}
