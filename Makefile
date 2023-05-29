TARGET = mkdemo.exe
OBJECTS = novademo.o asm6502.o

CFLAGS = -Wall -Werror -O0 -g
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
