#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <ucontext.h>

void recursive();
void do_work();

#define NAME_LEN 80
struct proc_maps_line {
  void *start;
  void *end;
  char rwxp[4];
  int read, write, execute; // Not used in this version of the code
  char name[NAME_LEN]; // for debugging only
};

int main(int argc, char* argv[]) {
	// go deep in the stack to avoid getting overriden by a.out's stack
	recursive(100);	
}

// returns bytes written - or 0 if the end of file was encountered
int read_loop(int fd, void *buf, size_t count) {
        int rc = read(fd, buf, count);
        if (rc == -1) {
                printf("Read address: %p", buf);
                perror("read");
                exit(1);
        }
        while (rc < count) {
                rc += read(fd, (char *)buf + rc, count - rc);
        }
        assert(rc == count);
}

void recursive(int levels) {
	if(levels > 0) {
		recursive(levels - 1);
	} else {
		do_work();
	}
}

void do_work() {
        int total_read = 0;
	// read register context
	struct ucontext ctx;
        // initialize
        int is_restart = 0;
        int context_return = getcontext(&ctx);
        if(context_return == -1) {
                perror("getcontext");
                return;
        } else if(is_restart == 1) {
                printf("This is restart. returning\n");
                return;
        } else {
                printf("getcontext returned: %d\n", is_restart);
        }
        is_restart = 1;
	int fd = open("myckpt", O_RDONLY);
        //struct ucontext dummy;
	int bytes_read = read(fd, &ctx, sizeof(struct ucontext));
        total_read += bytes_read;
        printf("Context: read %d bytes\n", bytes_read);
        perror("context read");

	// read memory segments
	struct proc_maps_line proc_maps[1000];
        int read_bytes;
        int i;
        for(i = 0; 1; i++) {
                read_bytes = read(fd, &proc_maps[i], sizeof(struct proc_maps_line));
                total_read += read_bytes;
                printf("Read context bytes: %d\n", read_bytes);
                if(read_bytes == 0) {
                        break;
                } else {
                        printf("Segment %d: %s (%c%c%c)\n"
                        "  Address-range: %p - %p\n",
                        i,
                        proc_maps[i].name,
                        proc_maps[i].rwxp[0], proc_maps[i].rwxp[1], proc_maps[i].rwxp[2],
                        proc_maps[i].start, proc_maps[i].end);
                        printf("Total size for segment %d: %d + %d = %d\n", i, sizeof(struct proc_maps_line), proc_maps[i].end - proc_maps[i].start, sizeof(struct proc_maps_line) + (proc_maps[i].end - proc_maps[i].start));

                	void *addr = mmap(
				proc_maps[i].start,
				proc_maps[i].end - proc_maps[i].start,
				PROT_READ|PROT_WRITE|PROT_EXEC,
				MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
			if(addr == MAP_FAILED) { perror("mmap"); }
                        // read and load memory segment from file
                        int memory_read = read(fd, proc_maps[i].start, proc_maps[i].end - proc_maps[i].start);
                        total_read += memory_read;
                        printf("Read memory bytes: %d\n", memory_read);
                        printf("\n\n\n");
		}
        }

        printf("Total bytes read: %d\n", total_read);

        // pull the trigger
        printf("Pulling the trigger\n");
        setcontext(&ctx);
	
}

