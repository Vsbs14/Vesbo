#include "../include/builtins.h"
#include "../include/raylib_bindings.h"
#include "../include/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

typedef struct {
    void *owner;
    char *name;
    VesboNativeCallback fn;
    void *user_data;
} NativeEntry;

static NativeEntry *native_registry = NULL;
static int native_count = 0;
static int native_cap = 0;
static void *active_native_owner = NULL;

static char *copy_string(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

void builtins_set_active_owner(void *owner) {
    active_native_owner = owner;
}

void builtins_register_native_owned(void *owner, const char *name, VesboNativeCallback fn, void *user_data) {
    if (!name || !fn) return;
    for (int i = 0; i < native_count; i++) {
        if (native_registry[i].owner == owner && strcmp(native_registry[i].name, name) == 0) {
            native_registry[i].fn = fn;
            native_registry[i].user_data = user_data;
            return;
        }
    }
    if (native_count >= native_cap) {
        native_cap = native_cap == 0 ? 16 : native_cap * 2;
        native_registry = realloc(native_registry, (size_t)native_cap * sizeof(NativeEntry));
    }
    native_registry[native_count].owner = owner;
    native_registry[native_count].name = copy_string(name);
    native_registry[native_count].fn = fn;
    native_registry[native_count].user_data = user_data;
    native_count++;
}

void builtins_register_native(const char *name, VesboNativeCallback fn, void *user_data) {
    builtins_register_native_owned(NULL, name, fn, user_data);
}

void builtins_clear_natives_owned(void *owner) {
    int write_idx = 0;
    for (int i = 0; i < native_count; i++) {
        if (native_registry[i].owner == owner) {
            free(native_registry[i].name);
        } else {
            if (write_idx != i) {
                native_registry[write_idx] = native_registry[i];
            }
            write_idx++;
        }
    }
    native_count = write_idx;
    if (active_native_owner == owner) {
        active_native_owner = NULL;
    }
}

void builtins_clear_natives(void) {
    for (int i = 0; i < native_count; i++) {
        free(native_registry[i].name);
    }
    free(native_registry);
    native_registry = NULL;
    native_count = 0;
    native_cap = 0;
    active_native_owner = NULL;
}

static NativeEntry *find_native(const char *name) {
    if (active_native_owner) {
        for (int i = 0; i < native_count; i++) {
            if (native_registry[i].owner == active_native_owner &&
                strcmp(native_registry[i].name, name) == 0) {
                return &native_registry[i];
            }
        }
    }
    for (int i = 0; i < native_count; i++) {
        if (strcmp(native_registry[i].name, name) == 0) {
            return &native_registry[i];
        }
    }
    return NULL;
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
        *out = make_number_value((double)args[0].as.array->count);
        return 1;
    }
    if (args[0].type == VAL_MAP) {
        *out = make_number_value((double)args[0].as.map->count);
        return 1;
    }
    *error = "length() expects a string, array, or map argument";
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
    value_array_push(arr.as.array, args[1]);
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
    if (args[0].as.array->count <= 0) {
        *error = "cannot pop from empty array";
        return -1;
    }
    *out = args[0].as.array->items[--args[0].as.array->count];
    return 1;
}

