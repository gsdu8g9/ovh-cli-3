#ifndef GRAPH_H

# define GRAPH_H

#include "struct/dptrarray.h"

typedef struct argument_t argument_t;
typedef argument_t graph_node_t;
typedef struct graph_t graph_t;

typedef bool (*complete_t)(const char *, size_t, DPtrArray *, void *);

argument_t *argument_create_choices(size_t, const char * const *);
argument_t *argument_create_literal(const char *, command_status_t (*) (void *, error_t **));
argument_t *argument_create_string(size_t, complete_t, void *);
bool complete_from_hashtable_keys(const char *, size_t, DPtrArray *, void *);
void graph_create_all_path(graph_node_t *, graph_node_t *, ...) SENTINEL;
void graph_create_full_path(graph_t *, graph_node_t *, ...) SENTINEL;
void graph_create_path(graph_t *, graph_node_t *, graph_node_t *, ...) SENTINEL;
void graph_destroy(graph_t *);
void graph_display(graph_t *);
graph_t *graph_new(void);
command_status_t graph_run_command(graph_t *, int, const char **, error_t **);
void graph_to_iterator(Iterator *, graph_t *, char **, int);

#endif /* !GRAPH_H */