/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _LINUX_VIRTIO_IDS_H
#define _LINUX_VIRTIO_IDS_H

#define PCI_SUBVENDOR_ID_MTK		0xf0ff /* Extend virtio dev IDs for co-branch */
/*
 * Virtio IDs
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */

#define VIRTIO_ID_NET			1 /* virtio net */
#define VIRTIO_ID_BLOCK			2 /* virtio block */
#define VIRTIO_ID_CONSOLE		3 /* virtio console */
#define VIRTIO_ID_RNG			4 /* virtio rng */
#define VIRTIO_ID_BALLOON		5 /* virtio balloon */
#define VIRTIO_ID_IOMEM			6 /* virtio ioMemory */
#define VIRTIO_ID_RPMSG			7 /* virtio remote processor messaging */
#define VIRTIO_ID_SCSI			8 /* virtio scsi */
#define VIRTIO_ID_9P			9 /* 9p virtio console */
#define VIRTIO_ID_MAC80211_WLAN		10 /* virtio WLAN MAC */
#define VIRTIO_ID_RPROC_SERIAL		11 /* virtio remoteproc serial link */
#define VIRTIO_ID_CAIF			12 /* Virtio caif */
//#define VIRTIO_ID_MEMORY_BALLOON	13 /* virtio memory balloon */
#define VIRTIO_ID_TRUSTY_IPC   13 /* virtio trusty ipc */
#define VIRTIO_ID_GPU			16 /* virtio GPU */
#define VIRTIO_ID_CLOCK			17 /* virtio clock/timer */
#define VIRTIO_ID_INPUT			18 /* virtio input */
#define VIRTIO_ID_VSOCK			19 /* virtio vsock transport */
#define VIRTIO_ID_CRYPTO		20 /* virtio crypto */
#define VIRTIO_ID_SIGNAL_DIST		21 /* virtio signal distribution device */
#define VIRTIO_ID_PSTORE		22 /* virtio pstore device */
#define VIRTIO_ID_IOMMU			23 /* virtio IOMMU */
#define VIRTIO_ID_MEM			24 /* virtio mem */
#define VIRTIO_ID_SOUND			25 /* virtio sound */
#define VIRTIO_ID_FS			26 /* virtio filesystem */
#define VIRTIO_ID_PMEM			27 /* virtio pmem */
#define VIRTIO_ID_RPMB			28 /* virtio rpmb */
#define VIRTIO_ID_MAC80211_HWSIM	29 /* virtio mac80211-hwsim */
#define VIRTIO_ID_VIDEO_ENCODER		30 /* virtio video encoder */
#define VIRTIO_ID_VIDEO_DECODER		31 /* virtio video decoder */
#define VIRTIO_ID_SCMI			32 /* virtio SCMI */
#define VIRTIO_ID_NITRO_SEC_MOD		33 /* virtio nitro secure module*/
#define VIRTIO_ID_I2C_ADAPTER		34 /* virtio i2c adapter */
#define VIRTIO_ID_WATCHDOG		35 /* virtio watchdog */
#define VIRTIO_ID_CAN			36 /* virtio can */
#define VIRTIO_ID_DMABUF		37 /* virtio dmabuf */
#define VIRTIO_ID_PARAM_SERV		38 /* virtio parameter server */
#define VIRTIO_ID_AUDIO_POLICY		39 /* virtio audio policy */
#define VIRTIO_ID_BT			40 /* virtio bluetooth */
#define VIRTIO_ID_GPIO			41 /* virtio gpio */

#define VIRTIO_ID_CUST_START    50
#define VIRTIO_ID_UFS_RPMB      56 /* virtio ufs rpmb */
#define VIRTIO_ID_VMCTL         60

#define VIRTIO_TRANS_ID_MTK_PROPRIETARY        0x103f
#define VIRTIO_SUBSYSTEM_ID_MTK_EINT           92
#define VIRTIO_SUBSYSTEM_ID_MTK_ISM            93 /* virtio ism */
#define VIRTIO_SUBSYSTEM_ID_MTK_SPI            94 /* virtio spi */
#define VIRTIO_SUBSYSTEM_ID_MTK_SENSOR         95 /* virtio sensor */
#define VIRTIO_SUBSYSTEM_ID_MTK_VDMABUF        96 /* virtio vdmabuf */
#define VIRTIO_SUBSYSTEM_ID_MTK_SENSOR_T       97 /* virtio sensor_t */
#define VIRTIO_SUBSYSTEM_ID_MTK_LEDS           98 /* virtio leds */
#define VIRTIO_SUBSYSTEM_ID_MTK_HELLO          99 /* virtio hello(sample) */
#define VIRTIO_SUBSYSTEM_ID_MTK_MMDVFS         100 /* virtio for mtk-mmdvfs */
#define VIRTIO_SUBSYSTEM_ID_MTK_DISP           101 /* virtio for mtk-disp */
#define VIRTIO_SUBSYSTEM_ID_MTK_TIMESYNC       102 /* virtio for mtk-timesync */
#define VIRTIO_SUBSYSTEM_ID_MTK_SLBC           103 /* virtio for mtk-slbc */
#define VIRTIO_SUBSYSTEM_ID_MTK_CONNINFRA_TBOX 104 /* virtio conninfra_tbox */
#define VIRTIO_SUBSYSTEM_ID_MTK_SMI            105 /* virtio smi */
#define VIRTIO_SUBSYSTEM_ID_MTK_RTC_ALARM      106 /* virtio rtc transport */
#define VIRTIO_SUBSYSTEM_ID_MTK_RTC_TOUCH      107 /* virtio rtc touch transport */
#define VIRTIO_SUBSYSTEM_ID_MTK_TOUCH          108 /* virtio touch transport */
#define VIRTIO_SUBSYSTEM_ID_MTK_UFS_RPMB       109 /* virtio mbrain transport */
#define VIRTIO_SUBSYSTEM_ID_MTK_MBRAIN         110 /* virtio mbrain transport */
#define VIRTIO_SUBSYSTEM_ID_MTK_APU            111 /* virtio for mtk-apu */
#define VIRTIO_SUBSYSTEM_ID_MTK_ADSP           112 /* virtio adsp */
#define VIRTIO_SUBSYSTEM_ID_MTK_WMT            113 /* virtio mailbox transport */
#define VIRTIO_SUBSYSTEM_ID_MTK_CMDQ           114 /* virtio for mtk-cmdq */
#define VIRTIO_SUBSYSTEM_ID_MTK_MTK            115 /* virtio mailbox transport */
#define VIRTIO_SUBSYSTEM_ID_MTK_NODE           116 /* virtio node */
#define VIRTIO_SUBSYSTEM_ID_MTK_ALLOC          117 /* virtio alloc */

/*
 * Virtio Transitional IDs
 */

#define VIRTIO_TRANS_ID_NET		0x1000 /* transitional virtio net */
#define VIRTIO_TRANS_ID_BLOCK		0x1001 /* transitional virtio block */
#define VIRTIO_TRANS_ID_BALLOON		0x1002 /* transitional virtio balloon */
#define VIRTIO_TRANS_ID_CONSOLE		0x1003 /* transitional virtio console */
#define VIRTIO_TRANS_ID_SCSI		0x1004 /* transitional virtio SCSI */
#define VIRTIO_TRANS_ID_RNG		0x1005 /* transitional virtio rng */
#define VIRTIO_TRANS_ID_9P		0x1009 /* transitional virtio 9p console */

#endif /* _LINUX_VIRTIO_IDS_H */
