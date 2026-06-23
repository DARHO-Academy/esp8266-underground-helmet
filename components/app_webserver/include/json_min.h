#ifndef JSON_MIN_H
#define JSON_MIN_H

#include <stdbool.h>
#include <stddef.h>

bool json_min_get_string(const char *json, const char *key, char *out, size_t out_size);
bool json_min_get_int(const char *json, const char *key, int *out);

#endif
