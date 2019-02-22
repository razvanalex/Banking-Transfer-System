CC=gcc
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
SEL_SRV=server
CLT=client

all: $(SEL_SRV) $(CLT)

$(SEL_SRV):$(SEL_SRV).c
	$(CC) -o $(SEL_SRV) $(CCFLAGS) $(LIBSOCKET) $(SEL_SRV).c

$(CLT):	$(CLT).c
	$(CC) -o $(CLT)  $(CCFLAGS) $(LIBSOCKET) $(CLT).c

clean:
	rm -f *.o *~
	rm -f $(SEL_SRV) $(CLT)
	rm *.log

