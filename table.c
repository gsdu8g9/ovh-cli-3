#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "table.h"
#include "modules/conv.h"
#include "struct/dptrarray.h"

typedef struct {
    const char *title;
    column_type_t type;
    size_t min_len, max_len, len;
} column_t;

struct table_t {
    size_t columns_count;
    column_t *columns;
    DPtrArray *rows;
};

typedef struct {
    size_t l;
    size_t b;
    uintptr_t v;
} value_t;

typedef struct {
    value_t *values;
} row_t;

#define DEFAULT_WIDTH 80
static int console_width(void)
{
    int columns;

    if (isatty(STDOUT_FILENO)) {
        struct winsize w;

        if (NULL != getenv("COLUMNS") && 0 != atoi(getenv("COLUMNS"))) {
            columns = atoi(getenv("COLUMNS"));
        } else if (-1 != ioctl(STDOUT_FILENO, TIOCGWINSZ, &w)) {
            columns = w.ws_col;
        } else {
            columns = DEFAULT_WIDTH;
        }
    } else {
        columns = -1; // unlimited
    }

    return columns;
}

#include <wchar.h>
#include <locale.h>
static bool cplen(const char *string, size_t *string_len, error_t **error)
{
    size_t cp_len;
    mbstate_t mbstate;

    assert(NULL != string_len);

    *string_len = 0;
    setlocale(LC_ALL, "");
    bzero(&mbstate, sizeof(mbstate));
    while ((cp_len = mbrlen(string, MB_CUR_MAX, &mbstate)) > 0) {
        if (((size_t) -1) == cp_len || ((size_t) -2) == cp_len) {
            break;
        }
        string += cp_len;
        ++*string_len;
    }
    if (((size_t) -1) == cp_len) {
        // An encoding error has occurred. The next n or fewer bytes do not contribute to a valid multibyte character.
        error_set(error, FATAL, "mbrlen: (size_t) -1 /* TODO */");
    } else if (((size_t) -2) == cp_len) {
        // The next n (MB_CUR_MAX) contribute to, but do not complete, a valid multibyte character sequence, and all n bytes have been processed
        error_set(error, FATAL, "mbrlen: (size_t) -2 /* TODO */");
    }

    return 0 == cp_len;
}

static void row_destroy(void *data)
{
    row_t *r;

    r = (row_t *) data;
    free(r->values);
    free(r);
}

table_t *table_new(size_t columns_count, ...)
{
    size_t i;
    table_t *t;
    va_list ap;

    t = mem_new(*t);
    t->columns_count = columns_count;
    t->columns = mem_new_n(*t->columns, columns_count);
    t->rows = dptrarray_new(NULL, row_destroy, NULL);
    // string "title", int type
    va_start(ap, columns_count);
    for (i = 0; i < columns_count; i++) {
        t->columns[i].title = va_arg(ap, const char *);
        t->columns[i].type = va_arg(ap, column_type_t);
        t->columns[i].len = t->columns[i].min_len = t->columns[i].max_len = strlen(t->columns[i].title);
    }
    va_end(ap);

    return t;
}

void table_destroy(table_t *t)
{
    assert(NULL != t);

    free(t->columns);
    dptrarray_destroy(t->rows);
    free(t);
}

void table_store(table_t *t, ...)
{
    size_t i;
    row_t *r;
    va_list ap;

    assert(NULL != t);

    r = mem_new(*r);
    r->values = mem_new_n(*r->values, t->columns_count);
    va_start(ap, t);
    for (i = 0; i < t->columns_count; i++) {
        switch (t->columns[i].type) {
            case TABLE_TYPE_STRING:
            {
                size_t s_len;
                const char *s;
                error_t *error;

                error = NULL;
                s = va_arg(ap, const char *);
                cplen(s, &s_len, &error);
                print_error(error);
                r->values[i].v = (uintptr_t) s; // TODO: conversion UTF-8 => local
                r->values[i].l = s_len;
                if (s_len > t->columns[i].max_len) {
                    t->columns[i].max_len = s_len;
                }
                break;
            }
            case TABLE_TYPE_INT:
            {
                int v;
                size_t len;

                v = va_arg(ap, int);
                len = snprintf(NULL, 0, "%d", v);
                r->values[i].v = (uintptr_t) v;
                r->values[i].l = len;
                if (len > t->columns[i].max_len) {
                    t->columns[i].len = t->columns[i].min_len = t->columns[i].max_len = len;
                }
                break;
            }
            default:
                assert(FALSE);
                break;
        }
    }
    va_end(ap);
    dptrarray_push(t->rows, r);
}

