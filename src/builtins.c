#include "../include/builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *copy_string(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

static int builtin_output(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1) { *error = "output() expects exactly 1 argument"; return -1; }
    char *s = value_to_display_string(args[0]);
    printf("%s\n", s);
    free(s);
    *out = make_none_value();
    return 1;
}

static int builtin_input(Value *args, int arg_count, Value *out, const char **error) {
    (void)args;
    if (arg_count != 0) { *error = "input() expects no arguments"; return -1; }
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) {
        *out = make_string_value("");
        return 1;
    }
    // strip trailing newline
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    *out = make_string_value(buf);
    return 1;
}

static int builtin_number(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1) { *error = "number() expects exactly 1 argument"; return -1; }
    if (args[0].type == VAL_NUMBER) { *out = args[0]; return 1; }
    if (args[0].type != VAL_STRING) {
        *error = "number() expects a string or number argument";
        return -1;
    }
    char *end;
    double result = strtod(args[0].as.string, &end);
    while (isspace((unsigned char)*end)) end++;
    if (end == args[0].as.string || *end != '\0') {
        *error = "number(): expected a valid number";
        return -1;
    }
    *out = make_number_value(result);
    return 1;
}

static int builtin_lowercase(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_STRING) {
        *error = "lowercase() expects exactly 1 string argument";
        return -1;
    }
    char *copy = copy_string(args[0].as.string);
    for (char *p = copy; *p; p++) *p = (char)tolower((unsigned char)*p);
    *out = make_string_value(copy);
    free(copy);
    return 1;
}

static int builtin_trim(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_STRING) {
        *error = "trim() expects exactly 1 string argument";
        return -1;
    }
    const char *s = args[0].as.string;
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && isspace((unsigned char)s[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;

    char *result = malloc(end - start + 1);
    memcpy(result, s + start, end - start);
    result[end - start] = '\0';
    *out = make_string_value(result);
    free(result);
    return 1;
}

static int builtin_length(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1) { *error = "length() expects exactly 1 argument"; return -1; }
    if (args[0].type == VAL_STRING) {
        *out = make_number_value((double)strlen(args[0].as.string));
        return 1;
    }
    if (args[0].type == VAL_ARRAY) {
        *out = make_number_value((double)args[0].as.array.count);
        return 1;
    }
    *error = "length() expects a string or array argument";
    return -1;
}

int call_builtin(const char *name, Value *args, int arg_count, Value *out,
                 const char **error_message) {
    if (strcmp(name, "output") == 0)    return builtin_output(args, arg_count, out, error_message);
    if (strcmp(name, "input") == 0)     return builtin_input(args, arg_count, out, error_message);
    if (strcmp(name, "number") == 0)    return builtin_number(args, arg_count, out, error_message);
    if (strcmp(name, "lowercase") == 0) return builtin_lowercase(args, arg_count, out, error_message);
    if (strcmp(name, "trim") == 0)     return builtin_trim(args, arg_count, out, error_message);
    if (strcmp(name, "length") == 0)   return builtin_length(args, arg_count, out, error_message);
    return 0; // not a built-in
}