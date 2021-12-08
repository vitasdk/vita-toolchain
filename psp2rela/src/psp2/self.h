
#ifndef _PSP2_SCE_SELF_TYPE_H_
#define _PSP2_SCE_SELF_TYPE_H_

#include "types.h"
#include <inttypes.h>

// some info taken from the wiki, see http://vitadevwiki.com/index.php?title=SELF_File_Format

typedef struct cf_header {
	uint32_t m_magic;
	uint32_t m_version;
	struct {
		uint8_t m_platform;
		uint8_t m_sdk_type;
	} attributes;
	uint16_t m_category;
	uint32_t m_ext_header_size;

	union {
		uint64_t m_header_length;
		uint64_t m_file_offset;
	};
	uint64_t m_file_size;

	uint64_t m_certified_file_size;
	uint64_t m_padding;
} __attribute__((packed)) cf_header;

typedef struct ext_header {
	// 0x00
	uint64_t self_offset;           /* SELF offset           */
	uint64_t appinfo_offset;        /* app info offset       */

	// 0x10
	uint64_t elf_offset;            /* ELF #1 offset         */
	uint64_t phdr_offset;           /* program header offset */

	// 0x20
	uint64_t shdr_offset;           /* section header offset */
	uint64_t section_info_offset;   /* section info offset   */

	// 0x30
	uint64_t sceversion_offset;     /* version offset        */
	uint64_t controlinfo_offset;    /* control info offset   */

	// 0x40
	uint64_t controlinfo_size;      /* control info size     */
	uint64_t padding;
} ext_header;

typedef struct cf_header_v2 {
	uint32_t m_magic;
	uint32_t m_version;
	struct {
		uint8_t m_platform;
		uint8_t m_sdk_type;
	} attributes;
	uint16_t m_category;
	uint32_t m_ext_header_size;

	union {
		uint64_t m_header_length;
		uint64_t m_file_offset;
	};
	uint64_t m_file_size;
} __attribute__((packed)) cf_header_v2;

typedef struct cf_header_v3 {
	uint32_t m_magic;
	uint32_t m_version;
	struct {
		uint8_t m_platform;
		uint8_t m_sdk_type;
	} attributes;
	uint16_t m_category;
	uint32_t m_ext_header_size;

	union {
		uint64_t m_header_length;
		uint64_t m_file_offset;
	};
	uint64_t m_file_size;

	uint64_t m_certified_file_size;
	uint64_t m_padding;
} __attribute__((packed)) cf_header_v3;

typedef union cf_header_t {
	struct {
		uint32_t m_magic;
		uint32_t m_version;
	} base;
	cf_header_v2 header_v2;
	cf_header_v3 header_v3;
} cf_header_t;

typedef struct {
	uint32_t magic;                 /* 53434500 = SCE\0 */
	uint32_t version;               /* header version 3*/
	uint8_t platform;               /* */
	uint8_t sdk_type;               /* */
	uint16_t header_type;           /* SceType, 1 self, 2 unknown, 3 pkg, 6 spsfo */
	uint32_t metadata_offset;       /* metadata offset */
	// 0x10
	uint64_t header_len;            /* self header length */
	uint64_t elf_filesize;          /* ELF file length */
	// 0x20
	uint64_t self_filesize;         /* SELF file length */
	uint64_t unknown;               /* UNKNOWN */
	// 0x30
	uint64_t self_offset;           /* SELF offset */
	uint64_t appinfo_offset;        /* app info offset */
	// 0x40
	uint64_t elf_offset;            /* ELF #1 offset */
	uint64_t phdr_offset;           /* program header offset */
	// 0x50
	uint64_t shdr_offset;           /* section header offset */
	uint64_t section_info_offset;   /* section info offset */
	// 0x60
	uint64_t sceversion_offset;     /* version offset */
	uint64_t controlinfo_offset;    /* control info offset */
	uint64_t controlinfo_size;      /* control info size */
	uint64_t padding;
} SCE_header;

typedef struct {
	uint64_t authid;                /* auth id */
	uint32_t vendor_id;             /* vendor id */
	uint32_t self_type;             /* SceSelfType */
	uint64_t version;               /* app version */
	uint64_t padding;               /* UNKNOWN */
} SCE_appinfo;

typedef struct {
	uint32_t unk1; // ex:1
	uint32_t unk2;
	uint32_t unk3; // ex:0x10
	uint32_t unk4;
} SCE_version;

typedef struct {
	uint32_t type; // 4==PSVita ELF digest info; 5==PSVita NPDRM info; 6==PSVita boot param info; 7==PSVita shared secret info
	uint32_t size;
	uint64_t next; // 1 if another Control Info structure follows else 0
	union {
		// type 4, 0x50 bytes
		struct  { // 0x40 bytes of data
			uint8_t constant[0x14]; // same for every PSVita/PS3 SELF, hardcoded in make_fself.exe: 627CB1808AB938E32C8C091708726A579E2586E4
			uint8_t elf_digest[0x20]; // on PSVita: SHA-256 of source ELF file, on PS3: SHA-1
			uint32_t padding;
			uint64_t min_required_fw; // ex: 0x363 for 3.63
		} PSVita_elf_digest_info;
		// type 5, 0x110 bytes
		struct { // 0x80 bytes of data
			uint32_t magic;               // 7F 44 52 4D (".DRM")
			uint32_t finalized_flag;      // ex: 80 00 00 01
			uint32_t drm_type;            // license_type ex: 2 local, 0XD free with license
			uint32_t padding;
			uint8_t content_id[0x30];
			uint8_t digest[0x10];         // ?sha-1 hash of debug self/sprx created using make_fself_npdrm?
			uint8_t padding_78[0x78];
			uint8_t hash_signature[0x38]; // unknown hash/signature
		} PSVita_npdrm_info;
		// type 6, 0x110 bytes
		struct { // 0x100 bytes of data
			int is_enable;
			int attribute;
			SceSize use_memblk_num_for_phycont; // total phycont mem size = (use_memblk_num_for_phycont * 0x400)
			SceSize use_memblk_num_for_app;     // total     app mem size = (use_memblk_num_for_app - 0x1000) * 0x400
			SceSize file_open_max_num;
			SceSize dir_open_max_level;
			SceSize mount_max_num_for_encrypt;
			SceSize mount_max_num_for_redirect;
			uint8_t rsvd[0xE0];
		} SceBootparam;
		// type 7, 0x50 bytes
		struct { // 0x40 bytes of data
			/*
			 * ex
			 * 01. F9 C5 23 5F FD E6 21 EA 2D F3 76 D2 77 6E 84 7C
			 * 02. 7E 7F D1 26 A7 B9 61 49 40 60 7E E1 BF 9D DF 5E
			 * 03. 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
			 */
			uint8_t shared_secret_0[0x10];
			uint8_t shared_secret_1[0x10]; // ex: full of zeroes
			uint8_t shared_secret_2[0x10]; // ex: full of zeroes
			uint8_t shared_secret_3[0x10]; // ex: full of zeroes
		} PSVita_shared_secret_info;
	};
} __attribute__((packed)) PSVita_CONTROL_INFO;

typedef struct {
	uint64_t offset;
	uint64_t length;
	uint64_t compression; // 1 = uncompressed, 2 = compressed
	uint64_t encryption;  // 1 = encrypted,    2 = plain
} segment_info;

enum {
	HEADER_LEN = 0x1000,
	SCE_MAGIC  = 0x454353
};

#endif /* _PSP2_SCE_SELF_TYPE_H_ */
