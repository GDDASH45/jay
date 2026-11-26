/*
 * Jay 0.1.0 - interpreted programming language
 * SPDX-License-Identifier: MIT
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JAY_VERSION "0.1.0"
#define MAX_TOKENS 8192
#define MAX_VARS 512
#define MAX_TEXT 1024

typedef enum {
    T_EOF, T_EOL, T_IDENT, T_INT, T_FLOAT, T_STRING, T_CHAR,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_COMMA,
    T_SEMI, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_BANG, T_EQUAL, T_EQEQ, T_NE, T_LT, T_LE, T_GT, T_GE,
    T_AND, T_OR
} TokenType;

typedef struct {
    TokenType type;
    char text[MAX_TEXT];
    int line;
} Token;

typedef enum { V_INT, V_FLOAT, V_STR, V_BOOL, V_CHAR, V_VOID } ValueType;

typedef struct {
    ValueType type;
    union {
        long long i;
        double f;
        char *s;
        bool b;
        char c;
    } as;
} Value;

typedef struct {
    char name[128];
    Value value;
    ValueType declared;
    bool typed;
    bool is_volatile;
} Variable;

static Token tokens[MAX_TOKENS];
static size_t token_count, current;
static Variable variables[MAX_VARS];
static size_t variable_count;
static const char *source_name = "<input>";
static int errors;

static void fail_at(int line, const char *message)
{
    fprintf(stderr, "%s:%d: error: %s\n", source_name, line, message);
    errors++;
}

static char *copy_string(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) {
        fputs("jay: out of memory\n", stderr);
        exit(2);
    }
    memcpy(p, s, n);
    return p;
}

static void add_token(TokenType type, const char *start, size_t length, int line)
{
    if (token_count >= MAX_TOKENS) {
        fail_at(line, "too many tokens in source file");
        return;
    }
    Token *t = &tokens[token_count++];
    t->type = type;
    t->line = line;
    if (length >= sizeof(t->text)) length = sizeof(t->text) - 1;
    memcpy(t->text, start, length);
    t->text[length] = '\0';
}

static bool lex(const char *src)
{
    const char *p = src;
    int line = 1;
    token_count = 0;
    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\r') { p++; continue; }
        if (*p == '\n') { add_token(T_EOL, "\n", 1, line++); p++; continue; }
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p++;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            add_token(T_IDENT, start, (size_t)(p - start), line);
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            const char *start = p++;
            while (isdigit((unsigned char)*p)) p++;
            TokenType type = T_INT;
            if (*p == '.' && isdigit((unsigned char)p[1])) {
                type = T_FLOAT;
                p++;
                while (isdigit((unsigned char)*p)) p++;
            }
            add_token(type, start, (size_t)(p - start), line);
            continue;
        }
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            char buf[MAX_TEXT];
            size_t n = 0;
            bool closed = false;
            while (*p) {
                if (*p == quote) { p++; closed = true; break; }
                if (*p == '\n') break;
                char c = *p++;
                if (c == '\\' && *p) {
                    c = *p++;
                    if (c == 'n') c = '\n';
                    else if (c == 't') c = '\t';
                    else if (c == 'r') c = '\r';
                }
                if (n + 1 < sizeof(buf)) buf[n++] = c;
            }
            if (!closed) {
                fail_at(line, "unterminated string or character literal");
                return false;
            }
            buf[n] = '\0';
            add_token(quote == '"' ? T_STRING : T_CHAR, buf, n, line);
            continue;
        }
        int tokline = line;
        switch (*p) {
        case '(': add_token(T_LPAREN, p++, 1, tokline); break;
        case ')': add_token(T_RPAREN, p++, 1, tokline); break;
        case '{': add_token(T_LBRACE, p++, 1, tokline); break;
        case '}': add_token(T_RBRACE, p++, 1, tokline); break;
        case ',': add_token(T_COMMA, p++, 1, tokline); break;
        case ';': add_token(T_SEMI, p++, 1, tokline); break;
        case '+': add_token(T_PLUS, p++, 1, tokline); break;
        case '-': add_token(T_MINUS, p++, 1, tokline); break;
        case '*': add_token(T_STAR, p++, 1, tokline); break;
        case '/': add_token(T_SLASH, p++, 1, tokline); break;
        case '%': add_token(T_PERCENT, p++, 1, tokline); break;
        case '!':
            p++;
            if (*p == '=') { p++; add_token(T_NE, "!=", 2, tokline); }
            else add_token(T_BANG, "!", 1, tokline);
            break;
        case '=':
            p++;
            if (*p == '=') { p++; add_token(T_EQEQ, "==", 2, tokline); }
            else add_token(T_EQUAL, "=", 1, tokline);
            break;
        case '<':
            p++;
            if (*p == '=') { p++; add_token(T_LE, "<=", 2, tokline); }
            else add_token(T_LT, "<", 1, tokline);
            break;
        case '>':
            p++;
            if (*p == '=') { p++; add_token(T_GE, ">=", 2, tokline); }
            else add_token(T_GT, ">", 1, tokline);
            break;
        case '&':
            p++;
            if (*p == '&') { p++; add_token(T_AND, "&&", 2, tokline); }
            else { fail_at(tokline, "expected '&' after '&'"); return false; }
            break;
        case '|':
            p++;
            if (*p == '|') { p++; add_token(T_OR, "||", 2, tokline); }
            else { fail_at(tokline, "expected '|' after '|'"); return false; }
            break;
        default:
            fprintf(stderr, "%s:%d: error: unexpected character '%c'\n", source_name, line, *p);
            errors++;
            return false;
        }
    }
    add_token(T_EOF, "", 0, line);
    return errors == 0;
}

static Token *peek(void) { return &tokens[current]; }
static Token *previous(void) { return &tokens[current - 1]; }
static bool check(TokenType t) { return peek()->type == t; }
static Token *advance_token(void) { if (!check(T_EOF)) current++; return previous(); }
static bool match(TokenType t) { if (!check(t)) return false; advance_token(); return true; }

static bool word(const char *s)
{
    return check(T_IDENT) && strcmp(peek()->text, s) == 0;
}

static bool match_word(const char *s)
{
    if (!word(s)) return false;
    advance_token();
    return true;
}

static bool expect(TokenType t, const char *message)
{
    if (match(t)) return true;
    fail_at(peek()->line, message);
    return false;
}

static Value make_int(long long x) { Value v = {.type=V_INT}; v.as.i=x; return v; }
static Value make_float(double x) { Value v = {.type=V_FLOAT}; v.as.f=x; return v; }
static Value make_bool(bool x) { Value v = {.type=V_BOOL}; v.as.b=x; return v; }
static Value make_char(char x) { Value v = {.type=V_CHAR}; v.as.c=x; return v; }
static Value make_string(const char *x) { Value v = {.type=V_STR}; v.as.s=copy_string(x); return v; }
static Value make_void(void) { Value v = {.type=V_VOID}; return v; }

static void free_value(Value *v)
{
    if (v->type == V_STR) free(v->as.s);
    v->type = V_VOID;
}

static Value clone_value(Value v)
{
    if (v.type == V_STR) return make_string(v.as.s);
    return v;
}

static const char *type_name(ValueType t)
{
    switch (t) {
    case V_INT: return "int";
    case V_FLOAT: return "float";
    case V_STR: return "str";
    case V_BOOL: return "bool";
    case V_CHAR: return "char";
    default: return "void";
    }
}

static bool is_numeric(Value v) { return v.type == V_INT || v.type == V_FLOAT; }
static double number(Value v) { return v.type == V_INT ? (double)v.as.i : v.as.f; }

static void print_value(Value v)
{
    switch (v.type) {
    case V_INT: printf("%lld", v.as.i); break;
    case V_FLOAT: printf("%.15g", v.as.f); break;
    case V_STR: fputs(v.as.s, stdout); break;
    case V_BOOL: fputs(v.as.b ? "true" : "false", stdout); break;
    case V_CHAR: putchar(v.as.c); break;
    case V_VOID: fputs("void", stdout); break;
    }
}

static bool convert_value(Value in, ValueType target, Value *out)
{
    if (in.type == target) { *out = clone_value(in); return true; }
    if (target == V_FLOAT && in.type == V_INT) { *out = make_float((double)in.as.i); return true; }
    if (target == V_INT && in.type == V_FLOAT && floor(in.as.f) == in.as.f) {
        *out = make_int((long long)in.as.f); return true;
    }
    return false;
}

static Variable *find_variable(const char *name)
{
    for (size_t i = 0; i < variable_count; i++)
        if (strcmp(variables[i].name, name) == 0) return &variables[i];
    return NULL;
}

static bool define_variable(const char *name, Value value, bool typed,
                            ValueType declared, bool is_volatile, int line)
{
    if (find_variable(name)) {
        fail_at(line, "variable is already declared");
        return false;
    }
    if (variable_count >= MAX_VARS) {
        fail_at(line, "variable limit reached");
        return false;
    }
    Variable *v = &variables[variable_count++];
    snprintf(v->name, sizeof(v->name), "%s", name);
    v->value = clone_value(value);
    v->typed = typed;
    v->declared = declared;
    v->is_volatile = is_volatile;
    return true;
}

static bool assign_variable(Variable *var, Value value, int line)
{
    Value replacement;
    if (var->typed) {
        if (!convert_value(value, var->declared, &replacement)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "cannot assign %s to %s variable '%s'",
                     type_name(value.type), type_name(var->declared), var->name);
            fail_at(line, msg);
            return false;
        }
    } else {
        replacement = clone_value(value);
    }
    free_value(&var->value);
    var->value = replacement;
    return true;
}

static Value expression(void);

static Value primary(void)
{
    Token t = *peek();
    if (match(T_INT)) return make_int(strtoll(t.text, NULL, 10));
    if (match(T_FLOAT)) return make_float(strtod(t.text, NULL));
    if (match(T_STRING)) return make_string(t.text);
    if (match(T_CHAR)) {
        if (strlen(t.text) != 1) {
            fail_at(t.line, "char literal must contain exactly one character");
            return make_void();
        }
        return make_char(t.text[0]);
    }
    if (match_word("true")) return make_bool(true);
    if (match_word("false")) return make_bool(false);
    if (match(T_LPAREN)) {
        Value v = expression();
        expect(T_RPAREN, "expected ')' after expression");
        return v;
    }
    if (check(T_IDENT)) {
        advance_token();
        if (strcmp(t.text, "print") == 0) {
            if (!expect(T_LPAREN, "expected '(' after print")) return make_void();
            bool first = true;
            while (!check(T_RPAREN) && !check(T_EOF)) {
                if (!first && !match(T_COMMA)) {
                    fail_at(peek()->line, "expected ',' between print arguments");
                    break;
                }
                Value arg = expression();
                print_value(arg);
                free_value(&arg);
                first = false;
                if (check(T_RPAREN)) break;
            }
            expect(T_RPAREN, "expected ')' after print arguments");
            putchar('\n');
            return make_void();
        }
        Variable *var = find_variable(t.text);
        if (!var) {
            char msg[256];
            snprintf(msg, sizeof(msg), "undefined variable '%s'", t.text);
            fail_at(t.line, msg);
            return make_void();
        }
        return clone_value(var->value);
    }
    fail_at(t.line, "expected an expression");
    if (!check(T_EOF)) advance_token();
    return make_void();
}

static Value unary(void)
{
    if (match(T_MINUS)) {
        Value v = unary();
        if (v.type == V_INT) { v.as.i = -v.as.i; return v; }
        if (v.type == V_FLOAT) { v.as.f = -v.as.f; return v; }
        fail_at(previous()->line, "unary '-' requires a number");
        free_value(&v);
        return make_void();
    }
    if (match(T_BANG)) {
        Value v = unary();
        if (v.type != V_BOOL) {
            fail_at(previous()->line, "'!' requires a bool");
            free_value(&v);
            return make_void();
        }
        bool b = !v.as.b;
        free_value(&v);
        return make_bool(b);
    }
    return primary();
}

static Value factor(void)
{
    Value left = unary();
    while (check(T_STAR) || check(T_SLASH) || check(T_PERCENT)) {
        Token op = *advance_token();
        Value right = unary();
        if (!is_numeric(left) || !is_numeric(right)) {
            fail_at(op.line, "arithmetic operators require numeric operands");
            free_value(&left); free_value(&right); return make_void();
        }
        if (op.type == T_SLASH && number(right) == 0.0) {
            fail_at(op.line, "division by zero");
            free_value(&left); free_value(&right); return make_void();
        }
        if (op.type == T_PERCENT) {
            if (left.type != V_INT || right.type != V_INT || right.as.i == 0) {
                fail_at(op.line, "'%' requires integers and a nonzero divisor");
                free_value(&left); free_value(&right); return make_void();
            }
            left = make_int(left.as.i % right.as.i);
        } else if (left.type == V_INT && right.type == V_INT && op.type == T_STAR) {
            left = make_int(left.as.i * right.as.i);
        } else {
            double result = op.type == T_STAR ? number(left) * number(right) : number(left) / number(right);
            left = make_float(result);
        }
        free_value(&right);
    }
    return left;
}

static Value term(void)
{
    Value left = factor();
    while (check(T_PLUS) || check(T_MINUS)) {
        Token op = *advance_token();
        Value right = factor();
        if (op.type == T_PLUS && left.type == V_STR && right.type == V_STR) {
            size_t n = strlen(left.as.s) + strlen(right.as.s) + 1;
            char *buf = malloc(n);
            if (!buf) exit(2);
            snprintf(buf, n, "%s%s", left.as.s, right.as.s);
            Value joined = make_string(buf);
            free(buf); free_value(&left); free_value(&right); left = joined;
        } else if (is_numeric(left) && is_numeric(right)) {
            if (left.type == V_INT && right.type == V_INT) {
                long long n = op.type == T_PLUS ? left.as.i + right.as.i : left.as.i - right.as.i;
                left = make_int(n);
            } else {
                double n = op.type == T_PLUS ? number(left) + number(right) : number(left) - number(right);
                left = make_float(n);
            }
            free_value(&right);
        } else {
            fail_at(op.line, "operator requires two numbers (or two strings for '+')");
            free_value(&left); free_value(&right); return make_void();
        }
    }
    return left;
}

static Value comparison(void)
{
    Value left = term();
    while (check(T_LT) || check(T_LE) || check(T_GT) || check(T_GE)) {
        Token op = *advance_token();
        Value right = term();
        if (!is_numeric(left) || !is_numeric(right)) {
            fail_at(op.line, "comparison requires numeric operands");
            free_value(&left); free_value(&right); return make_void();
        }
        double a = number(left), b = number(right);
        bool result = op.type == T_LT ? a < b : op.type == T_LE ? a <= b :
                      op.type == T_GT ? a > b : a >= b;
        free_value(&left); free_value(&right); left = make_bool(result);
    }
    return left;
}

static Value equality(void)
{
    Value left = comparison();
    while (check(T_EQEQ) || check(T_NE)) {
        Token op = *advance_token();
        Value right = comparison();
        bool equal = false;
        if (is_numeric(left) && is_numeric(right)) equal = number(left) == number(right);
        else if (left.type == right.type) {
            switch (left.type) {
            case V_STR: equal = strcmp(left.as.s, right.as.s) == 0; break;
            case V_BOOL: equal = left.as.b == right.as.b; break;
            case V_CHAR: equal = left.as.c == right.as.c; break;
            default: equal = false; break;
            }
        }
        free_value(&left); free_value(&right);
        left = make_bool(op.type == T_EQEQ ? equal : !equal);
    }
    return left;
}

static Value logical_and(void)
{
    Value left = equality();
    while (match(T_AND)) {
        Token op = *previous();
        Value right = equality();
        if (left.type != V_BOOL || right.type != V_BOOL) {
            fail_at(op.line, "'&&' requires bool operands");
            free_value(&left); free_value(&right); return make_void();
        }
        bool b = left.as.b && right.as.b;
        free_value(&left); free_value(&right); left = make_bool(b);
    }
    return left;
}

static Value expression(void)
{
    Value left = logical_and();
    while (match(T_OR)) {
        Token op = *previous();
        Value right = logical_and();
        if (left.type != V_BOOL || right.type != V_BOOL) {
            fail_at(op.line, "'||' requires bool operands");
            free_value(&left); free_value(&right); return make_void();
        }
        bool b = left.as.b || right.as.b;
        free_value(&left); free_value(&right); left = make_bool(b);
    }
    return left;
}

static bool parse_type(ValueType *type)
{
    if (match_word("int")) *type = V_INT;
    else if (match_word("float")) *type = V_FLOAT;
    else if (match_word("str")) *type = V_STR;
    else if (match_word("bool")) *type = V_BOOL;
    else if (match_word("char")) *type = V_CHAR;
    else return false;
    return true;
}

static void skip_separators(void)
{
    while (match(T_EOL) || match(T_SEMI)) {}
}

static void statement(void)
{
    skip_separators();
    if (check(T_EOF) || check(T_RBRACE)) return;

    int line = peek()->line;
    bool is_volatile = match_word("volatile");
    ValueType declared;
    bool typed = parse_type(&declared);

    if (typed || match_word("let")) {
        bool inferred = !typed;
        if (!expect(T_IDENT, "expected variable name after declaration")) return;
        Token name = *previous();
        if (!expect(T_EQUAL, "expected '=' in variable declaration")) return;
        Value value = expression();
        Value stored = value;
        bool converted = false;
        if (typed && !convert_value(value, declared, &stored)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "cannot initialize %s variable with %s",
                     type_name(declared), type_name(value.type));
            fail_at(line, msg);
        } else {
            converted = typed;
            if (!define_variable(name.text, stored, !inferred, declared, is_volatile, line)) {
                /* Error already reported. */
            }
        }
        free_value(&value);
        if (converted) free_value(&stored);
        return;
    }

    if (is_volatile) {
        fail_at(line, "'volatile' must precede a typed variable declaration or let");
        return;
    }

    if (word("print") && tokens[current + 1].type == T_LPAREN) {
        Value result = expression();
        free_value(&result);
        return;
    }

    if (check(T_IDENT)) {
        Token name = *advance_token();
        if (match(T_EQUAL)) {
            Value value = expression();
            Variable *var = find_variable(name.text);
            if (!var) {
                char msg[256];
                snprintf(msg, sizeof(msg), "undefined variable '%s'", name.text);
                fail_at(line, msg);
            } else {
                assign_variable(var, value, line);
            }
            free_value(&value);
            return;
        }
        fail_at(line, "unknown statement; expected declaration or assignment");
        return;
    }

    fail_at(line, "unexpected token at start of statement");
    advance_token();
}

static bool run_source(const char *src, const char *name)
{
    source_name = name;
    errors = 0;
    current = 0;
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

static int run_file(const char *path)
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

static void repl(void)
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

static void usage(const char *argv0)
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
