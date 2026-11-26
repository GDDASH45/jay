/*
 * Jay 0.2.0 - interpreted programming language
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "jay_internal.h"

void fail_at(int line, const char *message)
{
    fprintf(stderr, "%s:%d: error: %s\n", source_name, line, message);
    errors++;
}

void add_token(TokenType type, const char *start, size_t length, int line)
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

bool lex(const char *src)
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
