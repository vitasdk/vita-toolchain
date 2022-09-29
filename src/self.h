#pragma once

#include <inttypes.h>

// some info taken from the wiki, see https://www.psdevwiki.com/ps3/SELF_-_SPRX

const uint8_t digest_constant[0x14] =  {0x62, 0x7C, 0xB1, 0x80, 0x8A, 0xB9, 0x38, 0xE3, 0x2C, 0x8C, 0x09, 0x17, 0x08, 0x72, 0x6A, 0x57, 0x9E, 0x25, 0x86, 0xE4};

#pragma pack(push, 1)
typedef struct {
	uint32_t magic;                 /* 53434500 = SCE\0 */
	uint32_t version;               /* header version 3*/
	uint16_t sdk_type;              /* */
	uint16_t header_type;           /* 1 self, 2 unknown, 3 pkg */
	uint32_t metadata_offset;       /* metadata offset */
	uint64_t header_len;            /* self header length */
	uint64_t elf_filesize;          /* ELF file length */
	uint64_t self_filesize;         /* SELF file length */
	uint64_t unknown;               /* UNKNOWN */
	uint64_t self_offset;           /* SELF offset */
	uint64_t appinfo_offset;        /* app info offset */
	uint64_t elf_offset;            /* ELF #1 offset */
	uint64_t phdr_offset;           /* program header offset */
	uint64_t shdr_offset;           /* section header offset */
	uint64_t section_info_offset;   /* section info offset */
	uint64_t sceversion_offset;     /* version offset */
	uint64_t controlinfo_offset;    /* control info offset */
	uint64_t controlinfo_size;      /* control info size */
	uint64_t padding;
} SCE_header;

typedef struct {
	uint64_t authid;                /* auth id */
	uint32_t vendor_id;             /* vendor id */
	uint32_t self_type;             /* app type */
	uint64_t version;               /* app version */
	uint64_t padding;               /* UNKNOWN */
} SCE_appinfo;

typedef struct {
	uint32_t unk1;
	uint32_t unk2;
	uint32_t unk3;
	uint32_t unk4;
} SCE_version;

typedef struct {
	uint32_t type;
	uint32_t size;
	uint32_t has_next;
	uint32_t pad;
} SCE_controlinfo;


typedef enum {
	SCE_ELF_DIGEST_INFO = 4,
	SCE_NPDRM_INFO = 5,
	SCE_BOOTPARAM_INFO = 6,
	SCE_SHAREDSECRET_INFO = 7,
} SCE_controlinfo_type;

typedef struct {
	SCE_controlinfo head;
	uint8_t constant[0x14];   /* same for every PSVita/PS3 SELF, hardcoded as: 627CB1808AB938E32C8C091708726A579E2586E4 */
	uint8_t elf_digest[0x20]; /* on PSVita: SHA-256 of source ELF file, on PS3: SHA-1 */
	uint8_t padding[8];
	uint32_t min_required_fw; /* ex: 0x363 for 3.63, seems to be ignored */
} SCE_elf_digest_info;

// npdrm info is only needed for selfs, not fselfs
typedef struct {
	SCE_controlinfo head;
	uint32_t magic;               /* 7F 44 52 4D (".DRM") */
	uint32_t finalized_flag;      /* ex: 80 00 00 01 */
	uint32_t drm_type;            /* license_type ex: 2 local, 0xD free with license */
	uint32_t padding;
	uint8_t content_id[0x30];
	uint8_t digest[0x10];         /* ?sha-1 hash of debug self/sprx created using make_fself_npdrm? */
	uint8_t padding_78[0x78];
	uint8_t hash_signature[0x38]; /* unknown ECDSA224 signature */
} SCE_npdrm_info;

typedef struct {
	SCE_controlinfo head;
	uint32_t is_used;               /* always set to 1 */
	uint32_t attr;                  /* controls several app settings */
	uint32_t phycont_memsize;       /* physically contiguous memory budget */
	uint32_t total_memsize;         /* total memory budget (user + phycont) */
	uint32_t filehandles_limit;     /* max number of opened filehandles simultaneously */
	uint32_t dir_max_level;         /* max depth for directories support */
	uint32_t encrypt_mount_max;     /* UNKNOWN */
	uint32_t redirect_mount_max;    /* UNKNOWN */
	uint8_t unk[0xE0];
} SCE_boot_param_info;

typedef struct {
	SCE_controlinfo head;
	uint8_t shared_secret_0[0x10]; /* ex: 0x7E7FD126A7B9614940607EE1BF9DDF5E or full of zeroes */
	uint8_t shared_secret_1[0x10]; /* ex: full of zeroes */
	uint8_t shared_secret_2[0x10]; /* ex: full of zeroes */
	uint8_t shared_secret_3[0x10]; /* ex: full of zeroes */
} SCE_shared_secret_info;


typedef struct {
	uint64_t offset;
	uint64_t length;
	uint64_t compression; // 1 = uncompressed, 2 = compressed
	uint64_t encryption; // 1 = encrypted, 2 = plain
} segment_info;
#pragma pack(pop)

enum {
	HEADER_LEN = 0x1000
};
