#ifndef JSON_H

# define JSON_H

# include "struct/xtring.h"

typedef uintptr_t json_value_t;

typedef enum {
    JSON_TYPE_NULL,
    JSON_TYPE_TRUE,
    JSON_TYPE_FALSE,
    JSON_TYPE_NUMBER,
    JSON_TYPE_STRING,
    JSON_TYPE_ARRAY,
    JSON_TYPE_OBJECT
} json_type_t;

#define JSON_OPT_PRETTY_PRINT (0<<1)

#define JSON_MAX_DEPTH 32

typedef struct {
    json_value_t root;
    int current_depth;
    int values_by_depth[JSON_MAX_DEPTH];
} json_document_t;

typedef struct {
    json_type_t type;
    void *value;
} json_node_t;

enum {
    JSON_CONSTANT_NULL  = 4, /* ... 0000 0100 */
    JSON_CONSTANT_TRUE  = 8, /* ... 0000 1000 */
    JSON_CONSTANT_FALSE = 16 /* ... 0001 0000 */
};

enum {
    JSON_INTEGER_MASK = 1, /* ... XXXX XX01  */
    JSON_UNUSED_MASK  = 2  /* ... XXXX XX10  */
};

#define json_null ((json_value_t) JSON_CONSTANT_NULL)
#define json_true ((json_value_t) JSON_CONSTANT_TRUE)
#define json_false ((json_value_t) JSON_CONSTANT_FALSE)

json_value_t json_array(void) WARN_UNUSED_RESULT;
void json_document_destroy(json_document_t *);
json_document_t *json_document_new(void) WARN_UNUSED_RESULT;
int json_document_serialize(json_document_t *, String *);
void json_document_set_root(json_document_t *, json_value_t);
json_value_t json_integer(int64_t) WARN_UNUSED_RESULT;
json_value_t json_number(double) WARN_UNUSED_RESULT;
json_value_t json_string(const char *) WARN_UNUSED_RESULT;
json_value_t json_object(void) WARN_UNUSED_RESULT;
bool json_object_get_property(json_value_t, const char *, json_value_t *);
bool json_object_has_property(json_value_t, const char *);
bool json_object_remove_property(json_value_t, const char *);
void json_object_set_property(json_value_t, const char *, json_value_t);

#endif /* !JSON_H */