TARGETS = vita-libs-gen vita-elf-create
libsgen_OBJS = vita-libs-gen.o vita-import.o vita-import-parse.o
elfcreate_OBJS = vita-elf-create.o vita-elf.o
ALL_OBJS = $(libsgen_OBJS) $(elfcreate_OBJS)

CFLAGS = -Wall -O2
libsgen_LIBS = -ljansson
elfcreate_LIBS = -lelf

all: $(TARGETS)

vita-libs-gen: $(libsgen_OBJS)
	gcc $^ $(libsgen_LIBS) -o $@

vita-elf-create: $(elfcreate_OBJS)
	gcc $^ $(elfcreate_LIBS) -o $@

%.o: %.c
	gcc -MD -MP $(CFLAGS) -c $? -o $@

-include $(ALL_OBJS:.o=.d)

clean:
	@rm -rf $(ALL_OBJS) $(ALL_OBJS:.o=.d) $(TARGETS)
