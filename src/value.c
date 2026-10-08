#include "../include/value.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void **tracked_allocations = NULL;
static int tracked_count = 0;
static int tracked_capacity = 0;

static void track_allocation(void *allocation) {
    if (tracked_count >= tracked_capacity) {
        tracked_capacity = tracked_capacity == 0 ? 32 : tracked_capacity * 2;
        tracked_allocations = realloc(tracked_allocations,
                                       sizeof(void *) * tracked_capacity);
    }
    tracked_allocations[tracked_count++] = allocation;
}

static void *tracked_realloc(void *old_allocation, size_t size) {
    void *new_allocation = realloc(old_allocation, size);
    for (int i = 0; i < tracked_count; i++) {
        if (tracked_allocations[i] == old_allocation) {
            tracked_allocations[i] = new_allocation;
            return new_allocation;
        }
    }
    track_allocation(new_allocation);
    return new_allocation;
}

static char *copy_string(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

Value make_number_value(double n) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

Value make_string_value(const char *s) {
    Value v;
    v.type = VAL_STRING;
    v.as.string = copy_string(s);
    track_allocation(v.as.string);
    return v;
}

Value make_bool_value(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b ? 1 : 0;
    return v;
}

Value make_none_value(void) {
    Value v;
    v.type = VAL_NONE;
    return v;
}

Value make_array_value(void) {
    Value v;
    v.type = VAL_ARRAY;
    value_array_init(&v.as.array);
    return v;
}

Value make_map_value(void) {
    Value v;
    v.type = VAL_MAP;
    v.as.map = malloc(sizeof(ValueMap));
    track_allocation(v.as.map);
    value_map_init(v.as.map);
    return v;
}

void value_array_init(ValueArray *arr) {
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void value_array_push(ValueArray *arr, Value v) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->items = tracked_realloc(arr->items, sizeof(Value) * arr->capacity);
    }
    arr->items[arr->count++] = v;
}

void value_map_init(ValueMap *map) {
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
}

int value_map_set(ValueMap *map, const char *key, Value v) {
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].key, key) == 0) {
            map->entries[i].value = v;
            return 1;
        }
    }
    if (map->count >= map->capacity) {
        map->capacity = map->capacity == 0 ? 8 : map->capacity * 2;
        map->entries = tracked_realloc(map->entries, sizeof(ValueMapEntry) * map->capacity);
    }
    char *kcopy = copy_string(key);
    track_allocation(kcopy);
    map->entries[map->count].key = kcopy;
    map->entries[map->count].value = v;
    map->count++;
    return 1;
}

int value_map_get(ValueMap *map, const char *key, Value *out) {
    if (!map) return 0;
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].key, key) == 0) {
            if (out) *out = map->entries[i].value;
            return 1;
        }
    }
    return 0;
}

int value_map_has(ValueMap *map, const char *key) {
    return value_map_get(map, key, NULL);
}

int value_map_remove(ValueMap *map, const char *key) {
    if (!map) return 0;
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].key, key) == 0) {
            for (int j = i; j < map->count - 1; j++) {
                map->entries[j] = map->entries[j + 1];
            }
            map->count--;
            return 1;
        }
    }
    return 0;
}

int value_is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:   return v.as.boolean;
        case VAL_NONE:   return 0;
        case VAL_NUMBER: return v.as.number != 0;
        case VAL_STRING: return v.as.string[0] != '\0';
        case VAL_ARRAY:  return v.as.array.count > 0;
        case VAL_MAP:    return v.as.map && v.as.map->count > 0;
    }
    return 0;
}

