/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Copyright 2018 The Hafnium Authors.
 */

#pragma once

#include <linux/kernel.h>
#include <linux/types.h>

int memcmp(const void *a, const void *b, size_t n);
int strncmp(const char *a, const char *b, size_t n);

#define ctz(x) __builtin_ctz(x)

/* Compatibility with old compilers */
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

/**
 * Check whether the value `v` is aligned to the boundary `a`,
 * with `a` power of 2.
 */
#if __has_builtin(__builtin_is_aligned)
#define is_aligned(v, a) __builtin_is_aligned((v), (a))
#else
#define is_aligned(v, a) (((uintptr_t)(v) & ((a) - 1)) == 0)
#endif

/**
 * Align up the value `v` to the boundary `a`, with `a` power of 2.
 */
#if __has_builtin(__builtin_align_up)
#define align_up(v, a) __builtin_align_up((v), (a))
#else
#define align_up(v, a) (((uintptr_t)(v) + ((a) - 1)) & ~((a) - 1))
#endif

/**
 * Align down the value `v` to the boundary `a`, with `a` power of 2.
 */
#if __has_builtin(__builtin_align_down)
#define align_down(v, a) __builtin_align_down((v), (a))
#else
#define align_down(v, a) ((uintptr_t)(v) & ~((a) - 1))
#endif

#ifndef be16toh
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define be16toh(v) __builtin_bswap16(v)
#define be32toh(v) __builtin_bswap32(v)
#define be64toh(v) __builtin_bswap64(v)

#define htobe16(v) __builtin_bswap16(v)
#define htobe32(v) __builtin_bswap32(v)
#define htobe64(v) __builtin_bswap64(v)

#define le16toh(v) (v)
#define le32toh(v) (v)
#define le64toh(v) (v)

#define htole16(v) (v)
#define htole32(v) (v)
#define htole64(v) (v)

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define be16toh(v) (v)
#define be32toh(v) (v)
#define be64toh(v) (v)

#define htobe16(v) (v)
#define htobe32(v) (v)
#define htobe64(v) (v)

#define le16toh(v) __builtin_bswap16(v)
#define le32toh(v) __builtin_bswap32(v)
#define le64toh(v) __builtin_bswap64(v)

#define htole16(v) __builtin_bswap16(v)
#define htole32(v) __builtin_bswap32(v)
#define htole64(v) __builtin_bswap64(v)

#else

/*
 * __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ &&
 * __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
 */

#error "Unsupported byte order"

#endif
#endif
