#ifndef VESBO_VALUE_H
#define VESBO_VALUE_H

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_ARRAY,
    VAL_MAP,
    VAL_NONE,
} ValueType;

typedef struct Value Value;
typedef struct ValueMapEntry ValueMapEntry;
typedef struct ValueMap ValueMap;

typedef struct {
    Value *items;
    int count;
    int capacity;
} ValueArray;

struct Value {
    ValueType type;
    union {
        double number;
        char *string;
        int boolean;
        ValueArray array;
        ValueMap *map;
    } as;
};

struct ValueMapEntry {
    char *key;
    Value value;
};

struct ValueMap {
    ValueMapEntry *entries;
    int count;
    int capacity;
};

Value make_number_value(double n);
Value make_string_value(const char *s); // copies s
Value make_bool_value(int b);
Value make_none_value(void);
Value make_array_value(void); // empty array; use value_array_push to fill
Value make_map_value(void);   // empty map; use value_map_set to fill

void value_array_init(ValueArray *arr);
void value_array_push(ValueArray *arr, Value v);

void value_map_init(ValueMap *map);
int value_map_set(ValueMap *map, const char *key, Value v);
int value_map_get(ValueMap *map, const char *key, Value *out);
int value_map_has(ValueMap *map, const char *key);
int value_map_remove(ValueMap *map, const char *key);
void value_cleanup(void);

int value_is_truthy(Value v);
int values_equal(Value a, Value b);
char *value_to_display_string(Value v);
const char *value_type_name(Value v);

#endif