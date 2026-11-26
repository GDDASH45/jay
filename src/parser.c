/*
 * Jay 0.2.0 - interpreted programming language
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "jay_internal.h"

Token *peek(void) { return &tokens[current]; }

Token *previous(void) { return &tokens[current - 1]; }

bool check(TokenType t) { return peek()->type == t; }

Token *advance_token(void) { if (!check(T_EOF)) current++; return previous(); }

bool match(TokenType t) { if (!check(t)) return false; advance_token(); return true; }

bool word(const char *s)
{
    return check(T_IDENT) && strcmp(peek()->text, s) == 0;
}

bool match_word(const char *s)
{
    if (!word(s)) return false;
    advance_token();
    return true;
}

bool expect(TokenType t, const char *message)
{
    if (match(t)) return true;
    fail_at(peek()->line, message);
    return false;
}

Value primary(void)
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

Value unary(void)
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

Value factor(void)
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

Value term(void)
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

Value comparison(void)
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

Value equality(void)
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

Value logical_and(void)
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

Value expression(void)
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

bool parse_type(ValueType *type)
{
    if (match_word("int")) *type = V_INT;
    else if (match_word("float")) *type = V_FLOAT;
    else if (match_word("str")) *type = V_STR;
    else if (match_word("bool")) *type = V_BOOL;
    else if (match_word("char")) *type = V_CHAR;
    else return false;
    return true;
}

void skip_separators(void)
{
    while (match(T_EOL) || match(T_SEMI)) {}
}



size_t matching_brace(size_t open)
{
    if (open >= token_count || tokens[open].type != T_LBRACE)
        return (size_t)-1;

    size_t depth = 0;
    for (size_t i = open; i < token_count; i++) {
        if (tokens[i].type == T_LBRACE)
            depth++;
        else if (tokens[i].type == T_RBRACE) {
            if (--depth == 0)
                return i;
        }
    }
    return (size_t)-1;
}

void execute_range(size_t start, size_t end)
{
    current = start;
    while (current < end && flow_state == 0 && !check(T_EOF)) {
        size_t before = current;
        statement();
        skip_separators();
        if (current == before && current < end)
            current++;
    }
}

void statement(void)
{
    skip_separators();
    if (check(T_EOF) || check(T_RBRACE)) return;

    int line = peek()->line;

    if (match_word("break")) {
        if (loop_depth == 0)
            fail_at(line, "'break' is only valid inside a loop");
        else
            flow_state = 1;
        return;
    }

    if (match_word("continue")) {
        if (loop_depth == 0)
            fail_at(line, "'continue' is only valid inside a loop");
        else
            flow_state = 2;
        return;
    }

    if (match_word("if")) {
        if (!expect(T_LPAREN, "expected '(' after if"))
            return;
        Value condition = expression();
        bool take_then = condition.type == V_BOOL && condition.as.b;
        if (condition.type != V_BOOL)
            fail_at(line, "if condition must be bool");
        free_value(&condition);
        if (!expect(T_RPAREN, "expected ')' after if condition"))
            return;
        skip_separators();
        if (!expect(T_LBRACE, "expected '{' after if condition"))
            return;

        size_t then_start = current;
        size_t then_close = matching_brace(current - 1);
        if (then_close == (size_t)-1) {
            fail_at(line, "unterminated if block");
            return;
        }
        size_t after_then = then_close + 1;
        size_t else_start = 0, else_close = 0;
        bool has_else = false;

        current = after_then;
        skip_separators();
        if (match_word("else")) {
            skip_separators();
            if (!expect(T_LBRACE, "expected '{' after else"))
                return;
            else_start = current;
            else_close = matching_brace(current - 1);
            if (else_close == (size_t)-1) {
                fail_at(line, "unterminated else block");
                return;
            }
            has_else = true;
            current = else_close + 1;
        }

        size_t after_statement = current;
        if (take_then)
            execute_range(then_start, then_close);
        else if (has_else)
            execute_range(else_start, else_close);
        current = after_statement;
        return;
    }

    if (match_word("while")) {
        if (!expect(T_LPAREN, "expected '(' after while"))
            return;
        size_t condition_start = current;
        Value first_condition = expression();
        size_t condition_end = current;
        bool valid_condition = first_condition.type == V_BOOL;
        if (!valid_condition)
            fail_at(line, "while condition must be bool");
        free_value(&first_condition);
        if (!expect(T_RPAREN, "expected ')' after while condition"))
            return;
        skip_separators();
        if (!expect(T_LBRACE, "expected '{' after while condition"))
            return;

        size_t body_start = current;
        size_t body_close = matching_brace(current - 1);
        if (body_close == (size_t)-1) {
            fail_at(line, "unterminated while block");
            return;
        }
        size_t after_loop = body_close + 1;

        if (valid_condition) {
            loop_depth++;
            for (;;) {
                current = condition_start;
                Value condition = expression();
                current = condition_end;
                bool keep_going = condition.type == V_BOOL && condition.as.b;
                if (condition.type != V_BOOL)
                    fail_at(line, "while condition must evaluate to bool");
                free_value(&condition);
                if (!keep_going || errors)
                    break;

                flow_state = 0;
                execute_range(body_start, body_close);
                if (flow_state == 1) {
                    flow_state = 0;
                    break;
                }
                if (flow_state == 2)
                    flow_state = 0;
                if (errors)
                    break;
            }
            loop_depth--;
        }
        flow_state = 0;
        current = after_loop;
        return;
    }

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

    if (word("print") && current + 1 < token_count && tokens[current + 1].type == T_LPAREN) {
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