static void table_print_separator_line(table_t *t)
{
    size_t i, j;

    putchar('+');
    for (i = 0; i < t->columns_count; i++) {
        for (j = 0; j < t->columns[i].len + 2; j++) {
            putchar('-');
        }
        putchar('+');
    }
    putchar('\n');
}

typedef struct {
    size_t charlen;
    const char *part;
} break_t;

static size_t string_break(size_t max_len, const char *string, size_t string_len, break_t **breaks)
{
    size_t i, breaks_count;

    i = 0;
    breaks_count = (string_len / max_len) + 1; // NOTE: string_len unit is character
    *breaks = mem_new_n(**breaks, breaks_count);
    if (breaks_count > 1) {
        const char *p;
        size_t start, end; // byte index

        p = string;
        start = end = 0;
        while (i < breaks_count) {
            size_t cp_len, cp_count;

            (*breaks)[i].charlen = 0;
            for (cp_count = 0; cp_count < max_len && (cp_len = mblen(p, MB_CUR_MAX)) > 0; cp_count++) {
                p += cp_len;
                end += cp_len;
                ++(*breaks)[i].charlen;
            }
            (*breaks)[i++].part = strndup(string + start, end - start);
            start = end;
        }
    } else {
        (*breaks)[i].charlen = string_len;
        (*breaks)[i++].part = string; // don't free breaks[0].part if string == breaks[0].part (or 1 == breaks_count)
    }

    return breaks_count;
}

void table_display(table_t *t, uint32_t flags)
{
    size_t i;
    Iterator it;
    int width;
//     wchar_t s[] = L"\xF0\x9D\x9F\x8E";

    assert(NULL != t);
// debug("wcwidth = %d", wcwidth(s[0]));

    // wcswidth / wcwidth
//     if (dptrarray_size(t->rows) > 0) {
    width = console_width();
    if (width > 0) {
        int min_len_sum, candidates_for_growth;

        candidates_for_growth = min_len_sum = 0;
        for (i = 0; i < t->columns_count; i++) {
            min_len_sum += t->columns[i].min_len;
            candidates_for_growth += t->columns[i].max_len > t->columns[i].min_len;
        }
        min_len_sum += STR_LEN("| ") + STR_LEN(" | ") * (t->columns_count - 1) + STR_LEN(" |");
        if (min_len_sum < width) {
            int diff;

            diff = width - min_len_sum;
            for (i = 0; i < t->columns_count; i++) {
                if (t->columns[i].max_len > t->columns[i].min_len) {
                    t->columns[i].len = t->columns[i].min_len + diff / candidates_for_growth;
                }
            }
        }
    }
    if (!HAS_FLAG(flags, TABLE_FLAG_NO_HEADERS)) {
        // +--- ... ---+
        table_print_separator_line(t);
        // | ... | : column headers
        putchar('|');
        for (i = 0; i < t->columns_count; i++) {
            printf(" %-*s |", (int) t->columns[i].len, t->columns[i].title);
        }
        putchar('\n');
    }
    // +--- ... ---+
    table_print_separator_line(t);
    // | ... | : data
    dptrarray_to_iterator(&it, t->rows);
    for (iterator_first(&it); iterator_is_valid(&it); iterator_next(&it)) {
        row_t *r;
        size_t j, lines_needed;
#define MAX_COLUMNS 12
        break_t *breaks[MAX_COLUMNS]; // si alloué dynamiquement, le remonter dans la fonction
        size_t breaks_count[MAX_COLUMNS];

        lines_needed = 1;
        r = iterator_current(&it, NULL);
        bzero(&breaks_count, sizeof(breaks_count));
        for (i = 0; i < t->columns_count; i++) {
            /*if (r->values[i].l > t->columns[i].len && (r->values[i].l / t->columns[i].len + 1) > lines_needed) {
                lines_needed = r->values[i].l / t->columns[i].len + 1;
            }*/
            switch (t->columns[i].type) {
                case TABLE_TYPE_STRING:
                    breaks_count[i] = string_break(t->columns[i].len, (const char *) r->values[i].v, r->values[i].l, &breaks[i]);
                    if (breaks_count[i] > lines_needed) {
                        lines_needed = breaks_count[i];
                    }
                    break;
                case TABLE_TYPE_INT:
                    /* NOP */
                    break;
                default:
                    assert(FALSE);
                    break;
            }
        }
        for (j =  0; j < lines_needed; j++) {
            putchar('|');
            for (i = 0; i < t->columns_count; i++) {
                if (0 == j || j < breaks_count[i]) {
                    switch (t->columns[i].type) {
                        case TABLE_TYPE_STRING:
                        {
                            size_t k;

                            putchar(' ');
                            fputs(breaks[i][j].part, stdout);
                            for (k = breaks[i][j].charlen; k < t->columns[i].len; k++) {
                                putchar(' ');
                            }
                            fputs(" |", stdout);
                            break;
                        }
                        case TABLE_TYPE_INT:
                            printf(" %*d |", (int) t->columns[i].len, (int) r->values[i].v);
                            break;
                        default:
                            assert(FALSE);
                            break;
                    }
                } else {
                    printf(" %*c |", (int) t->columns[i].len, ' ');
                }
            }
            putchar('\n');
        }
        for (i = 0; i < t->columns_count; i++) {
            if (TABLE_TYPE_STRING == t->columns[i].type) {
                if (breaks_count[i] > 1) {
                    for (j =  0; j < breaks_count[i]; j++) {
                        free((void *) breaks[i][j].part);
                    }
                }
                free(breaks[i]);
            }
        }
    }
    iterator_close(&it);
    // +--- ... ---+
    table_print_separator_line(t);
}

