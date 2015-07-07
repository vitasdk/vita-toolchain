bin_PROGRAMS = vita-libs-gen vita-elf-create
vita_libs_gen_SOURCES = vita-libs-gen.c vita-import.c vita-import-parse.c
vita_elf_create_SOURCES = vita-elf-create.c vita-elf.c vita-import.c vita-import-parse.c elf-defs.c sce-elf.c varray.c elf-utils.c
vita_libs_gen_LDADD = -ljansson
vita_elf_create_LDADD = -lelf -ljansson

#all: $(bin_PROGRAMS)

#vita-libs-gen: $(libsgen_OBJS)
#	$(CC) $(LDFLAGS) $^ $(libsgen_LIBS) -o $@

#ita-elf-create: $(elfcreate_OBJS)
#	$(CC) $(LDFLAGS) $^ $(elfcreate_LIBS) -o $@

#%.o: %.c
#	$(CC) -MD -MP $(CFLAGS) -c $< -o $@

#-include $(ALL_OBJS:.o=.d)
#
#clean:
#	rm -f $(ALL_OBJS) $(ALL_OBJS:.o=.d) $(bin_PROGRAMS) $(TEST_OUTPUT)
