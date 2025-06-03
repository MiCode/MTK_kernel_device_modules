/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_H__
#define __MTK_APU_MDW_H__

#include <linux/miscdevice.h>
#include <linux/iopoll.h>
#include <linux/irqreturn.h>
#include <linux/dma-fence.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/genalloc.h>
#include <linux/platform_device.h>

#include "apusys_core.h"
#include "apusys_device.h"
#include "mdw_ioctl.h"
#include "mdw_import.h"
#include "mdw_trace.h"
#include "apu_sysmem.h"

#define MDW_NAME "apusys"
#define MDW_DEV_MAX (APUSYS_DEVICE_MAX)
#define MDW_DEV_TAB_DEV_MAX (16)
#define MDW_CMD_MAX (32)
#define MDW_SUBCMD_MAX (64)
#define MDW_PRIORITY_MAX (32)
#define MDW_DEFAULT_TIMEOUT_MS (30*1000)
#define MDW_CMD_TIMEOUT_MS (5*1000)
#define MDW_BOOST_MAX (100)
#define MDW_DEFAULT_ALIGN (128)
#define MDW_UTIL_CMD_MAX_SIZE (1*1024*1024)

#define MDW_FENCE_MAX_RINGS (64)

#define MDW_ALIGN(x, align) ((x+align-1) & (~(align-1)))
#define MDW_IS_HIGHADDR(addr) ((addr & 0xffffffff00000000) ? true : false)

/* history parameter */
#define MDW_NUM_HISTORY 2
#define MDW_NUM_PREDICT_CMD 16
#define MDW_POWER_GAIN_TH 7680 //us
#define MDW_PERIOD_TOLERANCE_TH(x) (x*10/100) //ms
#define MDW_IPTIME_TOLERANCE_TH(x) (x*10/100) //ms
#define MDW_EXECTIME_TOLERANCE_TH(x) (x*50/100) //us

/* power budget debounce time */
#define MDW_PB_DEBOUNCE_MS (50*1000)

/* dtime */
#define MAX_DTIME (2000) /* 2s */

struct mdw_fpriv;
struct mdw_device;
struct mdw_mem;

extern const struct mdw_plat_func ap_plat_drv_v1;
//extern const struct mdw_plat_func rv_plat_drv_v2;
//extern const struct mdw_plat_func rv_plat_drv_v3;
//extern const struct mdw_plat_func rv_plat_drv_v4;
//extern const struct mdw_plat_func rv_plat_drv_v6;

enum mdw_cmd_state {
	MDW_CMD_STATE_IDLE,
	MDW_CMD_STATE_RUN,
	MDW_CMD_STATE_POSTPROCESS_DONE,
	MDW_CMD_STATE_ERROR,

	MDW_CMD_STATE_MAX,
};

enum mdw_power_type {
	MDW_APU_POWER_OFF,
	MDW_APU_POWER_ON,
};

enum mdw_buf_type {
	MDW_BUF_TYPE_DATA,
	MDW_BUF_TYPE_CMD,
};

enum mdw_mem_op {
	MDW_MEM_OP_NONE,
	MDW_MEM_OP_INTERNAL,
	MDW_MEM_OP_ALLOC,
	MDW_MEM_OP_IMPORT,
};

struct mdw_mem_map {
	struct mdw_fpriv *mpriv;
	uint64_t flags;
	struct dma_buf *dbuf;
	enum mdw_mem_type mem_type;
	enum mdw_buf_type buf_type;

	struct apu_sysmem_map *sysmap; //from apu_sysmem
	uint64_t device_va; //info from sysmap
	uint64_t size; //info from sysmap
	void *vaddr; //info from sysmap

	struct list_head pool_node; //to mdw_pool
	struct list_head device_node; //to mdw_device
	struct hlist_node fpriv_node; //to mdw_fpriv
	struct mdw_mem_pool *pool; //for pool operation

	struct kref ref;
	void (*get)(struct mdw_mem_map *map);
	void (*put)(struct mdw_mem_map *map);
};

enum mdw_queue_type {
	MDW_QUEUE_COMMON,
	MDW_QUEUE_NORMAL,
	MDW_QUEUE_DEADLINE,

	MDW_QUEUE_MAX,
};

struct mdw_mem {
	struct mdw_fpriv *mpriv;
	enum mdw_mem_type mem_type;
	uint64_t size;
	uint64_t align;
	uint64_t flags;

	struct dma_buf *dbuf;
	void *vaddr;
	uint64_t device_va;
	uint64_t dva_size;

	/* control */
	struct apu_sysmem_buffer *sysbuf;
	struct hlist_node fpriv_node; //to mdw_fpriv
	struct hlist_node device_node; //to mdw_device

	struct kref ref;
	void (*get)(struct mdw_mem *m);
	void (*put)(struct mdw_mem *m);
};

