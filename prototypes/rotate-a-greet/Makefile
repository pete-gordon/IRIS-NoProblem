
TARGET := rag.exe
OBJECTS := main.o
CC = gcc
CFLAGS += -g -O0 -Wall -Werror -Dmain=SDL_main
LDFLAGS += -mwindows -lmingw32 -lSDL2main -lSDL2 -lm

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