int values_equal(Value a, Value b) {
    if (a.type != b.type) {
        // allow number/bool cross-comparison to fail cleanly rather than
        // crash; different types are simply never equal in v1
        return 0;
    }
    switch (a.type) {
        case VAL_NUMBER: return a.as.number == b.as.number;
        case VAL_STRING: return strcmp(a.as.string, b.as.string) == 0;
        case VAL_BOOL:   return a.as.boolean == b.as.boolean;
        case VAL_NONE:   return 1;
        case VAL_ARRAY:
            if (a.as.array.count != b.as.array.count) return 0;
            for (int i = 0; i < a.as.array.count; i++) {
                if (!values_equal(a.as.array.items[i], b.as.array.items[i])) return 0;
            }
            return 1;
        case VAL_MAP: {
            if (a.as.map == b.as.map) return 1;
            if (!a.as.map || !b.as.map) return 0;
            if (a.as.map->count != b.as.map->count) return 0;
            for (int i = 0; i < a.as.map->count; i++) {
                Value b_val;
                if (!value_map_get(b.as.map, a.as.map->entries[i].key, &b_val)) return 0;
                if (!values_equal(a.as.map->entries[i].value, b_val)) return 0;
            }
            return 1;
        }
    }
    return 0;
}

const char *value_type_name(Value v) {
    switch (v.type) {
        case VAL_NUMBER: return "number";
        case VAL_STRING: return "string";
        case VAL_BOOL:   return "boolean";
        case VAL_ARRAY:  return "array";
        case VAL_MAP:    return "map";
        case VAL_NONE:   return "none";
    }
    return "unknown";
}

char *value_to_display_string(Value v) {
    char buf[512];
    switch (v.type) {
        case VAL_NUMBER: {
            // Print integers without a trailing ".0"
            if (v.as.number == (long long)v.as.number) {
                snprintf(buf, sizeof(buf), "%lld", (long long)v.as.number);
            } else {
                snprintf(buf, sizeof(buf), "%g", v.as.number);
            }
            return copy_string(buf);
        }
        case VAL_STRING:
            return copy_string(v.as.string);
        case VAL_BOOL:
            return copy_string(v.as.boolean ? "true" : "false");
        case VAL_NONE:
            return copy_string("none");
        case VAL_ARRAY: {
            // Build "[a, b, c]"
            size_t cap = 256;
            char *out = malloc(cap);
            size_t len = 0;
            out[0] = '\0';

            #define APPEND(str) do { \
                size_t slen = strlen(str); \
                while (len + slen + 1 > cap) { cap *= 2; out = realloc(out, cap); } \
                memcpy(out + len, str, slen + 1); \
                len += slen; \
            } while (0)

            APPEND("[");
            for (int i = 0; i < v.as.array.count; i++) {
                if (i > 0) APPEND(", ");
                char *item_str = value_to_display_string(v.as.array.items[i]);
                APPEND(item_str);
                free(item_str);
            }
            APPEND("]");

            #undef APPEND
            return out;
        }
        case VAL_MAP: {
            // Build "{"key": val, ...}"
            size_t cap = 256;
            char *out = malloc(cap);
            size_t len = 0;
            out[0] = '\0';

            #define APPEND_MAP(str) do { \
                size_t slen = strlen(str); \
                while (len + slen + 1 > cap) { cap *= 2; out = realloc(out, cap); } \
                memcpy(out + len, str, slen + 1); \
                len += slen; \
            } while (0)

            APPEND_MAP("{");
            if (v.as.map) {
                for (int i = 0; i < v.as.map->count; i++) {
                    if (i > 0) APPEND_MAP(", ");
                    APPEND_MAP("\"");
                    APPEND_MAP(v.as.map->entries[i].key);
                    APPEND_MAP("\": ");
                    char *item_str = value_to_display_string(v.as.map->entries[i].value);
                    APPEND_MAP(item_str);
                    free(item_str);
                }
            }
            APPEND_MAP("}");

            #undef APPEND_MAP
            return out;
        }
    }
    return copy_string("");
}

void value_cleanup(void) {
    for (int i = 0; i < tracked_count; i++) free(tracked_allocations[i]);
    free(tracked_allocations);
    tracked_allocations = NULL;
    tracked_count = 0;
    tracked_capacity = 0;
}