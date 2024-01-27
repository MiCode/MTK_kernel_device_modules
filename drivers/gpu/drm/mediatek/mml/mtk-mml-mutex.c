// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-mmp.h"

#define MUTEX_MAX_MOD_REGS	((MML_MAX_COMPONENTS + 31) >> 5)

/* MUTEX register offset */
#define MUTEX_EN(id)		(0x20 + (id) * 0x20)
#define MUTEX_SHADOW_CTRL(id)	(0x24 + (id) * 0x20)
#define MUTEX_MOD(id, offset)	((offset) + (id) * 0x20)
#define MUTEX_SOF(id)		(0x2c + (id) * 0x20)

struct mutex_data {
	/* Count of display mutex HWs */
	u32 mutex_cnt;
	/* Offsets and count of MUTEX_MOD registers per mutex */
	u32 mod_offsets[MUTEX_MAX_MOD_REGS];
	u32 mod_cnt;
};

static const struct mutex_data mt6983_mutex_data = {
	.mutex_cnt = 16,
	.mod_offsets = {0x30, 0x34},
	.mod_cnt = 2,
};

struct mutex_module {
	u32 mutex_id;
	u32 index;
	u32 field;
	bool select:1;
	bool trigger:1;
};

struct mml_mutex {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct mutex_data *data;
	bool ddp_bound;
	atomic_t connect[MML_PIPE_CNT];
	enum mml_mode connected_mode;

	u16 event_pipe0_mml;
	u16 event_pipe1_mml;

	struct mutex_module modules[MML_MAX_COMPONENTS];
};

static inline struct mml_mutex *comp_to_mutex(struct mml_comp *comp)
{
	return container_of(comp, struct mml_mutex, comp);
}

static s32 mutex_enable(struct mml_mutex *mutex, struct cmdq_pkt *pkt,
			const struct mml_topology_path *path, u32 mutex_sof,
			enum mml_mode mode)
{
	const phys_addr_t base_pa = mutex->comp.base_pa;
	s32 mutex_id = -1;
	u32 mutex_mod[MUTEX_MAX_MOD_REGS] = {0};
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mutex_module *mod = &mutex->modules[path->nodes[i].id];

		if (mod->select)
			mutex_id = mod->mutex_id;
		if (mod->trigger)
			mutex_mod[mod->index] |= 1 << mod->field;
	}

	if (unlikely(mml_wrot_bkgd_en) &&
		(mode == MML_MODE_RACING || mode == MML_MODE_MML_DECOUPLE)) {
		u32 comp_id = path->out_engine_ids[0];
		struct mutex_module *mod = &mutex->modules[comp_id];

		memset(mutex_mod, 0, sizeof(mutex_mod));
		if (mod->trigger)
			mutex_mod[mod->index] |= 1 << mod->field;
	}

	/* TODO: get sof group0 shift from dts */
	mutex_mod[1] |= 1 << (24 + path->mux_group);

	if (mutex_id < 0)
		return -EINVAL;

	/* Do config mutex mod only in dc mode.
	 * For direct link mode, config from mml side (which mutex_sof is empty),
	 * since mml flow has correct topology.
	 */
	if (mode != MML_MODE_DIRECT_LINK || !mutex_sof) {
		mml_mmp(mutex_mod, MMPROFILE_FLAG_PULSE, mode, mutex->data->mod_cnt);

		for (i = 0; i < mutex->data->mod_cnt; i++) {
			u32 offset = mutex->data->mod_offsets[i];

			cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_MOD(mutex_id, offset),
				       mutex_mod[i], U32_MAX);

			mml_mmp(mutex_mod, MMPROFILE_FLAG_PULSE, mutex_id, mutex_mod[i]);
		}
	}

	/* Enable mutex for dc mode, which trigger directly.
	 * For DL mode enable mutex from disp addon
	 * (which mutex_sof contains disp signal bit) and wait disp signal.
	 */
	if (mode != MML_MODE_DIRECT_LINK || mutex_sof) {
		cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_SOF(mutex_id), mutex_sof, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_EN(mutex_id), 0x1, U32_MAX);
	}

	mml_mmp(mutex_en, MMPROFILE_FLAG_PULSE, mode, mutex_sof);

	return 0;
}

