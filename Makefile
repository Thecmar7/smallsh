sh: smallsh.o dynamicArray.o
	gcc -Wall smallsh.o dynamicArray.o -o smallsh 

smallsh.o:
	gcc -Wall -c smallsh.c 

dynamicArray.o:	dynamicArray.c dynamicArray.h
	gcc -Wall -c dynamicArray.c

clear: 
	clear 
	
clean:
	rm -f smallsh
	rm -f *.o
