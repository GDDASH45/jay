/*
 * Jay 0.9.0 - interpreted programming language
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "jay_internal.h"

char *copy_string(const char *s)
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

Value make_int(long long x) { Value v = {.type=V_INT}; v.as.i=x; return v; }

Value make_float(double x) { Value v = {.type=V_FLOAT}; v.as.f=x; return v; }

Value make_bool(bool x) { Value v = {.type=V_BOOL}; v.as.b=x; return v; }

Value make_char(char x) { Value v = {.type=V_CHAR}; v.as.c=x; return v; }

Value make_string(const char *x) { Value v = {.type=V_STR}; v.as.s=copy_string(x); return v; }

Value make_void(void) { Value v = {.type=V_VOID}; return v; }

void free_value(Value *v)
{
    if (v->type == V_STR) free(v->as.s);
    v->type = V_VOID;
}

Value clone_value(Value v)
{
    if (v.type == V_STR) return make_string(v.as.s);
    return v;
}

const char *type_name(ValueType t)
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

bool is_numeric(Value v) { return v.type == V_INT || v.type == V_FLOAT; }

double number(Value v) { return v.type == V_INT ? (double)v.as.i : v.as.f; }

void print_value(Value v)
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

bool convert_value(Value in, ValueType target, Value *out)
{
    if (in.type == target) { *out = clone_value(in); return true; }
    if (target == V_FLOAT && in.type == V_INT) { *out = make_float((double)in.as.i); return true; }
    if (target == V_INT && in.type == V_FLOAT && floor(in.as.f) == in.as.f) {
        *out = make_int((long long)in.as.f); return true;
    }
    return false;
}

Variable *find_variable(const char *name)
{
    for (size_t i = 0; i < variable_count; i++)
        if (strcmp(variables[i].name, name) == 0) return &variables[i];
    return NULL;
}

bool define_variable(const char *name, Value value, bool typed,
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
    snprintf(v->module_name, sizeof(v->module_name), "%s", active_module);
    return true;
}

bool assign_variable(Variable *var, Value value, int line)
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


