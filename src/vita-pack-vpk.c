#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <zip.h>

#define DEFAULT_OUTPUT_FILE "output.vpk"

static void usage(const char *arg);

static const struct option long_options[] = {
	{"sfo", required_argument, NULL, 's'},
	{"eboot", required_argument, NULL, 'b'},
	{"add", required_argument, NULL, 'a'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static int add_file_zip(zip_t *zip, const char *src, const char *dst)
{
	int ret;
	zip_source_t *zip_src;

	zip_src = zip_source_file(zip, src, 0, 0);
	if (!zip_src) {
		printf("Error adding \'%s\': %s\n", src,
			zip_strerror(zip));
		return 0;
	}

	ret = zip_file_add(zip, dst, zip_src, 0);
	if (ret == -1) {
		printf("Error adding \'%s\': %s\n", src,
			zip_strerror(zip));
		return 0;
	}

	return 1;
}

static struct {
	char **src;
	char **dst;
	int num;
} additional_list;

static void additional_list_add(char *src, char *dst)
{
	additional_list.src = realloc(additional_list.src,
		sizeof(*additional_list.src) * (additional_list.num + 1));
	additional_list.dst = realloc(additional_list.dst,
		sizeof(*additional_list.dst) * (additional_list.num + 1));

	additional_list.src[additional_list.num] = src;
	additional_list.dst[additional_list.num] = dst;

	additional_list.num++;
}

static void additional_list_init()
{
	additional_list.src = NULL;
	additional_list.dst = NULL;
	additional_list.num = 0;
}

static void additional_list_free()
{
	int i;

	for (i = 0; i < additional_list.num; i++) {
		free(additional_list.src[i]);
		free(additional_list.dst[i]);
	}

	free(additional_list.src);
	free(additional_list.dst);
}

static void parse_add_subopt(char *optarg)
{
	char *src;
	char *dst;
	const char *equals;
	size_t src_len;
	size_t dst_len;
	size_t len = strlen(optarg);

	equals = strchr(optarg, '=');
	if (!equals || (equals == optarg + len - 1))
		return;

	src_len = equals - optarg;
	dst_len = len - (equals - optarg + 1);

	src = malloc(sizeof(char) * (dst_len + 1));
	dst = malloc(sizeof(char) * (src_len + 1));

	strncpy(src, optarg, src_len);
	src[src_len] = '\0';

	strncpy(dst, optarg + src_len + 1, dst_len);
	dst[dst_len] = '\0';

	additional_list_add(src, dst);
}

int main(int argc, char *argv[])
{
	int i;
	int err;
	zip_t *zip;
	int opt;
	char *output = NULL;
	char *sfo = NULL;
	char *eboot = NULL;

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	additional_list_init();

	while ((opt = getopt_long(argc, argv, "hs:b:a:", long_options, NULL)) != -1) {
		switch (opt) {
		case 's':
			sfo = strdup(optarg);
			break;
		case 'b':
			eboot = strdup(optarg);
			break;
		case 'a':
			parse_add_subopt(optarg);
			break;
		case 'h':
			usage(argv[0]);
			goto error_wrong_args;
		}
	}

	if (!sfo) {
		printf(".sfo file missing.\n");
		goto error_wrong_args;
	}

	if (!eboot) {
		printf(".bin file missing.\n");
		goto error_wrong_args;
	}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		output = strdup(argv[0]);
	else
		output  = strdup(DEFAULT_OUTPUT_FILE);

	zip = zip_open(output, ZIP_CREATE | ZIP_TRUNCATE, &err);
	if (!zip) {
		printf("Error creating: \'%s\': %s\n", output,
			zip_strerror(zip));
		goto error_create_zip;
	}

	if (!add_file_zip(zip, sfo, "sce_sys/param.sfo"))
		goto error_add_zip;

	if (!add_file_zip(zip, eboot, "eboot.bin"))
		goto error_add_zip;

	for (i = 0; i < additional_list.num; i++) {
		if (!add_file_zip(zip, additional_list.src[i],
				  additional_list.dst[i]))
			goto error_add_zip;
	}

	err = zip_close(zip);
	if (err == -1) {
		printf("Error creating: \'%s\': %s\n", output,
			zip_strerror(zip));
		goto error_add_zip;
	}

	free(output);
	free(sfo);
	free(eboot);
	additional_list_free();

	return 0;

error_add_zip:
	zip_discard(zip);

error_create_zip:
	free(output);

error_wrong_args:
	if (sfo)
		free(sfo);
	if (eboot)
		free(eboot);

	additional_list_free();

	return -1;
}

void usage(const char *arg)
{
	printf("Usage:\n\t%s [OPTIONS] output.vpk\n\n"
		"  -s, --sfo=param.sfo     sets the param.sfo file\n"
		"  -b, --eboot=eboot.bin   sets the eboot.bin file\n"
		"  -a, --add src=dst       adds the file src to the vpk as dst\n"
		"  -h, --help              displays this help and exit\n"
		, arg);
}
