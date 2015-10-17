GZIP_COMPRESSION ?= no

CFLAGS=-I../../include -I.. -std=gnu99
ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS		+= -DESPFS_GZIP
endif

OBJS=main.o
TARGET=mkespfsimage

$(TARGET): $(OBJS)
ifeq ("$(GZIP_COMPRESSION)","yes")
	$(CC) -o $@ $^ -lz
else
	$(CC) -o $@ $^
endif

clean:
	rm -f $(TARGET) $(OBJS)
