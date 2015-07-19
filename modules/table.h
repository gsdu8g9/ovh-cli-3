#ifndef TABLE_H

# define TABLE_H

# define TABLE_FLAG_NONE       (0)
# define TABLE_FLAG_NO_HEADERS (1<<0)

# define TABLE_TYPE_FLAGS 0xFF00

# define TABLE_TYPE(x) \
    ((x) & ~TABLE_TYPE_FLAGS)

# define TABLE_TYPE_DELEGATE 0x0100

typedef enum {
    TABLE_SORT_ASC,
    TABLE_SORT_DESC,
    _TABLE_SORT_COUNT,
} table_sort_t;

typedef enum {
    TABLE_TYPE_INT,
    TABLE_TYPE_INTEGER = TABLE_TYPE_INT,
    TABLE_TYPE_ENUM,
    TABLE_TYPE_STRING,
    TABLE_TYPE_BOOL,
    TABLE_TYPE_BOOLEAN = TABLE_TYPE_BOOL,
    TABLE_TYPE_DATE,
    TABLE_TYPE_DATETIME,
    _TABLE_TYPE_LAST = TABLE_TYPE_DATETIME
} column_type_t;

typedef struct table_t table_t;

void table_destroy(table_t *);
void table_display(table_t *, uint32_t);
table_t *table_new(size_t, ...);
void table_sort(table_t *, size_t, table_sort_t);
void table_store(table_t *, ...);

# include "model.h"

table_t *table_model_new(model_t);
void table_model_store(table_t *, char *);

#endif /* !TABLE_H */