all:
	gcc forker.c -o forker
	gcc fork_tree.c -o fork_tree
	gcc -Wall -fPIC -shared -o wrap_fork.so wrap_fork.c -ldl
