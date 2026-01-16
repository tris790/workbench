#ifndef WSH_PARSER_H
#define WSH_PARSER_H

#include "wsh_tokenizer.h"

typedef enum {
  WSH_NODE_JOB,
  WSH_NODE_PIPELINE,
  WSH_NODE_COMMAND,
  WSH_NODE_REDIRECT
} WSH_NodeType;

typedef struct WSH_ASTNode WSH_ASTNode;

// Forward decl
typedef struct WSH_Command WSH_Command;
typedef struct WSH_Pipeline WSH_Pipeline;

typedef struct WSH_Redirect {
  int mode; // 0: >, 1: >>, 2: <
  char *filename;
  struct WSH_Redirect *next;
} WSH_Redirect;

struct WSH_Command {
  char **args; // args[0] is program
  int argc;
  WSH_Redirect *redirects;
  struct WSH_Command *next; // Next in pipeline
};

struct WSH_Pipeline {
  WSH_Command *commands;
  bool background;
  struct WSH_Pipeline *next; // Next pipeline in job
};

typedef struct {
  WSH_Pipeline *pipelines;
} WSH_Job;

WSH_Job *WSH_Parse(const char *input);
void WSH_Job_Free(WSH_Job *job);

#endif
