TARGET = beepalsa

CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lm -lasound -s
SRCS = src/beepalsa.c

all : $(TARGET)

$(TARGET) : $(SRCS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean :
	-rm -f $(TARGET)
