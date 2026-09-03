#ifndef VESBO_VALUE_H
#define VESBO_VALUE_H

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_ARRAY,
    VAL_NONE,
} ValueType;

typedef struct Value Value;

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
    } as;
};

Value make_number_value(double n);
Value make_string_value(const char *s); // copies s
Value make_bool_value(int b);
Value make_none_value(void);
Value make_array_value(void); // empty array; use value_array_push to fill

void value_array_init(ValueArray *arr);
void value_array_push(ValueArray *arr, Value v);
void value_cleanup(void);

int value_is_truthy(Value v);
int values_equal(Value a, Value b);
char *value_to_display_string(Value v);
const char *value_type_name(Value v);

#endif