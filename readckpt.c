#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>

#define NAME_LEN 80
struct proc_maps_line {
    void *start;
    void *end;
    char rwxp[4];
    int read, write, execute; // Not used in this version of the code
    char name[NAME_LEN]; // for debugging only
};

// returns bytes written - or 0 if the end of file was encountered
int read_loop(int fd, const void *buf, size_t count) {
    int read_count;
    int read_last;
    while (read_count < count) {
        read_last = read(fd, (void *) buf + read_count, count - read_count);
        if (read_count <= 0) {
            printf("Read error: %d\n", errno);
            return read_count;
        }
        read_count += read_last;
    }
    return count;
}

int main(int argc, char *argv[]) {
    int total_read = 0;
    printf("Ucontext: %d\n", sizeof(struct ucontext));
    printf("Proc_maps_line: %d\n", sizeof(struct proc_maps_line));
    int fd = open("myckpt", O_RDONLY);
    if (fd < 0) {
        printf("Open error: %d\n", errno);
    }

    // read register context
    struct ucontext ctx;
    int read_context = read(fd, &ctx, sizeof(struct ucontext));
    printf("Got context of size %d\n", read_context);
    total_read += read_context;

    // loop and read proc_maps_lines
    printf("    *** Memory segments of %s ***\n", argv[0]);
    struct proc_maps_line proc_maps[1000];
    int read_bytes;
    int i;
    for (i = 0; 1; i++) {
        printf("Reading memory segment %d header\n", i);
        read_bytes = read(fd, &proc_maps[i], sizeof(struct proc_maps_line));
        total_read += read_bytes;
        if (read_bytes == 0) {
            printf("Reached end of file. Total %d memory segments\n", i);
            break;
        } else {
            printf("Read %d bytes for first header\n", read_bytes);
        }

        if (read_bytes < sizeof(struct proc_maps_line)) {
            printf("ONLY READ %d BYTES OUT OF %d\n", read_bytes, sizeof(struct proc_maps_line));
        }
        printf("Segment %d: %s (%c%c%c)\n"
               "  Address-range: %p - %p\n",
               i,
               proc_maps[i].name,
               proc_maps[i].rwxp[0], proc_maps[i].rwxp[1], proc_maps[i].rwxp[2],
               proc_maps[i].start, proc_maps[i].end);
        // discard actual memory contents
        printf("Total size for segment %d: %d + %d = %d\n\n\n", i, sizeof(struct proc_maps_line),
               proc_maps[i].end - proc_maps[i].start,
               sizeof(struct proc_maps_line) + (proc_maps[i].end - proc_maps[i].start));
        lseek(fd, proc_maps[i].end - proc_maps[i].start, SEEK_CUR);
    }
    printf("Done. Total read from file was: %d\n", total_read);
}
