TARGET = musicconv.exe
OBJECTS = main.o

CC = gcc
CFLAGS = -Wall -Werror -Os -g
LDFLAGS = -g

.PHONY: all clean

all: $(TARGET)

-include $(OBJECTS:.o=.d)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)
	@$(CC) -MM $(CFLAGS) -MT "$@" $< > $*.d

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LFLAGS)

clean:
	del $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)

