/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _RELA_DEBUG_H_
#define _RELA_DEBUG_H_

typedef enum RelaDbgLogLevel {
	RELA_DBG_LOG_LEVEL_TRACE = 0,
	RELA_DBG_LOG_LEVEL_DEBUG,
	RELA_DBG_LOG_LEVEL_INFO,
	RELA_DBG_LOG_LEVEL_WARNING,
	RELA_DBG_LOG_LEVEL_ERROR,
	RELA_DBG_LOG_LEVEL_NONE,
	RELA_DBG_NUM_LOG_LEVELS
} RelaDbgLogLevel;

int rela_debug_init(const char *args, const char *path);
int rela_debug_fini(void);

int rela_is_show_mode(void);

int printf_t(const char *fmt, ...);
int printf_d(const char *fmt, ...);
int printf_i(const char *fmt, ...);
int printf_w(const char *fmt, ...);
int printf_e(const char *fmt, ...);

int log_open(const char *path);
int log_close(void);
int log_write(const char *fmt, ...);

#endif /* _RELA_DEBUG_H_ */
