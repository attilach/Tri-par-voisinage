GCC=gcc -pthread -lrt -Wall -std=gnu99

main: main.o 
	$(GCC) $^ -o $@

main.o: main.c
	$(GCC) $< -c

clean:
	rm -f *.o main