static s32 mutex_disable(struct mml_mutex *mutex, struct cmdq_pkt *pkt,
			 const struct mml_topology_path *path)
{
	const phys_addr_t base_pa = mutex->comp.base_pa;
	s32 mutex_id = -1;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mutex_module *mod = &mutex->modules[path->nodes[i].id];

		if (mod->select)
			mutex_id = mod->mutex_id;
	}

	if (mutex_id < 0)
		return -EINVAL;

	cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_EN(mutex_id), 0x0, U32_MAX);
	return 0;
}

static s32 mutex_trigger(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	struct mml_mutex *mutex = comp_to_mutex(comp);
	const struct mml_topology_path *path = task->config->path[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	s32 ret;

	/* DL mode config sof only, other modes enable to trigger directly */
	ret = mutex_enable(mutex, pkt, path, 0x0, task->config->info.mode);

	if (task->config->info.mode == MML_MODE_DIRECT_LINK) {
		if (ccfg->pipe == 0) {
			if (task->config->dual) {
				cmdq_pkt_set_event(pkt, mutex->event_pipe0_mml);
				cmdq_pkt_wfe(pkt, mutex->event_pipe1_mml);
			}

			cmdq_pkt_set_event(pkt, mml_ir_get_mml_ready_event(task->config->mml));
			cmdq_pkt_wfe(pkt, mml_ir_get_disp_ready_event(task->config->mml));
		} else {
			cmdq_pkt_set_event(pkt, mutex->event_pipe1_mml);
			cmdq_pkt_wfe(pkt, mutex->event_pipe0_mml);
		}
	}

	return ret;
}

static s32 mutex_disconnect(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_mutex *mutex = comp_to_mutex(comp);

	if (cfg->info.mode == MML_MODE_DIRECT_LINK)
		mutex_disable(mutex, task->pkts[ccfg->pipe], cfg->path[ccfg->pipe]);

	return 0;
}

static const struct mml_comp_config_ops mutex_config_ops = {
	.mutex = mutex_trigger,
	.post = mutex_disconnect,
};

static void mutex_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	struct mml_mutex *mutex = comp_to_mutex(comp);
	u8 i, j;

	mml_err("mutex component %u dump:", comp->id);
	for (i = 0; i < mutex->data->mutex_cnt; i++) {
		u32 en = readl(base + MUTEX_EN(i));
		u32 sof = readl(base + MUTEX_SOF(i));
		u32 mod[MUTEX_MAX_MOD_REGS] = {0};
		bool used = false;

		for (j = 0; j < mutex->data->mod_cnt; j++) {
			u32 offset = mutex->data->mod_offsets[j];

			mod[j] = readl(base + MUTEX_MOD(i, offset));
			if (mod[j])
				used = true;
		}
		if (used) {
			if (en) {
				mml_err("MDP_MUTEX%d_EN %#010x", i, en);
				mml_err("MDP_MUTEX%d_SOF %#010x", i, sof);
			}
			for (j = 0; j < mutex->data->mod_cnt; j++)
				mml_err("MDP_MUTEX%d_MOD%d %#010x",
					i, j, mod[j]);
		}
	}
}

static const struct mml_comp_debug_ops mutex_debug_ops = {
	.dump = &mutex_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_mutex *mutex = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &mutex->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &mutex->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			mutex->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_mutex *mutex = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &mutex->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &mutex->ddp_comp);
		mutex->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static inline struct mml_mutex *ddp_comp_to_mutex(struct mtk_ddp_comp *ddp_comp)
{
	return container_of(ddp_comp, struct mml_mutex, ddp_comp);
}

static u32 get_mutex_sof(struct mml_mutex_ctl *ctl)
{
	u32 sof = 0;

	switch (ctl->sof_src) {
	case DDP_COMPONENT_DSI0:
		sof |= 1;
		break;
	case DDP_COMPONENT_DSI1:
		sof |= 2;
		break;
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
	case DDP_COMPONENT_DP_INTF0:
		sof |= 3;
		break;
	}
	switch (ctl->eof_src) {
	case DDP_COMPONENT_DSI0:
		sof |= 1 << 6;
		break;
	case DDP_COMPONENT_DSI1:
		sof |= 2 << 6;
		break;
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
	case DDP_COMPONENT_DP_INTF0:
		sof |= 3 << 6;
		break;
	}
	if (!sof)
		mml_err("no sof/eof source %u/%u but not cmd mode",
			ctl->sof_src, ctl->eof_src);
	return sof;
}