static int builtin_contains(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2) {
        *error = "contains() expects exactly 2 arguments (collection, item)";
        return -1;
    }
    if (args[0].type == VAL_ARRAY) {
        for (int i = 0; i < args[0].as.array->count; i++) {
            if (values_equal(args[0].as.array->items[i], args[1])) {
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
    } else if (args[0].type == VAL_MAP) {
        int found;
        if (args[1].type == VAL_STRING) {
            found = value_map_has_obj(args[0].as.map, AS_OBJ_STRING(args[1]));
        } else {
            char *kstr = value_to_display_string(args[1]);
            found = value_map_has(args[0].as.map, kstr);
            free(kstr);
        }
        *out = make_bool_value(found);
        return 1;
    }
    *error = "contains() first argument must be an array, map, or string";
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

    int count = 0;
    const char *tmp = orig;
    const char *ins;
    while ((ins = strstr(tmp, rep)) != NULL) {
        count++;
        tmp = ins + rep_len;
    }

    size_t new_len = strlen(orig) + (with_len - rep_len) * (size_t)count;
    char *result = malloc(new_len + 1);
    char *dest = result;
    tmp = orig;
    while ((ins = strstr(tmp, rep)) != NULL) {
        size_t part_len = (size_t)(ins - tmp);
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
        for (size_t i = 0; str[i]; i++) {
            char ch[2] = { str[i], '\0' };
            value_array_push(arr.as.array, make_string_value(ch));
        }
        *out = arr;
        return 1;
    }

    const char *start = str;
    const char *found;
    while ((found = strstr(start, delim)) != NULL) {
        size_t part_len = (size_t)(found - start);
        char *part = malloc(part_len + 1);
        memcpy(part, start, part_len);
        part[part_len] = '\0';
        value_array_push(arr.as.array, make_string_value(part));
        free(part);
        start = found + delim_len;
    }
    value_array_push(arr.as.array, make_string_value(start));
    *out = arr;
    return 1;
}

static int builtin_join(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_STRING) {
        *error = "join() expects exactly 2 arguments (array, delimiter)";
        return -1;
    }
    ValueArray *arr = args[0].as.array;
    const char *delim = args[1].as.string;
    size_t delim_len = strlen(delim);

    if (arr->count == 0) {
        *out = make_string_value("");
        return 1;
    }

    char **parts = malloc((size_t)arr->count * sizeof(char *));
    size_t total_len = 0;
    for (int i = 0; i < arr->count; i++) {
        parts[i] = value_to_display_string(arr->items[i]);
        total_len += strlen(parts[i]);
    }
    total_len += delim_len * (size_t)(arr->count - 1);

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

static int builtin_keys(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_MAP) {
        *error = "keys() expects exactly 1 map argument";
        return -1;
    }
    Value arr = make_array_value();
    ValueMap *m = args[0].as.map;
    for (int i = 0; i < m->count; i++) {
        value_array_push(arr.as.array, make_obj_string_value(m->entries[i].key_obj));
    }
    *out = arr;
    return 1;
}

static int builtin_values(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_MAP) {
        *error = "values() expects exactly 1 map argument";
        return -1;
    }
    Value arr = make_array_value();
    ValueMap *m = args[0].as.map;
    for (int i = 0; i < m->count; i++) {
        value_array_push(arr.as.array, m->entries[i].value);
    }
    *out = arr;
    return 1;
}

static int builtin_has_key(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_MAP) {
        *error = "has_key() expects exactly 2 arguments (map, key)";
        return -1;
    }
    int found;
    if (args[1].type == VAL_STRING) {
        found = value_map_has_obj(args[0].as.map, AS_OBJ_STRING(args[1]));
    } else {
        char *kstr = value_to_display_string(args[1]);
        found = value_map_has(args[0].as.map, kstr);
        free(kstr);
    }
    *out = make_bool_value(found);
    return 1;
}

static int builtin_remove_key(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_MAP) {
        *error = "remove_key() expects exactly 2 arguments (map, key)";
        return -1;
    }
    int removed;
    if (args[1].type == VAL_STRING) {
        removed = value_map_remove_obj(args[0].as.map, AS_OBJ_STRING(args[1]));
    } else {
        char *kstr = value_to_display_string(args[1]);
        removed = value_map_remove(args[0].as.map, kstr);
        free(kstr);
    }
    *out = make_bool_value(removed);
    return 1;
}

static int builtin_read_file(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_STRING) {
        *error = "read_file() expects exactly 1 string argument (path)";
        return -1;
    }
    FILE *f = fopen(args[0].as.string, "rb");
    if (!f) {
        *error = "could not open file for reading";
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        *error = "could not determine file size";
        return -1;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        *error = "out of memory reading file";
        return -1;
    }

    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    buf[read_bytes] = '\0';
    fclose(f);

    *out = make_string_value(buf);
    free(buf);
    return 1;
}

static int builtin_write_file(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_STRING) {
        *error = "write_file() expects 2 arguments (path, content)";
        return -1;
    }
    char *content;
    int need_free = 0;
    if (args[1].type == VAL_STRING) {
        content = args[1].as.string;
    } else {
        content = value_to_display_string(args[1]);
        need_free = 1;
    }

    FILE *f = fopen(args[0].as.string, "wb");
    if (!f) {
        if (need_free) free(content);
        *error = "could not open file for writing";
        return -1;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    if (need_free) free(content);

    *out = make_bool_value(written == len);
    return 1;
}

static int builtin_append_file(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 2 || args[0].type != VAL_STRING) {
        *error = "append_file() expects 2 arguments (path, content)";
        return -1;
    }
    char *content;
    int need_free = 0;
    if (args[1].type == VAL_STRING) {
        content = args[1].as.string;
    } else {
        content = value_to_display_string(args[1]);
        need_free = 1;
    }

    FILE *f = fopen(args[0].as.string, "ab");
    if (!f) {
        if (need_free) free(content);
        *error = "could not open file for appending";
        return -1;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    if (need_free) free(content);

    *out = make_bool_value(written == len);
    return 1;
}

