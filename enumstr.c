#include <stdio.h>
#include <stdlib.h>
// /usr/local/include
#include <peppa.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


#define ENUMSTRFILE "./enumstr.peg"

void panic(const char *fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    fprintf(stderr, "Panic: %s\n", buffer);
    exit(1);
}

/** Load Grammer file */
P4_Grammar*  P4_CreateEnumStrGrammar() {
    int fd = open(ENUMSTRFILE, O_RDONLY);
    if (fd < 0) {
        panic("Can't open '%s' - %m", ENUMSTRFILE);
    }

    int fd = fileno(file);
    struct stat buf;
    fstat(fd, &buf);
    off_t size = buf.st_size;

    char *buf = malloc(size);
    if (!file) {
        panic("Can't open alloc buffer for '%s' size %lld", ENUMSTRFILE, size);
    }

    size_t bytes = fread(buf, size, )
    return P4_LoadGrammar()
