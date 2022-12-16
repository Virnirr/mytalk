CCFLAGS = -Wall -g -pedantic

LDFLAGS = -Wall -g -pedantic

OBJTALK = ~pn-cs357/Given/Talk/lib64

LINKTALK =  

mytalk: mytalk.o ~pn-cs357/Given/Talk/lib64/libtalk.a
	gcc $(LDFLAGS) -o mytalk -L $(OBJTALK) mytalk.o -ltalk -lncurses

mytalk.o: mytalk.c ~pn-cs357/Given/Talk/talk.h
	gcc -c $(CCFLAGS) -I $(LINKTALK) mytalk.c

clean:
	rm *.o
	rm -f mytalk
	echo DONE