static void mutex_addon_config_dl(struct mtk_ddp_comp *ddp_comp,
				  enum mtk_ddp_comp_id prev,
				  enum mtk_ddp_comp_id next,
				  union mtk_addon_config *addon_config,
				  struct cmdq_pkt *pkt)
{
	struct mml_mutex *mutex = ddp_comp_to_mutex(ddp_comp);
	struct mtk_addon_mml_config *cfg = &addon_config->addon_mml_config;
	const struct mml_topology_path *path;

	if (!cfg->ctx) {
		mml_err("%s cannot configure %d without ctx", __func__, cfg->config_type.type);
		return;
	}

	if (cfg->config_type.type == ADDON_CONNECT) {
		path = mml_drm_query_dl_path(cfg->ctx, &cfg->submit, cfg->pipe);
		if (!path) {
			mml_err("%s mml_drm_query_dl_path fail", __func__);
			return;
		}
		cmdq_pkt_clear_event(pkt, cfg->submit.info.disp_done_event);
		mutex_enable(mutex, pkt, path, get_mutex_sof(&cfg->mutex), MML_MODE_DIRECT_LINK);
		mutex->connected_mode = MML_MODE_DIRECT_LINK;
	}
}

static void mutex_addon_config_addon(struct mtk_ddp_comp *ddp_comp,
				     enum mtk_ddp_comp_id prev,
				     enum mtk_ddp_comp_id next,
				     union mtk_addon_config *addon_config,
				     struct cmdq_pkt *pkt)
{
	struct mml_mutex *mutex = ddp_comp_to_mutex(ddp_comp);
	struct mtk_addon_mml_config *cfg = &addon_config->addon_mml_config;
	const struct mml_topology_path *path;
	u32 mutex_sof;

	if (!cfg->task) {
		mml_err("%s cannot configure %d without ctx", __func__, cfg->config_type.type);
		return;
	}

	path = cfg->task->config->path[cfg->pipe];

	if (cfg->config_type.type == ADDON_DISCONNECT) {
		if (atomic_cmpxchg_acquire(&mutex->connect[cfg->pipe], 1, 0))
			mutex_disable(mutex, pkt, path);
		else
			mml_err("%s disconnect without connect pipe %u", __func__, cfg->pipe);
	} else {
		atomic_set(&mutex->connect[cfg->pipe], 1);
		mutex_sof = cfg->mutex.is_cmd_mode ? 0 : get_mutex_sof(&cfg->mutex);
		mutex_enable(mutex, pkt, path, mutex_sof, MML_MODE_DDP_ADDON);
		mutex->connected_mode = MML_MODE_DDP_ADDON;
	}
}

static void mutex_addon_config(struct mtk_ddp_comp *ddp_comp,
			       enum mtk_ddp_comp_id prev,
			       enum mtk_ddp_comp_id next,
			       union mtk_addon_config *addon_config,
			       struct cmdq_pkt *pkt)
{
	struct mtk_addon_mml_config *cfg = &addon_config->addon_mml_config;
	enum mml_mode mode = cfg->submit.info.mode;

	if (mode == MML_MODE_UNKNOWN) {
		struct mml_mutex *mutex = ddp_comp_to_mutex(ddp_comp);

		if (mutex->connected_mode == MML_MODE_UNKNOWN ||
			!atomic_read(&mutex->connect[cfg->pipe])) {
			/* no current connected mode, stop */
			return;
		}

		mode = mutex->connected_mode;
	}

	if (mode == MML_MODE_DIRECT_LINK)
		mutex_addon_config_dl(ddp_comp, prev, next, addon_config, pkt);
	else if (mode == MML_MODE_DDP_ADDON)
		mutex_addon_config_addon(ddp_comp, prev, next, addon_config, pkt);
	else
		mml_err("%s not support mode %d(%d)", __func__, mode, cfg->submit.info.mode);
}

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
	.addon_config = mutex_addon_config,
};

