/*
 * Jay module loader
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "jay_internal.h"
#include <limits.h>
#include <stdint.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_IMPORTED_MODULES 256
#define MAX_IMPORT_DEPTH 64

typedef struct {
    char name[128];
    bool active;
    bool loaded;
} ImportedModule;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} SourceBuffer;

static ImportedModule modules[MAX_IMPORTED_MODULES];
static size_t module_count;

static bool append_text(SourceBuffer *buffer, const char *text, size_t length)
{
    if (length > SIZE_MAX - buffer->length - 1)
        return false;

    size_t needed = buffer->length + length + 1;
    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 4096;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2)
                return false;
            capacity *= 2;
        }

        char *next = realloc(buffer->data, capacity);
        if (!next)
            return false;

        buffer->data = next;
        buffer->capacity = capacity;
    }

    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return true;
}

static char *read_module(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    size_t capacity = 4096;
    size_t length = 0;
    char *data = malloc(capacity);
    if (!data) {
        fclose(fp);
        return NULL;
    }

    for (;;) {
        if (length + 2048 + 1 > capacity) {
            if (capacity > SIZE_MAX / 2) {
                free(data);
                fclose(fp);
                return NULL;
            }

            capacity *= 2;
            char *next = realloc(data, capacity);
            if (!next) {
                free(data);
                fclose(fp);
                return NULL;
            }
            data = next;
        }

        size_t count = fread(data + length, 1, capacity - length - 1, fp);
        length += count;

        if (count == 0) {
            if (ferror(fp)) {
                free(data);
                fclose(fp);
                return NULL;
            }
            break;
        }
    }

    fclose(fp);
    data[length] = '\0';
    return data;
}

static ImportedModule *module_state(const char *name)
{
    for (size_t i = 0; i < module_count; i++) {
        if (strcmp(modules[i].name, name) == 0)
            return &modules[i];
    }

    if (module_count >= MAX_IMPORTED_MODULES)
        return NULL;

    ImportedModule *module = &modules[module_count++];
    snprintf(module->name, sizeof(module->name), "%s", name);
    module->active = false;
    module->loaded = false;
    return module;
}

static bool valid_module_name(const char *name, size_t length)
{
    if (!length || length >= 128)
        return false;

    if (!(isalpha((unsigned char)name[0]) || name[0] == '_'))
        return false;

    for (size_t i = 1; i < length; i++) {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_'))
            return false;
    }

    return true;
}

static char *expand_source(const char *source, const char *origin,
                           unsigned depth, SourceBuffer *output);

static bool expand_module(const char *name, const char *origin,
                          unsigned depth, SourceBuffer *output)
{
    ImportedModule *state = module_state(name);
    if (!state) {
        fprintf(stderr, "%s: error: too many imported modules\n", origin);
        return false;
    }

    if (state->active) {
        fprintf(stderr, "%s: error: circular import of module '%s'\n",
                origin, name);
        return false;
    }

    if (state->loaded)
        return true;

    if (depth >= MAX_IMPORT_DEPTH) {
        fprintf(stderr, "%s: error: maximum module import depth exceeded\n",
                origin);
        return false;
    }

    const char *directory = getenv("JAY_MODULE_DIR");
    if (!directory || !*directory)
        directory = "/lib/jay/modules";

    char path[PATH_MAX];
    int length = snprintf(path, sizeof(path), "%s/%s.jmd", directory, name);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        fprintf(stderr, "%s: error: module path is too long\n", origin);
        return false;
    }

    char *module_source = read_module(path);
    if (!module_source) {
        fprintf(stderr, "%s: error: cannot load module '%s' from '%s': %s\n",
                origin, name, path, strerror(errno));
        return false;
    }

    char marker[160];
    int marker_length = snprintf(marker, sizeof(marker),
                                 "__jay_module_begin %s\n", name);
    if (marker_length < 0 || (size_t)marker_length >= sizeof(marker) ||
        !append_text(output, marker, (size_t)marker_length)) {
        free(module_source);
        return false;
    }

    state->active = true;
    char *expanded = expand_source(module_source, path, depth + 1, output);
    state->active = false;
    free(module_source);

    if (!expanded)
        return false;

    free(expanded);
    if (!append_text(output, "\n", 1) ||
        !append_text(output, "__jay_module_end\n", 17))
        return false;
    state->loaded = true;
    return true;
}

/*
 * Imports occupy a complete line:
 *     import module_name
 *     import module_name;
 *
 * The module text is expanded in place, preserving source order.
 */
