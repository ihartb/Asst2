all:detector
helpers:helpers.c
	gcc -g helpers -o -pthread helpers.c -lm
detector:helpers.c Asst2.c
	gcc -c -Wall -g helpers.c -lm
	gcc -g -o detector -pthread Asst2.c helpers.c -lm
clean:
	rm -f helpers.o detector
