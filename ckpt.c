#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char *argv[]) {

    if (argc < 2 || access(argv[1], X_OK) != 0) {
        fprintf(stderr, "*** Missing target executable or no such file.\n\n");
        exit(1);
    }

    char buf[1000];
    buf[sizeof(buf) - 1] = '\0';
    snprintf(buf, sizeof buf, "%s/libckpt.so", dirname(argv[0]));
    // Guard against buffer overrun:
    assert(buf[sizeof(buf) - 1] == '\0');
    setenv("TARGET", argv[1], 1);
    setenv("LD_PRELOAD", buf, 1);

    int rc = execvp(argv[1], argv + 1);
    // We should not reach here.
    if (rc == -1) {
        fprintf(stderr, "Executable '%s' not found.\n", argv[1]);
        return 1;
    }
    return 0;
}
