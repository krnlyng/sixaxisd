TARGET = sixaxisd

all: $(TARGET)

CFLAGS = -O2 -Wall

OBJS = uinputdev.o sixsrv.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

clean:
	rm -f $(OBJS) $(TARGET)
