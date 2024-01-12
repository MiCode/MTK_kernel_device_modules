/*
 * Copyright (C) 2022-2023, Xiaomi, Inc.
 * All Rights Reserved.
 */

#ifndef FP_SPI_H
#define FP_SPI_H
/**********************MTK SPI**********************/
#ifndef FP_USE_SPI
#define FP_USE_SPI "spi1"
#define FP_USE_SPI_LENGTH 4
#endif

struct mtk_spi *fingerprint_ms;
/**********************MTK SPI**********************/
#endif