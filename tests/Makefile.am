bin_PROGRAMS: thread-dummy.velf
thread_dummy_velf_SOURCES: thread_dummy.c
AM_CC = arm-none-eabi-gcc
BUILT_SOURCES = libs/libSceLibKernel.a
thead-dummy.velf: thread-dummy.elf
	vita-elf-create test.elf test.velf sample-db.json
thread-dummy.elf: thread-dummy.o libs/libSceLibKernel.a
	arm-none-eabi-gcc -Wl,-q -nostartfiles -nostdlib $^ -o $@
thread-dummy.o:
	arm-none-eabi-gcc -march=armv7-a -c $< -o $@
libs/libSceLibKernel.a: sample-db.json
	@rm -rf libs
	@mkdir -p libs
	vita-libs-gen sample-db.json libs
	$(MAKE) -C libs
