#include "json_min.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ws(const char *p)
{
    while (p != NULL && *p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }

    return p;
}

static const char *find_key_value_start(const char *json, const char *key)
{
    if (json == NULL || key == NULL) {
        return NULL;
    }

    size_t key_len = strlen(key);
    const char *p = json;

    while ((p = strchr(p, '"')) != NULL) {
        p++;

        if (strncmp(p, key, key_len) == 0 && p[key_len] == '"') {
            p += key_len + 1;
            p = skip_ws(p);

            if (*p == ':') {
                p++;
                return skip_ws(p);
            }
        }

        /* Move past the current string safely. */
        while (*p != '\0') {
            if (*p == '\\' && p[1] != '\0') {
                p += 2;
                continue;
            }

            if (*p == '"') {
                p++;
                break;
            }

            p++;
        }
    }

    return NULL;
}

bool json_min_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return false;
    }

    out[0] = '\0';

    const char *p = find_key_value_start(json, key);

    if (p == NULL || *p != '"') {
        return false;
    }

    p++;

    size_t written = 0;

    while (*p != '\0' && *p != '"') {
        char ch = *p;

        if (ch == '\\' && p[1] != '\0') {
            p++;

            switch (*p) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                default: ch = *p; break;
            }
        }

        if (written < out_size - 1) {
            out[written++] = ch;
        } else {
            out[0] = '\0';
            return false;
        }

        p++;
    }

    if (*p != '"') {
        out[0] = '\0';
        return false;
    }

    out[written] = '\0';
    return true;
}

bool json_min_get_int(const char *json, const char *key, int *out)
{
    if (out == NULL) {
        return false;
    }

    const char *p = find_key_value_start(json, key);

    if (p == NULL) {
        return false;
    }

    char *end = NULL;
    long value = strtol(p, &end, 10);

    if (end == p) {
        return false;
    }

    *out = (int)value;
    return true;
}