static struct mml_mutex *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_mutex *priv;
	struct device_node *node = dev->of_node;
	struct property *prop;
	const char *name;
	u32 mod[3], comp_id, mutex_id;
	s32 id_count, i, ret;
	bool add_ddp = true;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	of_property_for_each_string(node, "mutex-comps", prop, name) {
		ret = of_property_read_u32_array(node, name, mod, 3);
		if (ret) {
			dev_err(dev, "no property %s in dts node %s: %d\n",
				name, dev->of_node->full_name, ret);
			return ret;
		}
		if (mod[0] >= MML_MAX_COMPONENTS) {
			dev_err(dev, "%s id %u is larger than max:%d\n",
				name, mod[0], MML_MAX_COMPONENTS);
			return -EINVAL;
		}
		if (mod[1] >= priv->data->mod_cnt) {
			dev_err(dev,
				"%s mod index %u is larger than count:%d\n",
				name, mod[1], priv->data->mod_cnt);
			return -EINVAL;
		}
		if (mod[2] >= 32) {
			dev_err(dev,
				"%s mod field %u is larger than bits:32\n",
				name, mod[2]);
			return -EINVAL;
		}
		priv->modules[mod[0]].index = mod[1];
		priv->modules[mod[0]].field = mod[2];
		priv->modules[mod[0]].trigger = true;
	}

	id_count = of_property_count_u32_elems(node, "mutex-ids");
	for (i = 0; i + 1 < id_count; i += 2) {
		of_property_read_u32_index(node, "mutex-ids", i, &comp_id);
		of_property_read_u32_index(node, "mutex-ids", i + 1, &mutex_id);
		if (comp_id >= MML_MAX_COMPONENTS) {
			dev_err(dev, "component id %u is larger than max:%d\n",
				comp_id, MML_MAX_COMPONENTS);
			return -EINVAL;
		}
		if (mutex_id >= priv->data->mutex_cnt) {
			dev_err(dev, "mutex id %u is larger than count:%d\n",
				mutex_id, priv->data->mutex_cnt);
			return -EINVAL;
		}
		priv->modules[comp_id].mutex_id = mutex_id;
		priv->modules[comp_id].select = true;
	}

	priv->comp.config_ops = &mutex_config_ops;
	priv->comp.debug_ops = &mutex_debug_ops;

	if (!of_property_read_u16(dev->of_node, "event-pipe0-mml", &priv->event_pipe0_mml))
		mml_log("dl event event_pipe0_mml %u", priv->event_pipe0_mml);
	if (!of_property_read_u16(dev->of_node, "event-pipe1-mml", &priv->event_pipe1_mml))
		mml_log("dl event event_pipe1_mml %u", priv->event_pipe1_mml);

	ret = mml_ddp_comp_init(dev, &priv->ddp_comp, &priv->comp,
				&ddp_comp_funcs);
	if (ret) {
		mml_log("failed to init ddp component: %d", ret);
		add_ddp = false;
	}

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (add_ddp)
		ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_mutex_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_mutex",
		.data = &mt6983_mutex_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6879-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6895-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6985-mml_mutex",
		.data = &mt6983_mutex_data,
	},
	{
		.compatible = "mediatek,mt6886-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6897-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6989-mml_mutex",
		.data = &mt6983_mutex_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_mutex_driver_dt_match);

struct platform_driver mml_mutex_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-mutex",
		.owner = THIS_MODULE,
		.of_match_table = mml_mutex_driver_dt_match,
	},
};

//module_platform_driver(mml_mutex_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%llx name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%llx pw: %d clk: %d\n",
				comp->larb_port, comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -     ddp comp_id: %d bound: %d\n",
				dbg_probed_components[i]->ddp_comp.id,
				dbg_probed_components[i]->ddp_bound);
		}
		break;
	default:
		mml_err("not support read for debug case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(mutex_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(mutex_debug, "mml mutex debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML Mutex driver");
MODULE_LICENSE("GPL v2");
