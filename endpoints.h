#ifndef ENDPOINTS_H

# define ENDPOINTS_H

typedef struct {
    const char *name;
    const char *base;
    const module_t * const *managed;
} endpoint_t;

extern const endpoint_t endpoints[];
extern const char *endpoint_names[];

#endif /* !ENDPOINTS_H */