static char *expand_source(const char *source, const char *origin,
                           unsigned depth, SourceBuffer *output)
{
    const char *line = source;

    while (*line) {
        const char *end = strchr(line, '\n');
        size_t line_length = end ? (size_t)(end - line) : strlen(line);
        const char *p = line;
        const char *limit = line + line_length;

        while (p < limit && (*p == ' ' || *p == '\t' || *p == '\r'))
            p++;

        bool is_import = (size_t)(limit - p) >= 6 &&
                         memcmp(p, "import", 6) == 0 &&
                         (p + 6 == limit || isspace((unsigned char)p[6]));

        bool is_umport = (size_t)(limit - p) >= 6 &&
                         memcmp(p, "umport", 6) == 0 &&
                         (p + 6 == limit || isspace((unsigned char)p[6]));

        if (is_umport) {
            p += 6;
            while (p < limit && (*p == ' ' || *p == '\t')) p++;
            const char *name_start = p;
            while (p < limit && (isalnum((unsigned char)*p) || *p == '_')) p++;
            size_t name_length = (size_t)(p - name_start);
            bool valid = valid_module_name(name_start, name_length);
            while (p < limit && isspace((unsigned char)*p)) p++;
            if (p < limit && *p == ';') {
                p++;
                while (p < limit && isspace((unsigned char)*p)) p++;
            }
            if (p < limit && *p != '#') valid = false;
            if (!valid) {
                fprintf(stderr, "%s: error: invalid umport statement\n", origin);
                return NULL;
            }
            char name[128];
            memcpy(name, name_start, name_length);
            name[name_length] = '\0';
            ImportedModule *state = module_state(name);
            if (!state) {
                fprintf(stderr, "%s: error: too many imported modules\n", origin);
                return NULL;
            }
            state->loaded = false;
            if (!append_text(output, line, line_length)) return NULL;
            if (end && !append_text(output, "\n", 1)) return NULL;
        } else if (is_import) {
            p += 6;
            while (p < limit && (*p == ' ' || *p == '\t'))
                p++;

            const char *name_start = p;
            while (p < limit &&
                   (isalnum((unsigned char)*p) || *p == '_'))
                p++;

            size_t name_length = (size_t)(p - name_start);
            bool valid = valid_module_name(name_start, name_length);

            while (p < limit && (*p == ' ' || *p == '\t' || *p == '\r'))
                p++;
            if (p < limit && *p == ';') {
                p++;
                while (p < limit &&
                       (*p == ' ' || *p == '\t' || *p == '\r'))
                    p++;
            }

            if (p < limit && *p != '#')
                valid = false;

            if (!valid) {
                fprintf(stderr, "%s: error: invalid import statement\n", origin);
                return NULL;
            }

            char name[128];
            memcpy(name, name_start, name_length);
            name[name_length] = '\0';

            if (!expand_module(name, origin, depth, output))
                return NULL;
        } else {
            if (!append_text(output, line, line_length))
                return NULL;
            if (end && !append_text(output, "\n", 1))
                return NULL;
        }

        if (!end)
            break;
        line = end + 1;
    }

    return copy_string("");
}

char *expand_imports(const char *source, const char *origin)
{
    SourceBuffer output = {0};
    module_count = 0;
    memset(modules, 0, sizeof(modules));

    if (!expand_source(source, origin, 0, &output)) {
        free(output.data);
        return NULL;
    }

    if (!output.data)
        return copy_string("");

    return output.data;
}
