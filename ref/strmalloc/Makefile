OBJECTS=teststrmalloc.o strmalloc.o

teststrmalloc: $(OBJECTS)
	gcc -o teststrmalloc $(OBJECTS)

testsstrmalloc.o: teststrmalloc.c strmalloc.h
strmalloc.o: strmalloc.c strmalloc.h

clean:
	rm -f teststrmalloc teststrmalloc.exe *.o
