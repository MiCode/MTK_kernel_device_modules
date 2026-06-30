// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#ifndef AUDIO_IPI_CLIENT_ULTRA_H
#define AUDIO_IPI_CLIENT_ULTRA_H

void ultra_dump_register_file(void);
void ultra_dump_init(void);
void ultra_dump_deinit(void);
void ultra_dump_message(void *msg_data);
int ultra_start_dump(void);
void ultra_stop_dump(void);

#endif /* end of AUDIO_IPI_CLIENT_ULTRA_H */

