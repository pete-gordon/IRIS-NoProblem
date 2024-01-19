TARGET = mkdemo.exe
OBJECTS = noproblem.o asm6502.o music.o
DEMOFILE = noproblm.tap
SYMFILE = noproblm.sym
CC = gcc

CFLAGS = -Wall -Werror -O3 -g
LDFLAGS = -g

.PHONY: all clean

all: $(DEMOFILE)

-include $(OBJECTS:.o=.d)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)
	@$(CC) -MM $(CFLAGS) -MT "$@" $< > $*.d

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LFLAGS)

$(DEMOFILE): $(TARGET)
	$(TARGET)

clean:
	del $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d) $(DEMOFILE) $(SYMFILE)

