make:
	gcc -o empty.x empty.c
	strace -o empty.trace ./empty.x
	gcc -o part1.x part1.c
	strace -o part1.trace ./part1.x
clean:
	rm *.x *.trace