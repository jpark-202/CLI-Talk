all: lets-talk.c
	gcc -c lets-talk.c list.c
	gcc -o lets-talk -pthread lets-talk.o list.o
	
Valgrind: 
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./lets-talk 3000 localhost 3001
clean:
	-rm -f *.o lets-talk
