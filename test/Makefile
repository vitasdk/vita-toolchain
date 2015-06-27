TEST_OUTPUT = test/Makefile test/*.S test/test.elf test/test.velf test/*.o test/*.a
.PHONY: all
all: test.elf
	vita-elf-create test.elf test.velf sample-db.json
	arm-none-eabi-readelf -a test.velf
	arm-none-eabi-objdump -D -j .text.fstubs test.velf
	arm-none-eabi-objdump -s -j .data.vstubs -j .sceModuleInfo.rodata -j .sceLib.ent -j .sceExport.rodata -j .sceLib.stubs -j .sceImport.rodata -j .sceFNID.rodata -j .sceFStub.rodata -j .sceVNID.rodata -j .sceVStub.rodata -j .sce.rel test.velf

test.elf: test.o libs/libSceLibKernel.a
	arm-none-eabi-gcc -Wl,-q -nostartfiles -nostdlib $^ -o $@

test.o: test.c
	arm-none-eabi-gcc -march=armv7-a -c $< -o $@

libs/%.a: sample-db.json
	vita-libs-gen sample-db.json libs/
	$(MAKE) -C libs $*.a

clean:
	rm -f test.o libs/* test.elf test.velf
