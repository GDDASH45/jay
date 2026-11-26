/*
 * Jay 0.2.0 - interpreted programming language
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "jay_internal.h"

Token tokens[MAX_TOKENS];
size_t token_count, current;
Variable variables[MAX_VARS];
size_t variable_count;
const char *source_name = "<input>";
int errors;
int loop_depth;
int flow_state;

bool run_source(const char *src, const char *name)
{
    source_name = name;
    errors = 0;
    current = 0;
    loop_depth = 0;
    flow_state = 0;
    if (!lex(src)) return false;
    while (!check(T_EOF)) {
        size_t before = current;
        statement();
        skip_separators();
        if (current == before && !check(T_EOF)) advance_token();
    }
    return errors == 0;
}

static char *read_stream(FILE *fp)
{
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 2048 + 1 > cap) {
            cap *= 2;
            char *next = realloc(buf, cap);
            if (!next) { free(buf); return NULL; }
            buf = next;
        }
        size_t n = fread(buf + len, 1, cap - len - 1, fp);
        len += n;
        if (n == 0) {
            if (ferror(fp)) { free(buf); return NULL; }
            break;
        }
    }
    buf[len] = '\0';
    return buf;
}

int run_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "jay: cannot open '%s': %s\n", path, strerror(errno));
        return 1;
    }
    char *src = read_stream(fp);
    fclose(fp);
    if (!src) { fputs("jay: could not read source file\n", stderr); return 1; }
    bool ok = run_source(src, path);
    free(src);
    return ok ? 0 : 1;
}

void repl(void)
{
    char *line = NULL;
    size_t cap = 0;
    puts("Jay Interactive Shell " JAY_VERSION);
    puts("Type expressions as statements; use Ctrl-D to exit.");
    while (true) {
        fputs("jay> ", stdout);
        fflush(stdout);
        ssize_t n = getline(&line, &cap, stdin);
        if (n < 0) break;
        if (n == 1 && line[0] == '\n') continue;
        run_source(line, "<repl>");
    }
    free(line);
}

void usage(const char *argv0)
{
    printf("Jay %s - interpreted programming language\n", JAY_VERSION);
    printf("Usage: %s [--version] [--help] [file.jay]\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc > 2) { usage(argv[0]); return 2; }
    if (argc == 2) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            puts("Jay " JAY_VERSION); return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage(argv[0]); return 0;
        }
        return run_file(argv[1]);
    }
    repl();
    for (size_t i = 0; i < variable_count; i++) free_value(&variables[i].value);
    return 0;
}