#define DIGIT_0 "\xF0\x9D\x9F\x8E" /* 1D7CE, Nd */
#define DIGIT_1 "\xF0\x9D\x9F\x8F"
#define DIGIT_2 "\xF0\x9D\x9F\x90"
#define DIGIT_3 "\xF0\x9D\x9F\x91"
#define DIGIT_4 "\xF0\x9D\x9F\x92"
#define DIGIT_5 "\xF0\x9D\x9F\x93"
#define DIGIT_6 "\xF0\x9D\x9F\x94"
#define DIGIT_7 "\xF0\x9D\x9F\x95"
#define DIGIT_8 "\xF0\x9D\x9F\x96"
#define DIGIT_9 "\xF0\x9D\x9F\x97"

#define UTF8
#ifdef UTF8
// # define STRING DIGIT_0 DIGIT_1 DIGIT_2 DIGIT_3 DIGIT_4 DIGIT_5 DIGIT_6 DIGIT_7 DIGIT_8 DIGIT_9
// # define STRING "\xEF\xAC\x80\xEF\xAC\x80\xEF\xAC\x80\xEF\xAC\x80\xEF\xAC\x80\xEF\xAC\x80\xEF\xAC\x80\xEF\xAC\x80\xEF\xAC\x80\xEF\xAC\x80"
#define STRING "éïàùçè"
#else
# define STRING "0123456789"
#endif
static const char long_string[] = \
    /* 1..10 */ STRING \
    /* 11..20 */ STRING \
    /* 21..30 */ STRING \
    /* 41..50 */ STRING \
    /* 51..60 */ STRING \
    /* 61..70 */ STRING \
    /* 71..80 */ STRING \
    /* 81..90 */ STRING \
    /* 91..100 */ STRING \
    /* 101..110 */ STRING \
    /* 111..120 */ STRING \
    /* 121..130 */ STRING \
    /* 131..140 */ STRING \
    /* 141..150 */ STRING \
    /* 151..160 */ STRING \
    /* 161..170 */ STRING \
    /* 171..180 */ STRING \
    /* 181..190 */ STRING \
    /* 191..200 */ STRING \
    /* 201..210 */ STRING \
    /* 211..220 */ STRING \
    /* 221..230 */ STRING \
    /* 231..240 */ STRING \
    /* 241..250 */ STRING \
    /* 251..260 */ STRING \
    /* 261..270 */ STRING \
    /* 271..280 */ STRING \
    /* 281..290 */ STRING \
    /* 291..300 */ STRING \
    /* 301..310 */ STRING \
    /* 311..320 */ STRING \
    /* 321..330 */ STRING;

INITIALIZER_DECL(table_test);
INITIALIZER_P(table_test)
{
    table_t *t;

    t = table_new(4, "id", TABLE_TYPE_INT, "subdomain", TABLE_TYPE_STRING, "target", TABLE_TYPE_STRING, "extra", TABLE_TYPE_STRING);
    table_store(t, 1, "abc", "def", "");
    table_store(t, 2, "ghi", "jkl", long_string);
    table_store(t, 3, "mno", long_string, "pqr");
    table_store(t, 4, "stu", long_string, long_string);
    table_display(t, TABLE_FLAG_NONE);
    table_destroy(t);
}
