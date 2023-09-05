#define _GNU_SOURCE /* Required for 'constructor' attribute (GNU extension) */

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
#include <sys/mman.h>


#define NAME_LEN 80
struct proc_maps_line {
    void *start;
    void *end;
    char rwxp[4];
    int read, write, execute; // Not used in this version of the code
    char name[NAME_LEN];      // for debugging only
};

int match_one_line(int proc_maps_fd,
                   struct proc_maps_line *proc_maps_line) {
    unsigned long int start, end;
    char rwxp[4];
    char tmp[10];
    char filename[80];
    int tmp_stdin = dup(0); // Make a copy of stdin
    dup2(proc_maps_fd, 0);  // Duplicate proc_maps_fd to stdin
    // scanf() reads stdin (fd:0). It's a dup of proc_maps_fd ('same struct file').
    int rc = scanf("%lx-%lx %4c %*s %*s %*[0-9 ]%[^\n]\n",
                   &start, &end, rwxp, filename);

    // fseek() removes the EOF indicator on stdin for any future calls to scanf().
    assert(fseek(stdin, 0, SEEK_CUR) == 0);
    dup2(tmp_stdin, 0); // Restore original stdin; proc_maps_fd offset was advanced.
    close(tmp_stdin);
    if (rc == EOF || rc == 0) {
        proc_maps_line->start = NULL;
        proc_maps_line->end = NULL;
        return EOF;
    }
    if (rc == 3) { // if no filename on /proc/self/maps line:
        strncpy(proc_maps_line->name,
                "ANONYMOUS_SEGMENT", strlen("ANONYMOUS_SEGMENT") + 1);
    } else {
        assert(rc == 4);
        strncpy(proc_maps_line->name, filename, NAME_LEN - 1);
        proc_maps_line->name[NAME_LEN - 1] = '\0';
    }
    proc_maps_line->start = (void *) start;
    proc_maps_line->end = (void *) end;
    memcpy(proc_maps_line->rwxp, rwxp, 4);
#ifdef DEBUG
    printf("\n\n%s (%c%c%c%c)\n"
           "  Address-range: %p - %p\n\n",
           proc_maps_line->name,
           proc_maps_line->rwxp[0], proc_maps_line->rwxp[1], proc_maps_line->rwxp[2], proc_maps_line->rwxp[3],
           proc_maps_line->start, proc_maps_line->end);

    if (rc == 0) {
        printf("proc_self_maps: filename: %s\n", filename); // for debugging
        printf("Name from struct: %s\n", proc_maps_line->name);
        assert(proc_maps_line->start != NULL);
    }
#endif
    return 0;
}

// This might cause scanf to call mmap().  If this is a problem, call it a second
//   time, and scanf() can re-use the previous memory, instead of mmap'ing more.
int proc_self_maps(struct proc_maps_line proc_maps[]) {
    // NOTE:  fopen calls malloc() and potentially extends the heap segment.
    int proc_maps_fd = open("/proc/self/maps", O_RDONLY);
    // for debugging
    int i = 0;
    int rc = -2; // any value that will not terminate the 'for' loop.
    for (i = 0; rc != EOF; i++) {
        rc = match_one_line(proc_maps_fd, &proc_maps[i]);
    }
    printf("Saved %d segments\n", i - 1);
    close(proc_maps_fd);
    return 0;
}


size_t write_loop(int fd, void *buf, size_t count) {
    int rc = write(fd, buf, count);
    if (rc == -1) {
        perror("write");
        exit(1);
    }
    while (rc < count) {
        printf("Write loop: only wrote %d/%d, calling write again\n", rc, count);
        int rb = write(fd, buf + rc, count - rc);
        if (rb < 0) {
            perror("WRITE ERROR");
            return rb;
        }
        rc += rb;
    }
    assert(rc == count);
    return rc;
}

void sigusr2_handler() {
    int fd = open("myckpt", O_CREAT | O_WRONLY | O_EXCL);
#ifdef DEBUG
    printf("myckpt open");
#endif
    if (fd < 0)
        perror("Error in opening myckpt file");

    int total_written = 0;

    struct ucontext ctx;
    int is_restart = 0;
    getcontext(&ctx); // save register
    if (is_restart != 0) {
        printf("Returning to user program\n");
        return;
    }
    is_restart = 1;
    int written = write(fd, &ctx, sizeof(ctx));
    if (written == -1) {
        perror("Error in writing context.");
        exit(1);
    }
#ifdef DEBUG
    printf("Wrote context to file: %i bytes\n", written);
#endif
    total_written += written;

    // save memory segments
    int segments_written = 0;
    struct proc_maps_line proc_maps[1000];
    assert(proc_self_maps(proc_maps) == 0);
    int i = 0;

    for (i = 0; proc_maps[i].start != NULL; i++) {


        if (proc_maps[i].rwxp[0] == '-') {
#ifdef DEBUG
            printf("Skipping read protected segment\n");
#endif
            continue;
        }

        if (strcmp(proc_maps[i].name, "[vdso]") == 0) {
#ifdef DEBUG
            printf("Skipping [vdso]\n");
#endif
            continue;
        }

        if (strcmp(proc_maps[i].name, "[vsyscall]") == 0) {
#ifdef DEBUG
            printf("Skipping [vsyscall]\n");
#endif
            continue;
        }

        int header_written = write(fd, &proc_maps[i], sizeof(struct proc_maps_line)); // write the memory segment header
#ifdef DEBUG
        printf("header write\n");
#endif
        if (header_written != sizeof(struct proc_maps_line)) {
            printf("HEADER WRITE ERROR: Wrote only %d bytes when struct is %d bytes!\n", header_written,
                   sizeof(struct proc_maps_line));
        }

        total_written += header_written;

        int memory_written = write_loop(fd, proc_maps[i].start,
                                        proc_maps[i].end - proc_maps[i].start); // write the memory itself
#ifdef DEBUG
        printf("memory write\n");
#endif
        if (memory_written != proc_maps[i].end - proc_maps[i].start) {
            printf("MEMORY WRITE ERROR: Wrote only %d bytes when struct is %d bytes!\n", memory_written,
                   proc_maps[i].end - proc_maps[i].start);
        }

        total_written += memory_written;
        segments_written += 1;
    }
#ifdef DEBUG
    printf("Last start address was %p\n", proc_maps[i].start);
#endif

    printf("Done writing. Wrote %d segments out of %d total segments\n", segments_written, i);
    printf("Total size: %d\n", total_written);
    chmod("myckpt", S_IRUSR | S_IWUSR);
    close(fd);

    exit(0);
}

void __attribute__((constructor)) my_constructor() {
    signal(SIGUSR2, &sigusr2_handler);

    printf("*************************************\n"
           "***   Signal handler registered!. ***\n"
           "*************************************\n");
    fflush(stdout);
}
