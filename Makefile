# _*_ MakefiLE _*_

TARGET=mash

CC=gcc
CFLAG= -Wall -I. -c

all: $(TARGET).o masherror.o
	$(CC) $(TARGET).o masherror.o -o $(TARGET)

$(TARGET).o: $(TARGET).c $(TARGET).h
	$(CC) $(CFLAG) $(TARGET).c

masherror.o: masherror.c masherror.h
	$(CC) $(CFLAG) masherror.c

clean:
	rm -rf *.o job* $(TARGET)
