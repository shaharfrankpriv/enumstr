#include <stdio.h>
#include <stdlib.h>
// /usr/local/include
#include <peppa.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

#define ENUMSTRFILE "./enumstr.peg"

char *_progname = "";

char *basename(char *path)
{
    char *s = strrchr(path, '/');
    return s ? s+1 : path;
}

void SetProgName(char *path)
{
    _progname = basename(path);
}

void Panic(const char *fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    fprintf(stderr, "[%s] Panic: %s\n", _progname, buffer);
    exit(1);
}

ssize_t NRead(int fd, void *buf, size_t nbytes)
{
    ssize_t left = nbytes;
    while (left > 0) {
        int n = read(fd, buf, nbytes);
        if (n <= 0) {
            return n;   // 0 == EOF, -1 == error
        }
        left -= n;
        buf = (void *)((char*)buf + n);
    }
    return nbytes;
}

#define ERRSTRSZ 256
typedef struct ErrStr {
    char str[ERRSTRSZ];
} ErrStr;

void *FormatErrStr(ErrStr *errstr, const char *fmt, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(errstr->str, sizeof(errstr->str), fmt, args);
    va_end(args);
    return NULL;
}

char *ReadFile(char *path, ErrStr *errstr)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return FormatErrStr(errstr, "can't open '%s' - %m", path);
    }

    struct stat sbuf;
    fstat(fd, &sbuf);
    off_t size = sbuf.st_size;

    char *buf = malloc(size);
    if (!buf) {
        return FormatErrStr(errstr, "can't open alloc buffer for '%s' size %lld", path, size);
    }

    if (NRead(fd, buf, size) != size) {
        return FormatErrStr(errstr, "can't read grammer file '%s' - %m", path);
    }
    return buf;
}


P4_Error
P4_ProcessEnums(P4_Node *root, FILE *file, ErrStr *errstr)
{
    return 0;
}

int main(int argc, char **argv)
{
    SetProgName(argv[0]);
    argc--; // ignore argv0

    if (argc < 1) {
        Panic("At least one source file must be specified.");
    }

    ErrStr errstr;
    char *buf = ReadFile(ENUMSTRFILE, &errstr);
    if (!buf) {
        Panic("Can't read grammer file: %s", errstr.str);
    }

    P4_Grammar *grammar = P4_LoadGrammar(buf);

    for (int i; i < argc; i++) {
        P4_Error error = P4_Ok;
        long result = 0;

        char *srcbuf = ReadFile(argv[i], &errstr);
        if (!buf) {
            Panic("Can't read grammer file: %s", errstr.str);
        }
        P4_Source *source = P4_CreateSource(srcbuf, "SourceFile");
        if ((error = P4_Parse(grammar, source)) != P4_Ok) {
            Panic("parse: %s", P4_GetErrorMessage(source));
        } else if ((error = P4_ProcessEnums(P4_GetSourceAst(source), stdout, &errstr)) != P4_Ok) {
            Panic("error: eval: %d", error);
        } else {
            printf("[Out] %ld\n\n", result);
        }
        P4_DeleteSource(source);
    }

    P4_DeleteGrammar(grammar);
}
