
TARGET := bouncy.exe
OBJECTS := main.o
CFLAGS += -g -O0 -Wall -Werror -Dmain=SDL_main -IC:/MinGW/include/SDL
LDFLAGS += -mwindows -lmingw32 -lSDLmain -lSDL -lm

.PHONY: all clean

all: $(TARGET)

-include $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)
	@$(CC) -MM -c $< > $*.d $(CFLAGS)

clean:
	del $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)
