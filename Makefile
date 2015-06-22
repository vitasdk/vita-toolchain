TARGETS = vita-libs-gen vita-elf-create
libsgen_OBJS = vita-libs-gen.o vita-import.o vita-import-parse.o
elfcreate_OBJS = vita-elf-create.o vita-elf.o vita-import.o vita-import-parse.o elf-defs.o sce-elf.o varray.o elf-utils.o
ALL_OBJS = $(libsgen_OBJS) $(elfcreate_OBJS)

CC = gcc
CFLAGS = -Wall -g # -O2
libsgen_LIBS = -ljansson
elfcreate_LIBS = -lelf -ljansson

all: $(TARGETS)

vita-libs-gen: $(libsgen_OBJS)
	$(CC) $(LDFLAGS) $^ $(libsgen_LIBS) -o $@

vita-elf-create: $(elfcreate_OBJS)
	$(CC) $(LDFLAGS) $^ $(elfcreate_LIBS) -o $@

%.o: %.c
	$(CC) -MD -MP $(CFLAGS) -c $< -o $@

TEST_OUTPUT = test/Makefile test/*.S test/test.elf test/test.velf test/*.o test/*.a
.PHONY: test
test: test/test.elf vita-elf-create
	./vita-elf-create test/test.elf test/test.velf sample-db.json
	arm-none-eabi-readelf -a test/test.velf
	arm-none-eabi-objdump -D -j .text.fstubs test/test.velf
	arm-none-eabi-objdump -s -j .data.vstubs -j .sceModuleInfo.rodata -j .sceLib.ent -j .sceExport.rodata -j .sceLib.stubs -j .sceImport.rodata -j .sceFNID.rodata -j .sceFStub.rodata -j .sceVNID.rodata -j .sceVStub.rodata -j .sce.rel test/test.velf

test/test.elf: test/test.o test/libSceLibKernel.a
	arm-none-eabi-gcc -Wl,-q -nostartfiles -nostdlib $^ -o $@

test/test.o: test/test.c
	arm-none-eabi-gcc -march=armv7-a -c $< -o $@

test/%.a: vita-libs-gen sample-db.json
	./vita-libs-gen sample-db.json test/
	$(MAKE) -C test $*.a

-include $(ALL_OBJS:.o=.d)

clean:
	rm -f $(ALL_OBJS) $(ALL_OBJS:.o=.d) $(TARGETS) $(TEST_OUTPUT)
