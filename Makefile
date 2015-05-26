TARGET = vita-libs-gen
OBJS = vita-libs-gen.o vita-import.o vita-import-parse.o

CFLAGS = -Wall -O2
LIBS = -ljansson

$(TARGET): $(OBJS)
	gcc $^ $(LIBS) -o $@

%.o: %.c
	gcc $(CFLAGS) -c $? -o $@

clean:
	@rm -rf $(OBJS) $(TARGET)
