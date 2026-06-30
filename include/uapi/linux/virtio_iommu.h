/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note*/
/*
 * Virtio-iommu definition v0.12
 *
 * Copyright (C) 2019 Arm Ltd.
 * Copyright (c) 2023 MediaTek Inc.
 */
#ifndef _UAPI_LINUX_VIRTIO_IOMMU_H
#define _UAPI_LINUX_VIRTIO_IOMMU_H

#include <linux/types.h>

/* Feature bits */
#define VIRTIO_IOMMU_F_INPUT_RANGE		0
#define VIRTIO_IOMMU_F_DOMAIN_RANGE		1
#define VIRTIO_IOMMU_F_MAP_UNMAP		2
#define VIRTIO_IOMMU_F_BYPASS			3
#define VIRTIO_IOMMU_F_PROBE			4
#define VIRTIO_IOMMU_F_MMIO			5

struct virtio_iommu_range_64 {
	__le64					start;
	__le64					end;
};

struct virtio_iommu_range_32 {
	__le32					start;
	__le32					end;
};

struct virtio_iommu_config {
	/* Supported page sizes */
	__le64					page_size_mask;
	/* Supported IOVA range */
	struct virtio_iommu_range_64		input_range;
	/* Max domain ID size */
	struct virtio_iommu_range_32		domain_range;
	/* Probe buffer size */
	__le32					probe_size;
	__u8					bypass;
	__u8					reserved[3];
};

/* Request types */
#define VIRTIO_IOMMU_T_ATTACH			0x01
#define VIRTIO_IOMMU_T_DETACH			0x02
#define VIRTIO_IOMMU_T_MAP			0x03
#define VIRTIO_IOMMU_T_UNMAP			0x04
#define VIRTIO_IOMMU_T_PROBE			0x05
#define VIRTIO_IOMMU_T_MAP_SG			0x06

/* Status types */
#define VIRTIO_IOMMU_S_OK			0x00
#define VIRTIO_IOMMU_S_IOERR			0x01
#define VIRTIO_IOMMU_S_UNSUPP			0x02
#define VIRTIO_IOMMU_S_DEVERR			0x03
#define VIRTIO_IOMMU_S_INVAL			0x04
#define VIRTIO_IOMMU_S_RANGE			0x05
#define VIRTIO_IOMMU_S_NOENT			0x06
#define VIRTIO_IOMMU_S_FAULT			0x07
#define VIRTIO_IOMMU_S_NOMEM			0x08

struct virtio_iommu_req_head {
	__u8					type;
	__u8					reserved[3];
};

struct virtio_iommu_req_tail {
	__u8					status;
	__u8					reserved[3];
};

struct virtio_iommu_req_attach {
	struct virtio_iommu_req_head		head;
	__le32					domain;
	__le32					endpoint;
	__u8					reserved[8];
	struct virtio_iommu_req_tail		tail;
};

struct virtio_iommu_req_detach {
	struct virtio_iommu_req_head		head;
	__le32					domain;
	__le32					endpoint;
	__u8					reserved[8];
	struct virtio_iommu_req_tail		tail;
};

#define VIRTIO_IOMMU_MAP_F_READ			(1 << 0)
#define VIRTIO_IOMMU_MAP_F_WRITE		(1 << 1)
#define VIRTIO_IOMMU_MAP_F_MMIO			(1 << 2)

#define VIRTIO_IOMMU_MAP_F_MASK			(VIRTIO_IOMMU_MAP_F_READ |	\
						 VIRTIO_IOMMU_MAP_F_WRITE |	\
						 VIRTIO_IOMMU_MAP_F_MMIO)

struct virtio_iommu_map_record {
	__le64 virt_start;
	__le64 phys_start;
	__le64 size;
	__le32 flags;
};

struct virtio_iommu_req_map_sg {
	struct virtio_iommu_req_head		head;
	__le32 domain;
	__le32 count;
	__le32 page_size;
	struct virtio_iommu_map_record record[];
};

struct virtio_iommu_req_map {
	struct virtio_iommu_req_head		head;
	__le32					domain;
	__le64					virt_start;
	__le64					virt_end;
	__le64					phys_start;
	__le32					flags;
	struct virtio_iommu_req_tail		tail;
};

struct virtio_iommu_req_unmap {
	struct virtio_iommu_req_head		head;
	__le32					domain;
	__le64					virt_start;
	__le64					virt_end;
	__u8					reserved[4];
	struct virtio_iommu_req_tail		tail;
};

#define VIRTIO_IOMMU_PROBE_T_NONE		0
#define VIRTIO_IOMMU_PROBE_T_RESV_MEM		1
#define VIRTIO_IOMMU_PROBE_T_IOVA_SPACE		2
#define VIRTIO_IOMMU_PROBE_T_DOM_ID		3

#define VIRTIO_IOMMU_PROBE_T_MASK		0xfff

struct virtio_iommu_probe_property {
	__le16					type;
	__le16					length;
};

#define VIRTIO_IOMMU_RESV_MEM_T_RESERVED	0
#define VIRTIO_IOMMU_RESV_MEM_T_MSI		1

struct virtio_iommu_probe_resv_mem {
	struct virtio_iommu_probe_property	head;
	__u8					subtype;
	__u8					reserved[3];
	__le64					start;
	__le64					end;
};

struct virtio_iommu_req_probe {
	struct virtio_iommu_req_head		head;
	__le32					endpoint;
	__u8					reserved[64];

	__u8					properties[];

	/*
	 * Tail follows the variable-length properties array. No padding,
	 * property lengths are all aligned on 8 bytes.
	 */
};

/* Fault types */
#define VIRTIO_IOMMU_FAULT_R_UNKNOWN		0
#define VIRTIO_IOMMU_FAULT_R_DOMAIN		1
#define VIRTIO_IOMMU_FAULT_R_MAPPING		2

#define VIRTIO_IOMMU_FAULT_F_READ		(1 << 0)
#define VIRTIO_IOMMU_FAULT_F_WRITE		(1 << 1)
#define VIRTIO_IOMMU_FAULT_F_EXEC		(1 << 2)
#define VIRTIO_IOMMU_FAULT_F_ADDRESS		(1 << 8)

struct virtio_iommu_fault {
	__u8					reason;
	__u8					reserved[3];
	__le32					flags;
	__le32					endpoint;
	__u8					reserved2[4];
	__le64					address;
};

struct virtio_iommu_probe_iova_space {
	struct virtio_iommu_probe_property	head;
	__u8					reserved[4];
	__u8 force;
	__le64 start;
	__le64 end;
};

struct virtio_iommu_probe_domain_id {
	struct virtio_iommu_probe_property head;
	uint32_t dom_id;
};


struct viommu_group {
	struct list_head	list;
	struct iommu_group	*group;
	u32	*sids;
	u32	num_sids;

};

struct virtio_iommu_pal_ops {
	int (*get_iova_space)(uint32_t endpoint, struct iommu_domain_geometry *geometry);
	int (*get_reserved_mems)(uint32_t endpoint, struct list_head *regions);
	int (*handle_iommu_attach)(struct virtio_iommu_req_attach *req);
	__s8 (*handle_iommu_map)(struct virtio_iommu_req_map *req);
	__s8 (*handle_iommu_map_sg)(struct virtio_iommu_req_map_sg *req);
	__s8 (*handle_iommu_unmap)(struct virtio_iommu_req_unmap *req);
	int (*get_iommu_domain_id)(uint32_t endpoint, uint32_t *domain_id);
};

void virtio_iommu_dump_iova_space(unsigned long target);

#endif
