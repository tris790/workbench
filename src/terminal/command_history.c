/*
 * command_history.c - Command history implementation
 *
 * Ring buffer storage with prefix search for fish-style autosuggestions.
 * Persistent storage to file.
 * C99, handmade hero style.
 */

#include "command_history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Helper Functions ===== */

static u32 prev_index(CommandHistory *hist, u32 idx) {
    if (idx == 0) {
        return hist->capacity - 1;
    }
    return idx - 1;
}

static u32 next_index(CommandHistory *hist, u32 idx) {
    return (idx + 1) % hist->capacity;
}

/* Check if command should be stored (filters out empty/whitespace-only) */
static b32 is_valid_command(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return false;
    
    /* Skip whitespace-only commands */
    const char *p = cmd;
    while (*p) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            return true;
        }
        p++;
    }
    return false;
}

/* ===== Public API ===== */

CommandHistory* History_Create(const char *filepath) {
    CommandHistory *hist = calloc(1, sizeof(CommandHistory));
    if (!hist) return NULL;
    
    hist->capacity = HISTORY_MAX_ENTRIES;
    hist->entries = calloc(HISTORY_MAX_ENTRIES, HISTORY_MAX_LENGTH);
    if (!hist->entries) {
        free(hist);
        return NULL;
    }
    
    hist->count = 0;
    hist->head = 0;
    hist->dirty = false;
    
    if (filepath) {
        strncpy(hist->filepath, filepath, sizeof(hist->filepath) - 1);
        History_Load(hist);
    } else {
        /* Default path: ~/.workbench_history */
        const char *home = getenv("HOME");
        if (home) {
            snprintf(hist->filepath, sizeof(hist->filepath), 
                     "%s/%s", home, HISTORY_DEFAULT_FILE);
            History_Load(hist);
        }
    }
    
    /* Pre-populate with defaults if empty to ensure the feature is discoverable */
    if (hist->count == 0) {
        const char *defaults[] = {
            "ls -la",
            "git status",
            "git commit -m \"\"",
            "grep -r \"pattern\" .",
            "cd ..",
            "make clean",
            "ping google.com",
            "history",
            "exit"
        };
        for (u32 i = 0; i < sizeof(defaults)/sizeof(defaults[0]); i++) {
            History_Add(hist, defaults[i]);
        }
    }
    
    return hist;
}

void History_Destroy(CommandHistory *hist) {
    if (!hist) return;
    
    if (hist->dirty) {
        History_Save(hist);
    }
    
    if (hist->entries) {
        free(hist->entries);
    }
    free(hist);
}

void History_Add(CommandHistory *hist, const char *command) {
    if (!hist || !is_valid_command(command)) return;
    
    /* Check for duplicates in recent history (avoid adjacent duplicates) */
    if (hist->count > 0) {
        const char *last = hist->entries[hist->head];
        if (strcmp(last, command) == 0) {
            return; /* Don't add duplicate of most recent */
        }
    }
    
    /* Move head forward */
    hist->head = next_index(hist, hist->head);
    
    /* Store the command */
    strncpy(hist->entries[hist->head], command, HISTORY_MAX_LENGTH - 1);
    hist->entries[hist->head][HISTORY_MAX_LENGTH - 1] = '\0';
    
    /* Update count */
    if (hist->count < hist->capacity) {
        hist->count++;
    }
    
    hist->dirty = true;
}

const char* History_SearchPrefix(CommandHistory *hist, const char *prefix) {
    if (!hist || !prefix || prefix[0] == '\0') return NULL;
    
    u32 prefix_len = (u32)strlen(prefix);
    
    /* Search from most recent to oldest */
    u32 idx = hist->head;
    for (u32 i = 0; i < hist->count; i++) {
        const char *entry = hist->entries[idx];
        
        /* Check if entry starts with prefix */
        if (strncmp(entry, prefix, prefix_len) == 0) {
            /* Only return if there's actually more text after the prefix */
            if (strlen(entry) > prefix_len) {
                return entry;
            }
        }
        
        idx = prev_index(hist, idx);
    }
    
    return NULL;
}

const char* History_Get(CommandHistory *hist, u32 index) {
    if (!hist || index >= hist->count) return NULL;
    
    /* Index 0 = most recent (head) */
    u32 actual_idx = hist->head;
    for (u32 i = 0; i < index; i++) {
        actual_idx = prev_index(hist, actual_idx);
    }
    
    return hist->entries[actual_idx];
}

u32 History_Count(CommandHistory *hist) {
    if (!hist) return 0;
    return hist->count;
}

void History_Save(CommandHistory *hist) {
    if (!hist || hist->filepath[0] == '\0') return;
    
    FILE *f = fopen(hist->filepath, "w");
    if (!f) return;
    
    /* Write from oldest to newest */
    u32 start_idx = hist->head;
    for (u32 i = 0; i < hist->count; i++) {
        start_idx = prev_index(hist, start_idx);
    }
    start_idx = next_index(hist, start_idx);
    
    u32 idx = start_idx;
    for (u32 i = 0; i < hist->count; i++) {
        /* Escape newlines in commands (shouldn't happen often) */
        const char *entry = hist->entries[idx];
        if (entry[0] != '\0') {
            fprintf(f, "%s\n", entry);
        }
        idx = next_index(hist, idx);
    }
    
    fclose(f);
    hist->dirty = false;
}

void History_Load(CommandHistory *hist) {
    if (!hist || hist->filepath[0] == '\0') return;
    
    FILE *f = fopen(hist->filepath, "r");
    if (!f) return;
    
    char line[HISTORY_MAX_LENGTH];
    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        
        if (len > 0) {
            /* Direct add without duplicate check during load */
            hist->head = next_index(hist, hist->head);
            strncpy(hist->entries[hist->head], line, HISTORY_MAX_LENGTH - 1);
            hist->entries[hist->head][HISTORY_MAX_LENGTH - 1] = '\0';
            if (hist->count < hist->capacity) {
                hist->count++;
            }
        }
    }
    
    fclose(f);
    hist->dirty = false;
}
