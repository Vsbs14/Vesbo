#ifndef VESBO_MODULE_H
#define VESBO_MODULE_H

#include "ast.h"

void module_system_init(void);
void module_system_cleanup(void);

void module_set_current_file(const char *filepath);
const char *module_get_current_file(void);

char *module_resolve_path(const char *raw_path, const char *relative_to_file);

int module_is_imported(const char *canonical_path);
void module_mark_imported(const char *canonical_path);

int module_parse_file(const char *canonical_path, StmtList *out_ast, char **out_error);

#endif
