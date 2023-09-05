# You may decide to replace 'gcc' by 'clang' if you have 'clang on your
# local machine.  It has better error reporting.

# Some reasonable tests to try this out might be:
# make all
# ./test1 primtes-test 12345678912345
# ./test1 counting-test 12345678912345

CC=gcc
CFLAGS=-g3 -O0

hw1: primes-test counting-test ckpt libckpt.so readckpt restart

all: primes-test counting-test \
	ckpt libckpt.so \
	readckpt restart

build: all

TEST=counting-test

check: clean build
	./ckpt ./${TEST} 17 &
	sleep 4
	kill -12 `pgrep --newest ${TEST}`
	sleep 3
	./restart

#========================

ckpt: ckpt.c
	${CC} ${CFLAGS} -o $@ $<

libckpt.so: libckpt.o
	${CC} ${CFLAGS} -shared -fPIC -o $@ $<

libckpt.o: libckpt.c
	${CC} ${CFLAGS} -fPIC -c $<

readckpt: readckpt.c
	${CC} ${CFLAGS} -o $@ $<

restart: restart.c
	${CC} ${CFLAGS} -static \
        	-Wl,-Ttext-segment=5000000 -Wl,-Tdata=5100000 -Wl,-Tbss=5200000 \
        	-o $@ $<

# https://stackoverflow.com/questions/36692315/what-exactly-does-rdynamic-do-and-when-exactly-is-it-needed
primes-test: primes-test.c
	${CC} ${CFLAGS} -rdynamic -o $@ $<

counting-test: counting-test.c 
	${CC} ${CFLAGS} -rdynamic -o $@ $<

test: test.c
	${CC} ${CFLAGS} -o $@ $<

test%: test libconstructor%.so
	cp $< $@

# -fPIC required for shared libraries (.so)
libconstructor%.so: constructor%.o
	${CC} ${CFLAGS} -shared -fPIC -o $@ $<
constructor%.o: constructor%.c
	${CC} ${CFLAGS} -fPIC -c $<

#========================
proc-self-maps: proc-self-maps.c
	${CC} ${CFLAGS} -DSTANDALONE -o $@ $<

#========================
save-restore-memory: save-restore-memory.c
	${CC} ${CFLAGS} -o $@ $<

#========================

clean:
	rm -f a.out primes-test counting-test
	rm -f libconstructor?.so constructor?.o test? test
	rm -f proc-self-maps save-restore-memory save-restore.dat
	rm -f ckpt libckpt.so libckpt.o myckpt readckpt restart

dist: clean
	dir=`basename $$PWD` && cd .. && tar czvf $$dir.tar.gz ./$$dir
	dir=`basename $$PWD` && ls -l ../$$dir.tar.gz

.PRECIOUS: libconstructor%.so constructor%.o
