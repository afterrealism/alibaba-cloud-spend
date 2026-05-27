CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk4) $(shell pkg-config --cflags libcurl) $(shell pkg-config --cflags json-c) $(shell pkg-config --cflags openssl 2>/dev/null)
LIBS = $(shell pkg-config --libs gtk4) $(shell pkg-config --libs libcurl) $(shell pkg-config --libs json-c) $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
TARGET = alibaba-cloud-spend

SRCS = main.c api.c ui.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