static int builtin_file_exists(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_STRING) {
        *error = "file_exists() expects exactly 1 string argument (path)";
        return -1;
    }
    FILE *f = fopen(args[0].as.string, "rb");
    if (f) {
        fclose(f);
        *out = make_bool_value(1);
    } else {
        *out = make_bool_value(0);
    }
    return 1;
}

static int builtin_remove_file(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_STRING) {
        *error = "remove_file() expects exactly 1 string argument (path)";
        return -1;
    }
    int res = remove(args[0].as.string);
    *out = make_bool_value(res == 0);
    return 1;
}

static int builtin_system(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_STRING) {
        *error = "system() expects exactly 1 string argument (command)";
        return -1;
    }
    int code = system(args[0].as.string);
    *out = make_number_value((double)code);
    return 1;
}

static int builtin_env(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_STRING) {
        *error = "env() expects exactly 1 string argument (variable name)";
        return -1;
    }
    const char *val = getenv(args[0].as.string);
    if (val) {
        *out = make_string_value(val);
    } else {
        *out = make_none_value();
    }
    return 1;
}

static int builtin_clock(Value *args, int arg_count, Value *out, const char **error) {
    (void)args;
    if (arg_count != 0) {
        *error = "clock() expects no arguments";
        return -1;
    }
    *out = make_number_value((double)clock() / (double)CLOCKS_PER_SEC);
    return 1;
}

static int builtin_time(Value *args, int arg_count, Value *out, const char **error) {
    (void)args;
    if (arg_count != 0) {
        *error = "time() expects no arguments";
        return -1;
    }
    *out = make_number_value((double)time(NULL));
    return 1;
}

static int builtin_copy(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1) {
        *error = "copy() expects exactly 1 argument";
        return -1;
    }
    *out = value_copy(args[0]);
    return 1;
}

static int builtin_struct_name(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1) {
        *error = "struct_name() expects exactly 1 argument";
        return -1;
    }
    if (args[0].type != VAL_STRUCT || !args[0].as.instance || !args[0].as.instance->struct_def) {
        *out = make_none_value();
        return 1;
    }
    *out = make_obj_string_value(args[0].as.instance->struct_def->name_obj);
    return 1;
}

static int builtin_coroutine_status(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_COROUTINE || !args[0].as.coroutine) {
        *error = "coroutine_status() expects a coroutine argument";
        return -1;
    }
    CoroState st = args[0].as.coroutine->state;
    const char *st_str = (st == CORO_RUNNING) ? "running" :
                         (st == CORO_DEAD)    ? "dead"    : "suspended";
    *out = make_string_value(st_str);
    return 1;
}

static int builtin_is_alive(Value *args, int arg_count, Value *out, const char **error) {
    if (arg_count != 1 || args[0].type != VAL_COROUTINE || !args[0].as.coroutine) {
        *error = "is_alive() expects a coroutine argument";
        return -1;
    }
    *out = make_bool_value(args[0].as.coroutine->state != CORO_DEAD);
    return 1;
}

static int builtin_gc_collect(Value *args, int arg_count, Value *out, const char **error) {
    (void)args;
    if (arg_count != 0) {
        *error = "gc_collect() expects no arguments";
        return -1;
    }
    gc_collect();
    *out = make_number_value((double)gc_get_object_count());
    return 1;
}

static int builtin_gc_stats(Value *args, int arg_count, Value *out, const char **error) {
    (void)args;
    if (arg_count != 0) {
        *error = "gc_stats() expects no arguments";
        return -1;
    }
    Value m = make_map_value();
    value_map_set(m.as.map, "instructions", make_number_value((double)gc_get_instructions()));
    value_map_set(m.as.map, "allocations", make_number_value((double)gc_get_total_allocations()));
    value_map_set(m.as.map, "total_allocations", make_number_value((double)gc_get_total_allocations()));
    value_map_set(m.as.map, "bytes_allocated", make_number_value((double)gc_get_total_bytes()));
    value_map_set(m.as.map, "total_bytes", make_number_value((double)gc_get_total_bytes()));
    value_map_set(m.as.map, "live_bytes", make_number_value((double)gc_get_live_bytes()));
    value_map_set(m.as.map, "live_objects", make_number_value((double)gc_get_object_count()));
    value_map_set(m.as.map, "collections", make_number_value((double)gc_get_collections()));
    value_map_set(m.as.map, "objects_freed", make_number_value((double)gc_get_objects_freed()));
    value_map_set(m.as.map, "freed_objects", make_number_value((double)gc_get_objects_freed()));
    *out = m;
    return 1;
}

static int builtin_stack_trace(Value *args, int arg_count, Value *out, const char **error) {
    (void)args;
    if (arg_count != 0) {
        *error = "stack_trace() expects no arguments";
        return -1;
    }
    *out = make_string_value(vesbo_get_last_stack_trace());
    return 1;
}

