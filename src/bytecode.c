/* SPDX-License-Identifier: GPL-2.0-only */
#include "jay_internal.h"
#include <stdint.h>

static const unsigned char magic[8] = { 'J', 'A', 'Y', 'B', 'C', 0, 1, 0 };

static bool write_u32(FILE *fp, uint32_t value)
{
    unsigned char b[4] = {
        (unsigned char)value,
        (unsigned char)(value >> 8),
        (unsigned char)(value >> 16),
        (unsigned char)(value >> 24)
    };
    return fwrite(b, 1, sizeof(b), fp) == sizeof(b);
}

static bool read_u32(FILE *fp, uint32_t *value)
{
    unsigned char b[4];
    if (fread(b, 1, sizeof(b), fp) != sizeof(b))
        return false;
    *value = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
             ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
}

bool bytecode_write(const char *path, const Token *items, size_t count)
{
    if (count == 0 || count > MAX_TOKENS || items[count - 1].type != T_EOF)
        return false;

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "jay: cannot create bytecode '%s': %s\n", path, strerror(errno));
        return false;
    }

    bool ok = fwrite(magic, 1, sizeof(magic), fp) == sizeof(magic) &&
              write_u32(fp, 1) && write_u32(fp, (uint32_t)count);

    for (size_t i = 0; ok && i < count; i++) {
        size_t n = strlen(items[i].text);
        unsigned char type = (unsigned char)items[i].type;
        unsigned char len[2] = { (unsigned char)n, (unsigned char)(n >> 8) };
        ok = n < MAX_TEXT &&
             fwrite(&type, 1, 1, fp) == 1 &&
             write_u32(fp, (uint32_t)items[i].line) &&
             fwrite(len, 1, 2, fp) == 2 &&
             fwrite(items[i].text, 1, n, fp) == n;
    }

    if (fclose(fp) != 0)
        ok = false;
    if (!ok) {
        fprintf(stderr, "jay: failed writing bytecode '%s'\n", path);
        remove(path);
    }
    return ok;
}

bool bytecode_read(const char *path, Token *items, size_t capacity, size_t *count)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "jay: cannot open bytecode '%s': %s\n", path, strerror(errno));
        return false;
    }

    unsigned char header[8];
    uint32_t version = 0, total = 0;
    bool ok = fread(header, 1, sizeof(header), fp) == sizeof(header) &&
              memcmp(header, magic, sizeof(magic)) == 0 &&
              read_u32(fp, &version) && read_u32(fp, &total) &&
              version == 1 && total > 0 && total <= capacity && total <= MAX_TOKENS;

    for (uint32_t i = 0; ok && i < total; i++) {
        unsigned char type = 0, len[2] = {0, 0};
        uint32_t line = 0;
        ok = fread(&type, 1, 1, fp) == 1 &&
             read_u32(fp, &line) &&
             fread(len, 1, 2, fp) == 2;
        size_t n = (size_t)len[0] | ((size_t)len[1] << 8);
        if (!ok || type > T_COLON || n >= MAX_TEXT || line > INT32_MAX ||
            (type == T_EOF && i + 1 != total) ||
            (type != T_EOF && i + 1 == total)) {
            ok = false;
            break;
        }
        items[i].type = (TokenType)type;
        items[i].line = (int)line;
        ok = fread(items[i].text, 1, n, fp) == n;
        items[i].text[n] = '\0';
    }

    if (ok) {
        int extra = fgetc(fp);
        ok = extra == EOF && items[total - 1].type == T_EOF;
    }
    fclose(fp);

    if (!ok) {
        fprintf(stderr, "jay: invalid or unsupported bytecode file '%s'\n", path);
        return false;
    }
    *count = total;
    return true;
}

static char *read_source_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "jay: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) { fclose(fp); return NULL; }
    for (;;) {
        if (len + 2048 + 1 > cap) {
            cap *= 2;
            char *next = realloc(buf, cap);
            if (!next) { free(buf); fclose(fp); return NULL; }
            buf = next;
        }
        size_t n = fread(buf + len, 1, cap - len - 1, fp);
        len += n;
        if (n == 0) {
            if (ferror(fp)) { free(buf); fclose(fp); return NULL; }
            break;
        }
    }
    fclose(fp);
    buf[len] = '\0';
    return buf;
}

int compile_file(const char *source_path, const char *bytecode_path)
{
    char *source = read_source_file(source_path);
    if (!source) return 1;

    source_name = source_path;
    errors = 0;
    char *expanded = expand_imports(source, source_path);
    free(source);
    if (!expanded) return 1;

    bool ok = lex(expanded);
    free(expanded);
    if (ok)
        ok = bytecode_write(bytecode_path, tokens, token_count);
    if (ok)
        printf("Compiled %s -> %s (%zu tokens)\n", source_path, bytecode_path, token_count);
    return ok ? 0 : 1;
}

int run_bytecode(const char *path)
{
    reset_interpreter();
    source_name = path;
    errors = 0;
    current = 0;
    if (!bytecode_read(path, tokens, MAX_TOKENS, &token_count))
        return 1;

    while (!check(T_EOF)) {
        size_t before = current;
        statement();
        skip_separators();
        if (current == before && !check(T_EOF))
            advance_token();
    }
    return errors == 0 ? 0 : 1;
}