/* default chunk size of memory pool */
#define MDW_MEM_POOL_CHUNK_SIZE (4*1024*1024)

struct mdw_mem_pool_chunk {
	struct mdw_fpriv *mpriv;
	struct mdw_mem *mem;
	struct mdw_mem_map *map;
	struct list_head pool_node;
};

struct mdw_mem_pool {
	struct mdw_fpriv *mpriv;
	/* pool attribute */
	enum mdw_mem_type mem_type;
	enum mdw_buf_type buf_type;
	uint64_t flags;
	uint32_t align;
	uint32_t chunk_size;
	/* container and lock */
	struct gen_pool *gp;
	struct mutex m_mtx;
	/* list of resource chunks */
	struct list_head m_chunks; //for mdw_mem_pool_chunk
	/* list of allocated memories from gp */
	struct list_head m_list; //for mdw_mem_map
	/* ref count for cmd/mem */
	struct kref m_ref;
	void (*get)(struct mdw_mem_pool *pool);
	void (*put)(struct mdw_mem_pool *pool);
};

struct mdw_dinfo {
	uint32_t type;
	uint32_t num;
	uint8_t meta[MDW_DEV_META_SIZE];
};

enum mdw_driver_type {
	MDW_DRIVER_TYPE_PLATFORM,
	MDW_DRIVER_TYPE_RPMSG,
};

enum mdw_info_type {
	MDW_INFO_KLOG,
	MDW_INFO_ULOG,
	MDW_INFO_PREEMPT_POLICY,
	MDW_INFO_SCHED_POLICY,

	MDW_INFO_NORMAL_TASK_DLA,
	MDW_INFO_NORMAL_TASK_DSP,
	MDW_INFO_NORMAL_TASK_DMA,

	MDW_INFO_MIN_DTIME,
	MDW_INFO_MIN_ETIME,
	MDW_INFO_MAX_DTIME,

	MDW_INFO_RESERV_TIME_REMAIN,

	MDW_INFO_DPLCY_MODE,
	MDW_INFO_DPLCY_TIME_MS,

	MDW_INFO_MAX,
};

enum mdw_pwrplcy_type {
	MDW_POWERPOLICY_DEFAULT = 0, //do nothing
	MDW_POWERPOLICY_SUSTAINABLE = 1,
	MDW_POWERPOLICY_PERFORMANCE = 2,
	MDW_POWERPOLICY_POWERSAVING = 3,
};

enum {
	MDW_APPTYPE_DEFAULT = 0,
	MDW_APPTYPE_ONESHOT = 1,
	MDW_APPTYPE_STREAMING = 2,
};

struct mdw_device {
	enum mdw_driver_type driver_type;
	union {
		struct platform_device *pdev;
		struct rpmsg_device *rpdev;
	};
	struct miscdevice *misc_dev;

	/* init flag */
	bool inited;

	/* allocator */
	struct apu_sysmem_allocator *allocator;

	/* cores enable bitmask */
	uint32_t dsp_mask;
	uint32_t dla_mask;
	uint32_t dma_mask;

	/* mdw version */
	uint32_t mdw_ver;

	/* device */
	struct apusys_device *adevs[MDW_DEV_MAX];

	/* device support information */
	unsigned long dev_mask[BITS_TO_LONGS(MDW_DEV_MAX)];
	struct mdw_dinfo *dinfos[MDW_DEV_MAX];
	/* memory support information */
	unsigned long mem_mask[BITS_TO_LONGS(MDW_MEM_TYPE_MAX)];
	struct mdw_mem_map minfos[MDW_MEM_TYPE_MAX];

	/* memory list */
	DECLARE_HASHTABLE(u_mem_hash, 4); //hash(dbuf, mdw_mem)
	//struct mutex hash_mtx;
	struct list_head maps;
	struct mutex mctl_mtx;

	/* cmd clear wq */
	struct mutex c_mtx;
	struct list_head d_cmds;
	struct work_struct c_wk;

	/* platform functions */
	const struct mdw_plat_func *plat_funcs;
	void *dev_specific;

	/* fence info */
	uint64_t base_fence_ctx;
	uint32_t num_fence_ctx;
	unsigned long fence_ctx_mask[BITS_TO_LONGS(MDW_FENCE_MAX_RINGS)];
	struct mutex f_mtx;

	/* cmd history */
	atomic_t cmd_running;

	/* power fast on/off */
	bool support_power_fast_on_off;
	enum mdw_power_type power_state;
	uint64_t max_dtime_ts;
	struct mutex dtime_mtx;
	struct mutex power_mtx;
	uint32_t power_gain_time_us;

	atomic_t pwr_usage;
	atomic_t ipi_usage;
};

