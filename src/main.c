/*
 * Jay 0.9.0 - interpreted programming language
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "jay_internal.h"

Token tokens[MAX_TOKENS];
size_t token_count, current;

Variable variables[MAX_VARS];
size_t variable_count;

Value return_value = {.type = V_VOID};
size_t scope_depth;
size_t scope_starts[MAX_SCOPE_DEPTH];

const char *source_name = "<input>";

int errors;
int loop_depth;
int flow_state;

char active_module[128];
char module_stack[MAX_SCOPE_DEPTH][128];
size_t module_stack_depth;

static void clear_variables(void)
{
    for (size_t i = 0; i < variable_count; i++)
        free_value(&variables[i].value);

    variable_count = 0;
}

static void clear_functions(void)
{
    memset(functions, 0, sizeof(functions));
    function_count = 0;
}

void reset_interpreter(void)
{
    clear_variables();
    clear_functions();

    free_value(&return_value);
    return_value = make_void();

    scope_depth = 0;
    scope_starts[0] = 0;

    loop_depth = 0;
    flow_state = 0;
    active_module[0] = '\0';
    module_stack_depth = 0;
}

bool run_source(const char *src, const char *name)
{
    source_name = name;
    errors = 0;
    current = 0;
    loop_depth = 0;
    flow_state = 0;
    active_module[0] = '\0';
    module_stack_depth = 0;

    char *expanded = expand_imports(src, name);
    if (!expanded) {
        errors++;
        return false;
    }

    bool lexed = lex(expanded);
    free(expanded);
    if (!lexed)
        return false;

    while (!check(T_EOF)) {
        size_t before = current;

        statement();
        skip_separators();

        if (current == before && !check(T_EOF))
            advance_token();
    }

    return errors == 0;
}

static char *read_stream(FILE *fp)
{
    size_t cap = 4096;
    size_t len = 0;

    char *buf = malloc(cap);
    if (!buf)
        return NULL;

    for (;;) {
        if (len + 2048 + 1 > cap) {
            cap *= 2;

            char *next = realloc(buf, cap);
            if (!next) {
                free(buf);
                return NULL;
            }

            buf = next;
        }

        size_t n = fread(buf + len, 1, cap - len - 1, fp);
        len += n;

        if (n == 0) {
            if (ferror(fp)) {
                free(buf);
                return NULL;
            }

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
        fprintf(stderr, "jay: cannot open '%s': %s\n",
                path, strerror(errno));
        return 1;
    }

    char *src = read_stream(fp);
    fclose(fp);

    if (!src) {
        fputs("jay: could not read source file\n", stderr);
        return 1;
    }

    reset_interpreter();

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

        if (n < 0)
            break;

        if (n == 1 && line[0] == '\n')
            continue;

        run_source(line, "<repl>");
    }

    free(line);
}

void usage(const char *argv0)
{
    printf("Jay %s - interpreted programming language\n", JAY_VERSION);
    printf("Usage: %s [--version] [--help] [file.jay|file.jbc]\n", argv0);
    printf("       %s --compile source.jay -o output.jbc\n", argv0);
    printf("       %s --run-bytecode file.jbc\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc == 5 && strcmp(argv[1], "--compile") == 0 &&
        strcmp(argv[3], "-o") == 0)
        return compile_file(argv[2], argv[4]);

    if (argc == 3 && strcmp(argv[1], "--run-bytecode") == 0)
        return run_bytecode(argv[2]);

    if (argc > 2) {
        usage(argv[0]);
        return 2;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "--version") == 0 ||
            strcmp(argv[1], "-v") == 0) {
            puts("Jay " JAY_VERSION);
            return 0;
        }

        if (strcmp(argv[1], "--help") == 0 ||
            strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }

        size_t n = strlen(argv[1]);
        if (n >= 4 && strcmp(argv[1] + n - 4, ".jbc") == 0)
            return run_bytecode(argv[1]);

        return run_file(argv[1]);
    }

    repl();
    reset_interpreter();
    return 0;
}
