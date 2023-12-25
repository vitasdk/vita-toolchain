
#ifndef _VITA_STUB_GEN_2_DEFS_H_
#define _VITA_STUB_GEN_2_DEFS_H_

#ifdef __cplusplus
extern "C" {
#endif


/*
 * weak is set to 8 for compatibility with previous vita-libs-gen.
 * When this value is changed due to optimization, it is when that compatibility is broken.
 */
#define VITA_STUB_GEN_2_FLAG_WEAK      (0x8)
#define VITA_STUB_GEN_2_FLAG_IS_KERNEL (0x10)


#ifdef __cplusplus
}
#endif

#endif /* _VITA_STUB_GEN_2_DEFS_H_ */