int is_builtin(const char *name) {
    if (find_native(name)) return 1;
    if (is_raylib_builtin(name)) return 1;
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
           strcmp(name, "join") == 0 ||
           strcmp(name, "keys") == 0 ||
           strcmp(name, "values") == 0 ||
           strcmp(name, "has_key") == 0 ||
           strcmp(name, "remove_key") == 0 ||
           strcmp(name, "read_file") == 0 ||
           strcmp(name, "write_file") == 0 ||
           strcmp(name, "append_file") == 0 ||
           strcmp(name, "file_exists") == 0 ||
           strcmp(name, "remove_file") == 0 ||
           strcmp(name, "system") == 0 ||
           strcmp(name, "env") == 0 ||
           strcmp(name, "clock") == 0 ||
           strcmp(name, "time") == 0 ||
           strcmp(name, "copy") == 0 ||
           strcmp(name, "struct_name") == 0 ||
           strcmp(name, "coroutine_status") == 0 ||
           strcmp(name, "status") == 0 ||
           strcmp(name, "is_alive") == 0 ||
           strcmp(name, "gc_collect") == 0 ||
           strcmp(name, "gc_stats") == 0 ||
           strcmp(name, "profile_stats") == 0 ||
           strcmp(name, "stack_trace") == 0;
}

int call_builtin(const char *name, Value *args, int arg_count, Value *out,
                 const char **error_message) {
    NativeEntry *nat = find_native(name);
    if (nat) {
        return nat->fn(nat->user_data, arg_count, args, out, error_message);
    }
    if (is_raylib_builtin(name)) {
        return call_raylib_builtin(name, args, arg_count, out, error_message);
    }
    if (strcmp(name, "output") == 0)    return builtin_output(args, arg_count, out, error_message);
    if (strcmp(name, "input") == 0)     return builtin_input(args, arg_count, out, error_message);
    if (strcmp(name, "number") == 0)    return builtin_number(args, arg_count, out, error_message);
    if (strcmp(name, "lowercase") == 0) return builtin_lowercase(args, arg_count, out, error_message);
    if (strcmp(name, "trim") == 0)      return builtin_trim(args, arg_count, out, error_message);
    if (strcmp(name, "length") == 0)    return builtin_length(args, arg_count, out, error_message);
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
    if (strcmp(name, "keys") == 0)      return builtin_keys(args, arg_count, out, error_message);
    if (strcmp(name, "values") == 0)    return builtin_values(args, arg_count, out, error_message);
    if (strcmp(name, "has_key") == 0)   return builtin_has_key(args, arg_count, out, error_message);
    if (strcmp(name, "remove_key") == 0) return builtin_remove_key(args, arg_count, out, error_message);
    if (strcmp(name, "read_file") == 0) return builtin_read_file(args, arg_count, out, error_message);
    if (strcmp(name, "write_file") == 0) return builtin_write_file(args, arg_count, out, error_message);
    if (strcmp(name, "append_file") == 0) return builtin_append_file(args, arg_count, out, error_message);
    if (strcmp(name, "file_exists") == 0) return builtin_file_exists(args, arg_count, out, error_message);
    if (strcmp(name, "remove_file") == 0) return builtin_remove_file(args, arg_count, out, error_message);
    if (strcmp(name, "system") == 0)    return builtin_system(args, arg_count, out, error_message);
    if (strcmp(name, "env") == 0)       return builtin_env(args, arg_count, out, error_message);
    if (strcmp(name, "clock") == 0)     return builtin_clock(args, arg_count, out, error_message);
    if (strcmp(name, "time") == 0)      return builtin_time(args, arg_count, out, error_message);
    if (strcmp(name, "copy") == 0)      return builtin_copy(args, arg_count, out, error_message);
    if (strcmp(name, "struct_name") == 0) return builtin_struct_name(args, arg_count, out, error_message);
    if (strcmp(name, "coroutine_status") == 0 || strcmp(name, "status") == 0)
        return builtin_coroutine_status(args, arg_count, out, error_message);
    if (strcmp(name, "is_alive") == 0)  return builtin_is_alive(args, arg_count, out, error_message);
    if (strcmp(name, "gc_collect") == 0) return builtin_gc_collect(args, arg_count, out, error_message);
    if (strcmp(name, "gc_stats") == 0 || strcmp(name, "profile_stats") == 0)
        return builtin_gc_stats(args, arg_count, out, error_message);
    if (strcmp(name, "stack_trace") == 0) return builtin_stack_trace(args, arg_count, out, error_message);
    return 0;
}