struct mdw_fpriv {
	struct mdw_device *mdev;
	struct device *dev; //mdw_dev->misc_dev->this_device

	DECLARE_HASHTABLE(u_mem_hash, 4); //hash(dbuf, mdw_mem)
	DECLARE_HASHTABLE(u_map_hash, 4); //hash(dbuf, mdw_mem_map)
	struct mutex mtx;
	struct mdw_mem_pool cmd_buf_pool;
	struct idr cmds;
	atomic_t exec_seqno;

	struct apu_sysmem_allocator *mem_allocator;

	/* ref count for cmd/mem */
	struct kref ref;
	void (*get_ref)(struct mdw_fpriv *mpriv);
	void (*put_ref)(struct mdw_fpriv *mpriv);

	/* cmd execute id counter */
	uint32_t counter;
};

struct mdw_exec_info {
	struct mdw_cmd_exec_info c;
	struct mdw_subcmd_exec_info sc;
};

struct mdw_subcmd_kinfo {
	struct mdw_subcmd_info *info; //c->subcmds
	struct mdw_subcmd_cmdbuf *cmdbufs; //from usr
	struct mdw_mem **ori_cbs; //pointer to original cmdbuf
	struct mdw_subcmd_exec_info *sc_einfo;
	uint64_t *kvaddrs; //pointer to duplicated buf
	uint64_t *daddrs; //pointer to duplicated buf
	void *priv; //mdw_ap_sc
};

struct mdw_fence {
	struct dma_fence base_fence;
	struct mdw_device *mdev;
	spinlock_t lock;
	char name[32];
};

struct mdw_cmd_map_invoke {
	struct list_head c_node;
	struct mdw_mem_map *map;
};

struct mdw_cmd {
	pid_t pid;
	pid_t tgid;
	char comm[16];
	uint64_t kid;
	uint64_t uid;
	uint64_t rvid;
	uint32_t u_pid;
	uint32_t priority;
	uint32_t hardlimit;
	uint32_t softlimit;
	uint32_t power_save;
	uint32_t power_plcy;
	uint32_t power_dtime;
	uint32_t power_etime;
	uint32_t fastmem_ms;
	uint32_t app_type;
	uint32_t hse_num;
	uint64_t function_bitmask;
	uint32_t num_subcmds;
	uint32_t num_links;
	struct mdw_subcmd_info *subcmds; //from usr
	struct mdw_subcmd_kinfo *ksubcmds;
	uint32_t num_cmdbufs;
	uint32_t size_cmdbufs;
	struct mdw_mem_map *cmdbufs;
	struct mdw_mem *exec_infos;
	struct mdw_exec_info *einfos;
	uint8_t *adj_matrix;
	struct mdw_subcmd_link_v1 *links;

	/* exec order */
	uint32_t *execute_orders;
	uint32_t *predecessors;
	uint32_t predecessors_num;
	uint32_t predecessors_size; //predecessors_num * uint32_t
	uint32_t *pack_friends;
	uint32_t pack_friends_num;
	uint32_t pack_friends_size; //pack_friends_num * uint32_t
	uint32_t *end_vertices;
	uint32_t end_vertices_num;
	uint32_t end_vertices_size;

	struct mutex mtx;
	struct list_head map_invokes; //for mdw_cmd_map_invoke
	struct mutex cm_mtx;  // for map_invokes
	struct list_head d_node; //mdev->d_cmds for delete async

	int id;
	struct kref ref;

	uint64_t start_ts;
	uint64_t end_ts;

	struct mdw_fpriv *mpriv;
	void *plat_priv;
	int (*complete)(struct mdw_cmd *c, int ret);
	void (*get_ref)(struct mdw_cmd *c);
	void (*put_ref)(struct mdw_cmd *c);

	struct mdw_fence *fence;
	struct work_struct t_wk; //for fence wait trigger
	struct dma_fence *wait_fence;

	/* history params */
	uint32_t inference_ms;
	uint32_t tolerance_ms;
	/* set dtime */
	uint64_t is_dtime_set;
	/* polling cmd result */

	/* ext operation */
	uint64_t ext_id; // for apuext unique id

	/* cmd poll */
	uint32_t cmd_state;
	//struct completion cmplt;
	struct semaphore exec_sem;

	/* cmd exec id */
	uint64_t inference_id;

	/* cmd tag time */
	uint64_t enter_complt_time;
	uint64_t pb_put_time;
	uint64_t cmdbuf_out_time;
	uint64_t handle_cmd_result_time;
	uint64_t load_aware_pwroff_time;
	uint64_t enter_mpriv_release_time;
	uint64_t mpriv_release_time;
	uint64_t enter_rv_cb_time;
	uint64_t rv_cb_time;

	/* auto dvfs target time (ms) */
	uint64_t auto_dvfs_target_time;
};

