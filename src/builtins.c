#include "../include/builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static char *copy_string(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
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
    // strip trailing newline and carriage return
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
    if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
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

static int builtin_uppercase(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_STRING) {
        *error = "uppercase() expects exactly 1 string argument";
        return -1;
    }
    char *copy = copy_string(args[0].as.string);
    for (char *p = copy; *p; p++) *p = (char)toupper((unsigned char)*p);
    *out = make_string_value(copy);
    free(copy);
    return 1;
}

static int builtin_string(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1) { *error = "string() expects exactly 1 argument"; return -1; }
    char *s = value_to_display_string(args[0]);
    *out = make_string_value(s);
    free(s);
    return 1;
}

static int builtin_push(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2) { *error = "push() expects exactly 2 arguments (array, value)"; return -1; }
    if (args[0].type != VAL_ARRAY) { *error = "push() first argument must be an array"; return -1; }
    Value arr = args[0];
    value_array_push(&arr.as.array, args[1]);
    *out = arr;
    return 1;
}

static int builtin_type_of(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1) { *error = "type_of() expects exactly 1 argument"; return -1; }
    *out = make_string_value(value_type_name(args[0]));
    return 1;
}

static int builtin_abs(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_NUMBER) {
        *error = "abs() expects exactly 1 number argument";
        return -1;
    }
    *out = make_number_value(fabs(args[0].as.number));
    return 1;
}

static int builtin_round(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_NUMBER) {
        *error = "round() expects exactly 1 number argument";
        return -1;
    }
    *out = make_number_value(round(args[0].as.number));
    return 1;
}

static int builtin_floor(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_NUMBER) {
        *error = "floor() expects exactly 1 number argument";
        return -1;
    }
    *out = make_number_value(floor(args[0].as.number));
    return 1;
}

static int builtin_ceil(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_NUMBER) {
        *error = "ceil() expects exactly 1 number argument";
        return -1;
    }
    *out = make_number_value(ceil(args[0].as.number));
    return 1;
}

static int builtin_min(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER) {
        *error = "min() expects exactly 2 number arguments";
        return -1;
    }
    *out = make_number_value(args[0].as.number < args[1].as.number ? args[0].as.number : args[1].as.number);
    return 1;
}

static int builtin_max(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER) {
        *error = "max() expects exactly 2 number arguments";
        return -1;
    }
    *out = make_number_value(args[0].as.number > args[1].as.number ? args[0].as.number : args[1].as.number);
    return 1;
}

static int builtin_pop(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_ARRAY) {
        *error = "pop() expects exactly 1 array argument";
        return -1;
    }
    if (args[0].as.array.count <= 0) {
        *error = "cannot pop from empty array";
        return -1;
    }
    *out = args[0].as.array.items[--args[0].as.array.count];
    return 1;
}

static int builtin_contains(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2) {
        *error = "contains() expects exactly 2 arguments (collection, item)";
        return -1;
    }
    if (args[0].type == VAL_ARRAY) {
        for (int i = 0; i < args[0].as.array.count; i++) {
            if (values_equal(args[0].as.array.items[i], args[1])) {
                *out = make_bool_value(1);
                return 1;
            }
        }
        *out = make_bool_value(0);
        return 1;
    } else if (args[0].type == VAL_STRING) {
        char *search_str;
        int need_free = 0;
        if (args[1].type == VAL_STRING) {
            search_str = args[1].as.string;
        } else {
            search_str = value_to_display_string(args[1]);
            need_free = 1;
        }
        int found = strstr(args[0].as.string, search_str) != NULL;
        if (need_free) free(search_str);
        *out = make_bool_value(found);
        return 1;
    }
    *error = "contains() first argument must be an array or string";
    return -1;
}

static int builtin_replace(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 3 || args[0].type != VAL_STRING || args[1].type != VAL_STRING || args[2].type != VAL_STRING) {
        *error = "replace() expects exactly 3 string arguments (str, old, new)";
        return -1;
    }
    const char *orig = args[0].as.string;
    const char *rep = args[1].as.string;
    const char *with = args[2].as.string;
    size_t rep_len = strlen(rep);
    size_t with_len = strlen(with);

    if (rep_len == 0) {
        *out = make_string_value(orig);
        return 1;
    }

    /* Count occurrences */
    int count = 0;
    const char *tmp = orig;
    const char *ins;
    while ((ins = strstr(tmp, rep)) != NULL) {
        count++;
        tmp = ins + rep_len;
    }

    size_t new_len = strlen(orig) + (with_len - rep_len) * count;
    char *result = malloc(new_len + 1);
    char *dest = result;
    tmp = orig;
    while ((ins = strstr(tmp, rep)) != NULL) {
        size_t part_len = ins - tmp;
        memcpy(dest, tmp, part_len);
        dest += part_len;
        memcpy(dest, with, with_len);
        dest += with_len;
        tmp = ins + rep_len;
    }
    strcpy(dest, tmp);
    *out = make_string_value(result);
    free(result);
    return 1;
}

