all: compile \
	run \
	clean
compile: 
	gcc -O2 -Wall -pthread quicksort.c -o quicksort
run: 
	./quicksort
clean: 
	rm ./quicksort
