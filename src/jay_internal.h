/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef JAY_INTERNAL_H
#define JAY_INTERNAL_H
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define JAY_VERSION "0.5.0"
#define MAX_TOKENS 8192
#define MAX_VARS 2048
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

#define MAX_FUNCTIONS 4096
#define MAX_PARAMETERS 128
#define MAX_SCOPE_DEPTH 128

typedef struct {
    char name[128];
    ValueType type;
    bool typed;
} Parameter;

typedef struct {
    char name[128];
    Parameter parameters[MAX_PARAMETERS];
    size_t parameter_count;
    size_t body_start;
    size_t body_end;
    int line;
} Function;

extern Function functions[MAX_FUNCTIONS];
extern size_t function_count;

extern Value return_value;
extern size_t scope_depth;
extern size_t scope_starts[MAX_SCOPE_DEPTH];

bool define_function(const char *name, Parameter *parameters,
                     size_t parameter_count, size_t body_start,
                     size_t body_end, int line);
Function *find_function(const char *name);
Value call_function(const char *name, Value *arguments,
                    size_t argument_count, int line);
bool jaystdlib_call(const char *name, Value *arguments,
                    size_t argument_count, Value *result, int line);

extern Token tokens[MAX_TOKENS];
extern size_t token_count, current;
extern Variable variables[MAX_VARS];
extern size_t variable_count;
extern const char *source_name;
extern int errors;
extern int loop_depth;
extern int flow_state;

void fail_at(int line, const char *message);
char *copy_string(const char *s);
void add_token(TokenType type, const char *start, size_t length, int line);
bool lex(const char *src);
Token *peek(void);
Token *previous(void);
bool check(TokenType t);
Token *advance_token(void);
bool match(TokenType t);
bool word(const char *s);
bool match_word(const char *s);
bool expect(TokenType t, const char *message);
Value make_int(long long x);
Value make_float(double x);
Value make_bool(bool x);
Value make_char(char x);
Value make_string(const char *x);
Value make_void(void);
void free_value(Value *v);
Value clone_value(Value v);
const char *type_name(ValueType t);
bool is_numeric(Value v);
double number(Value v);
void print_value(Value v);
bool convert_value(Value in, ValueType target, Value *out);
Variable *find_variable(const char *name);
bool define_variable(const char *name, Value value, bool typed, ValueType declared, bool is_volatile, int line);
bool assign_variable(Variable *var, Value value, int line);
Value primary(void);
Value unary(void);
Value factor(void);
Value term(void);
Value comparison(void);
Value equality(void);
Value logical_and(void);
Value expression(void);
bool parse_type(ValueType *type);
void skip_separators(void);
size_t matching_brace(size_t open);
void execute_range(size_t start, size_t end);
void statement(void);
char *expand_imports(const char *source, const char *origin);
bool run_source(const char *src, const char *name);
int run_file(const char *path);
void repl(void);
void usage(const char *argv0);
int main(int argc, char **argv);

#endif