static int builtin_split(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        *error = "split() expects exactly 2 string arguments (str, delim)";
        return -1;
    }
    const char *str = args[0].as.string;
    const char *delim = args[1].as.string;
    size_t delim_len = strlen(delim);

    Value arr = make_array_value();

    if (delim_len == 0) {
        /* Split into individual characters */
        for (size_t i = 0; str[i]; i++) {
            char ch[2] = { str[i], '\0' };
            value_array_push(&arr.as.array, make_string_value(ch));
        }
        *out = arr;
        return 1;
    }

    const char *start = str;
    const char *found;
    while ((found = strstr(start, delim)) != NULL) {
        size_t part_len = found - start;
        char *part = malloc(part_len + 1);
        memcpy(part, start, part_len);
        part[part_len] = '\0';
        value_array_push(&arr.as.array, make_string_value(part));
        free(part);
        start = found + delim_len;
    }
    value_array_push(&arr.as.array, make_string_value(start));
    *out = arr;
    return 1;
}

static int builtin_join(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_STRING) {
        *error = "join() expects exactly 2 arguments (array, delimiter)";
        return -1;
    }
    ValueArray *arr = &args[0].as.array;
    const char *delim = args[1].as.string;
    size_t delim_len = strlen(delim);

    if (arr->count == 0) {
        *out = make_string_value("");
        return 1;
    }

    char **parts = malloc(arr->count * sizeof(char *));
    size_t total_len = 0;
    for (int i = 0; i < arr->count; i++) {
        parts[i] = value_to_display_string(arr->items[i]);
        total_len += strlen(parts[i]);
    }
    total_len += delim_len * (arr->count - 1);

    char *result = malloc(total_len + 1);
    char *dest = result;
    for (int i = 0; i < arr->count; i++) {
        if (i > 0) {
            memcpy(dest, delim, delim_len);
            dest += delim_len;
        }
        size_t plen = strlen(parts[i]);
        memcpy(dest, parts[i], plen);
        dest += plen;
        free(parts[i]);
    }
    free(parts);
    *dest = '\0';
    *out = make_string_value(result);
    free(result);
    return 1;
}

int is_builtin(const char *name) {
    return strcmp(name, "output") == 0 ||
           strcmp(name, "input") == 0 ||
           strcmp(name, "number") == 0 ||
           strcmp(name, "lowercase") == 0 ||
           strcmp(name, "uppercase") == 0 ||
           strcmp(name, "trim") == 0 ||
           strcmp(name, "length") == 0 ||
           strcmp(name, "string") == 0 ||
           strcmp(name, "push") == 0 ||
           strcmp(name, "pop") == 0 ||
           strcmp(name, "type_of") == 0 ||
           strcmp(name, "abs") == 0 ||
           strcmp(name, "round") == 0 ||
           strcmp(name, "floor") == 0 ||
           strcmp(name, "ceil") == 0 ||
           strcmp(name, "min") == 0 ||
           strcmp(name, "max") == 0 ||
           strcmp(name, "contains") == 0 ||
           strcmp(name, "replace") == 0 ||
           strcmp(name, "split") == 0 ||
           strcmp(name, "join") == 0;
}

int call_builtin(const char *name, Value *args, int arg_count, Value *out,
                 const char **error_message) {
    if (strcmp(name, "output") == 0)    return builtin_output(args, arg_count, out, error_message);
    if (strcmp(name, "input") == 0)     return builtin_input(args, arg_count, out, error_message);
    if (strcmp(name, "number") == 0)    return builtin_number(args, arg_count, out, error_message);
    if (strcmp(name, "lowercase") == 0) return builtin_lowercase(args, arg_count, out, error_message);
    if (strcmp(name, "trim") == 0)     return builtin_trim(args, arg_count, out, error_message);
    if (strcmp(name, "length") == 0)   return builtin_length(args, arg_count, out, error_message);
    if (strcmp(name, "uppercase") == 0) return builtin_uppercase(args, arg_count, out, error_message);
    if (strcmp(name, "string") == 0)    return builtin_string(args, arg_count, out, error_message);
    if (strcmp(name, "push") == 0)      return builtin_push(args, arg_count, out, error_message);
    if (strcmp(name, "pop") == 0)       return builtin_pop(args, arg_count, out, error_message);
    if (strcmp(name, "type_of") == 0)   return builtin_type_of(args, arg_count, out, error_message);
    if (strcmp(name, "abs") == 0)       return builtin_abs(args, arg_count, out, error_message);
    if (strcmp(name, "round") == 0)     return builtin_round(args, arg_count, out, error_message);
    if (strcmp(name, "floor") == 0)     return builtin_floor(args, arg_count, out, error_message);
    if (strcmp(name, "ceil") == 0)      return builtin_ceil(args, arg_count, out, error_message);
    if (strcmp(name, "min") == 0)       return builtin_min(args, arg_count, out, error_message);
    if (strcmp(name, "max") == 0)       return builtin_max(args, arg_count, out, error_message);
    if (strcmp(name, "contains") == 0)  return builtin_contains(args, arg_count, out, error_message);
    if (strcmp(name, "replace") == 0)   return builtin_replace(args, arg_count, out, error_message);
    if (strcmp(name, "split") == 0)     return builtin_split(args, arg_count, out, error_message);
    if (strcmp(name, "join") == 0)      return builtin_join(args, arg_count, out, error_message);
    return 0; // not a built-in
}