struct mdw_plat_func {
	int (*late_init)(struct mdw_device *mdev);
	void (*late_deinit)(struct mdw_device *mdev);
	int (*sw_init)(struct mdw_device *mdev);
	void (*sw_deinit)(struct mdw_device *mdev);
	int (*set_power)(struct mdw_device *mdev, uint32_t type, uint32_t idx, uint32_t boost);
	int (*ucmd)(struct mdw_device *mdev, uint32_t type, void *vaddr, uint32_t size);
	int (*set_param)(struct mdw_device *mdev, enum mdw_info_type type, uint32_t val);
	uint32_t (*get_info)(struct mdw_device *mdev, enum mdw_info_type type);
	int (*register_device)(struct apusys_device *adev);
	int (*unregister_device)(struct apusys_device *adev);

	int (*create_session)(struct mdw_fpriv *mpriv);
	int (*delete_session)(struct mdw_fpriv *mpriv);
	int (*create_cmd_priv)(struct mdw_cmd *c); // call after mdw_cmd create
	int (*delete_cmd_priv)(struct mdw_cmd *c); // call before mdw_cmd delete
	int (*run_cmd)(struct mdw_cmd *c);
	int (*preprocess_cmd)(struct mdw_cmd *c); // call before cmd done
	int (*postprocess_cmd)(struct mdw_cmd *c); // call after cmd done
	int (*late_postprocess_cmd)(struct mdw_cmd *c); // call after signal to user
	int (*check_sc_rets)(struct mdw_cmd *c, int ipi_ret); // call for check subcmd rets
	int (*cmd_sanity_check)(struct mdw_cmd *c); // call for cmd sanity check
};

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#define _mdw_exception(key, reason) \
	do { \
		char info[150];\
		mdw_drv_err(reason); \
		if (snprintf(info, 150, "apu_mdw:" reason) > 0) { \
			aee_kernel_exception(info, \
				"\nCRDISPATCH_KEY:%s\n", key); \
		} else { \
			mdw_drv_err("apu_mdw: %s snprintf fail(%d)\n", __func__, __LINE__); \
		} \
	} while (0)
#define mdw_exception(reason) _mdw_exception("APUSYS_MIDDLEWARE", reason)
#define dma_exception(reason) _mdw_exception("APUSYS_EDMA", reason)
#define aps_exception(reason) _mdw_exception("APUSYS_APS", reason)
#define mdw_rv_exception(reason) _mdw_exception("APUSYS_RV_MIDDLEWARE", reason)
#else
#define mdw_exception(reason) { (void)(reason); }
#define dma_exception(reason) { (void)(reason); }
#define aps_exception(reason) { (void)(reason); }
#define mdw_rv_exception(reason) { (void)(reason); }
#endif

long mdw_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
int mdw_hs_ioctl(struct mdw_fpriv *mpriv, void *data);
int mdw_cmd_ioctl(struct mdw_fpriv *mpriv, void *kdata);
int mdw_mem_ioctl(struct mdw_fpriv *mpriv, void *data);
int mdw_util_ioctl(struct mdw_fpriv *mpriv, void *data);

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, enum mdw_mem_type mem_type,
	uint64_t size, uint32_t align, uint64_t flags, char *name);
int mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *m);
struct mdw_mem_map *mdw_mem_create_map(struct mdw_fpriv *mpriv, struct dma_buf *dbuf,
	enum mdw_buf_type buf_type, uint64_t flags, bool share_region);
int mdw_mem_delete_map(struct mdw_fpriv *mpriv, struct mdw_mem_map *map);
int mdw_mem_flush(struct mdw_fpriv *mpriv, struct mdw_mem_map *map);
int mdw_mem_invalidate(struct mdw_fpriv *mpriv, struct mdw_mem_map *map);
int mdw_mem_init(struct mdw_device *mdev);
void mdw_mem_deinit(struct mdw_device *mdev);
void mdw_mem_release_session(struct mdw_fpriv *mpriv);
struct mdw_mem *mdw_mem_get_mem_by_handle(struct mdw_fpriv *mpriv, int handle);

int mdw_sysfs_init(struct mdw_device *mdev);
void mdw_sysfs_deinit(struct mdw_device *mdev);

int mdw_dbg_init(struct apusys_core_info *info);
void mdw_dbg_deinit(void);

int mdw_dev_init(struct device *dev, struct mdw_device *mdev);
void mdw_dev_deinit(struct mdw_device *mdev);
void mdw_dev_session_create(struct mdw_fpriv *mpriv);
void mdw_dev_session_delete(struct mdw_fpriv *mpriv);
int mdw_dev_validation(struct mdw_fpriv *mpriv, uint32_t dtype,
	struct mdw_cmd *cmd, struct apusys_cmdbuf *cbs, uint32_t num);

#endif
