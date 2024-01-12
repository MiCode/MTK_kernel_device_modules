// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <uapi/linux/sched/types.h>
#include <linux/pinctrl/consumer.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_trace.h"
#include "mtk_dsi.h"
#include "mi_disp/mi_dsi_panel.h"

#define ESD_TRY_CNT 5
#define ESD_CHK_TRY_CNT 5
#define ESD_CHECK_PERIOD 2000 /* ms */
#define esd_timer_to_mtk_crtc(x) container_of(x, struct mtk_drm_crtc, esd_timer)

static DEFINE_MUTEX(pinctrl_lock);

/* pinctrl implementation */
long _set_state(struct drm_crtc *crtc, const char *name)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct pinctrl_state *pState = 0;
	long ret = 0;

	mutex_lock(&pinctrl_lock);
	if (!priv->pctrl) {
		DDPPR_ERR("this pctrl is null\n");
		ret = -1;
		goto exit;
	}

	pState = pinctrl_lookup_state(priv->pctrl, name);
	if (IS_ERR(pState)) {
		DDPPR_ERR("lookup state '%s' failed\n", name);
		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(priv->pctrl, pState);

exit:
	mutex_unlock(&pinctrl_lock);
	return ret; /* Good! */
#else
	return 0; /* Good! */
#endif
}

long disp_dts_gpio_init(struct device *dev, struct mtk_drm_private *private)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	long ret = 0;
	struct pinctrl *pctrl;

	/* retrieve */
	pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pctrl)) {
		DDPPR_ERR("Cannot find disp pinctrl!\n");
		ret = PTR_ERR(pctrl);
		goto exit;
	}

	private->pctrl = pctrl;

exit:
	return ret;
#else
	return 0;
#endif
}

static inline int _can_switch_check_mode(struct drm_crtc *crtc,
					 struct mtk_panel_ext *panel_ext)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	int ret = 0;

	if (panel_ext->params->cust_esd_check == 0 &&
	    panel_ext->params->lcm_esd_check_table[0].cmd != 0 &&
	    mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_ESD_CHECK_SWITCH))
		ret = 1;

	return ret;
}

static inline int _lcm_need_esd_check(struct mtk_panel_ext *panel_ext)
{
	int ret = 0;

	if (panel_ext->params->esd_check_enable == 1)
		ret = 1;

	return ret;
}

static inline int need_wait_esd_eof(struct drm_crtc *crtc,
				    struct mtk_panel_ext *panel_ext)
{
	int ret = 1;

	/*
	 * 1.vdo mode
	 * 2.cmd mode te
	 */
	if (!mtk_crtc_is_frame_trigger_mode(crtc))
		ret = 0;

	if (panel_ext->params->cust_esd_check == 0)
		ret = 0;

	return ret;
}

static void esd_cmdq_timeout_cb(struct cmdq_cb_data data)
{
	struct drm_crtc *crtc = data.data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;

	if (!crtc) {
		DDPMSG("%s find crtc fail\n", __func__);
		return;
	}

	DDPMSG("read flush fail\n");
	esd_ctx->chk_sta = 0xff;
	mtk_drm_crtc_analysis(crtc);
	mtk_drm_crtc_dump(crtc);
}

int _mtk_esd_check_read(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	struct mtk_panel_ext *panel_ext;
	struct cmdq_pkt *cmdq_handle, *cmdq_handle2;
	struct mtk_drm_esd_ctx *esd_ctx;
	int index = drm_crtc_index(crtc);
	int ret = 0;

	DDPINFO("[ESD%u]%s\n", index, __func__);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		return -EINVAL;
	}

	if (mtk_drm_is_idle(crtc) && mtk_dsi_is_cmd_mode(output_comp))
		return 0;

	mtk_ddp_comp_io_cmd(output_comp, NULL, REQ_PANEL_EXT, &panel_ext);
	if (unlikely(!(panel_ext && panel_ext->params))) {
		DDPPR_ERR("%s:can't find panel_ext handle\n", __func__);
		return -EINVAL;
	}

	esd_ctx = mtk_crtc->esd_ctx;

	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cmdq_handle->err_cb.cb = esd_cmdq_timeout_cb;
	cmdq_handle->err_cb.data = crtc;

	CRTC_MMP_MARK(index, esd_check, 2, 1);

	if (mtk_dsi_is_cmd_mode(output_comp)) {
		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_SECOND_PATH, 0);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
						 DDP_FIRST_PATH, 0);

		cmdq_pkt_wfe(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

		/* Record Vblank start timestamp */
		mtk_vblank_config_rec_start(mtk_crtc, cmdq_handle, ESD_CHECK);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, ESD_CHECK_READ,
				    (void *)mtk_crtc);

		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

		/* Record Vblank end timestamp and calculate duration */
		mtk_vblank_config_rec_end_cal(mtk_crtc, cmdq_handle, ESD_CHECK);
	} else { /* VDO mode */
		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_SECOND_PATH,
						 (mtk_crtc->is_mml || mtk_crtc->is_mml_dl) ? 0 : 1);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH,
						 (mtk_crtc->is_mml || mtk_crtc->is_mml_dl) ? 0 : 1);

		if (mtk_crtc->msync2.msync_on) {
			u32 vfp_early_stop = 1;

			mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_VFP_EARLYSTOP,
							&vfp_early_stop);
		}

		CRTC_MMP_MARK(index, esd_check, 2, 2);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_STOP_VDO_MODE,
				    NULL);

		CRTC_MMP_MARK(index, esd_check, 2, 3);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, ESD_CHECK_READ,
				    (void *)mtk_crtc);

		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle,
				    DSI_START_VDO_MODE, NULL);

		mtk_disp_mutex_trigger(mtk_crtc->mutex[0], cmdq_handle);
		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, COMP_REG_START,
				    NULL);
		if (atomic_read(&esd_ctx->target_time) == 0) {
			if (esd_ctx->chk_retry < ESD_CHK_TRY_CNT) {
				esd_ctx->chk_retry++;
				ret = 0;
				DDPINFO("%s: miss target line, retry count:%u\n",
					__func__, esd_ctx->chk_retry);
				goto done;
			}
			DDPMSG("%s: miss target line, retry timeout:%u\n",
				__func__, esd_ctx->chk_retry);
		}
	}
	esd_ctx->chk_retry = 0;
	esd_ctx->chk_sta = 0;
	CRTC_MMP_MARK(index, esd_check, 2, 4);
	cmdq_pkt_flush(cmdq_handle);

	CRTC_MMP_MARK(index, esd_check, 2, 5);


	mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_READ_EPILOG,
				    NULL);
	if (esd_ctx->chk_sta == 0xff) {
		ret = -1;
		if (need_wait_esd_eof(crtc, panel_ext)) {
			/* TODO: set ESD_EOF event through CPU is better */
			mtk_crtc_pkt_create(&cmdq_handle2, crtc,
				mtk_crtc->gce_obj.client[CLIENT_CFG]);

			cmdq_pkt_set_event(
				cmdq_handle2,
				mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
			cmdq_pkt_flush(cmdq_handle2);
			cmdq_pkt_destroy(cmdq_handle2);
		}
		goto done;
	}

	ret = mtk_ddp_comp_io_cmd(output_comp, NULL, ESD_CHECK_CMP,
				  (void *)mtk_crtc);
done:
	cmdq_pkt_destroy(cmdq_handle);
	return ret;
}

static irqreturn_t _esd_check_ext_te_irq_handler(int irq, void *data)
{
	struct mtk_drm_esd_ctx *esd_ctx = (struct mtk_drm_esd_ctx *)data;

	atomic_set(&esd_ctx->ext_te_event, 1);
	wake_up_interruptible(&esd_ctx->ext_te_wq);

	return IRQ_HANDLED;
}

static int _mtk_esd_check_eint(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	int index = drm_crtc_index(crtc);
	int ret = 1;

	DDPINFO("[ESD%u]%s\n", index, __func__);

	if (unlikely(!esd_ctx)) {
		DDPPR_ERR("%s:invalid ESD context\n", __func__);
		return -EINVAL;
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_DUAL_TE) &&
			(atomic_read(&mtk_crtc->d_te.te_switched) == 1))
		atomic_set(&mtk_crtc->d_te.esd_te1_en, 1);
	else
		enable_irq(esd_ctx->eint_irq);

	/* check if there is TE in the last 2s, if so ESD check is pass */
	if (wait_event_interruptible_timeout(
		    esd_ctx->ext_te_wq,
		    atomic_read(&esd_ctx->ext_te_event),
		    HZ / 2) > 0)
		ret = 0;

	if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_DUAL_TE) &&
			(atomic_read(&mtk_crtc->d_te.te_switched) == 1))
		atomic_set(&mtk_crtc->d_te.esd_te1_en, 0);
	else
		disable_irq(esd_ctx->eint_irq);
	atomic_set(&esd_ctx->ext_te_event, 0);

	return ret;
}

static int mtk_drm_request_eint(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	struct mtk_ddp_comp *output_comp;
	struct device_node *node;
	u32 ints[2] = {0, 0};
	char *compat_str = "";
	int ret = 0;

	if (unlikely(!esd_ctx)) {
		DDPPR_ERR("%s:invalid ESD context\n", __func__);
		return -EINVAL;
	}

	if (unlikely(esd_ctx->eint_irq != -1)) {
		DDPPR_ERR("%s: reentry with inited eint_irq %d\n", __func__, esd_ctx->eint_irq);
		return -EINVAL;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		return -EINVAL;
	}

	mtk_ddp_comp_io_cmd(output_comp, NULL, REQ_ESD_EINT_COMPAT,
			    &compat_str);
	if (unlikely(!compat_str)) {
		DDPPR_ERR("%s: invalid compat string\n", __func__);
		return -EINVAL;
	}
	node = of_find_compatible_node(NULL, NULL, compat_str);
	if (unlikely(!node)) {
		DDPPR_ERR("can't find ESD TE eint compatible node %s\n", compat_str);
		return -EINVAL;
	}

	of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
	esd_ctx->eint_irq = irq_of_parse_and_map(node, 0);

	ret = request_irq(esd_ctx->eint_irq, _esd_check_ext_te_irq_handler,
			  IRQF_TRIGGER_RISING, "ESD_TE-eint", esd_ctx);
	if (ret) {
		DDPPR_ERR("eint irq line %u not available! %d\n", esd_ctx->eint_irq, ret);
		return ret;
	}

	disable_irq(esd_ctx->eint_irq);

	/* mode_te_te1 mapping to non-primary display's TE */
	if (output_comp->id == DDP_COMPONENT_DSI0)
		_set_state(crtc, "mode_te_te");
	else
		_set_state(crtc, "mode_te_te1");

	return ret;
}

static int mtk_drm_esd_check(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext;
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;
	int index = drm_crtc_index(crtc);
	int ret = 0;

	CRTC_MMP_EVENT_START(index, esd_check, 0, 0);

	if (mtk_crtc->enabled == 0) {
		CRTC_MMP_MARK(index, esd_check, 0, 99);
		DDPINFO("[ESD%d]CRTC disable. skip esd check\n", index);
		goto done;
	}

	panel_ext = mtk_crtc->panel_ext;
	if (unlikely(!(panel_ext && panel_ext->params))) {
		DDPPR_ERR("can't find panel_ext handle\n");
		ret = -EINVAL;
		goto done;
	}

	/* Check panel EINT */
	mtk_drm_trace_begin("esd_check:%d-%d", panel_ext->params->cust_esd_check, esd_ctx->chk_mode);
	if (panel_ext->params->cust_esd_check == 0 &&
	    esd_ctx->chk_mode == READ_EINT) {
		CRTC_MMP_MARK(index, esd_check, 1, 0);
		ret = _mtk_esd_check_eint(crtc);
	} else { /* READ LCM CMD  */
		CRTC_MMP_MARK(index, esd_check, 2, 0);
		ret = _mtk_esd_check_read(crtc);
	}

	/* switch ESD check mode */
	if (_can_switch_check_mode(crtc, panel_ext) &&
	    !mtk_crtc_is_frame_trigger_mode(crtc) &&
	    esd_ctx->chk_retry == 0)
		esd_ctx->chk_mode =
			(esd_ctx->chk_mode == READ_EINT) ? READ_LCM : READ_EINT;

done:
	CRTC_MMP_EVENT_END(index, esd_check, esd_ctx->chk_retry, ret);
	mtk_drm_trace_end();
	return ret;
}

static atomic_t panel_dead;
int get_panel_dead_flag(void) {
 	return atomic_read(&panel_dead);
}
 EXPORT_SYMBOL(get_panel_dead_flag);

void set_panel_dead_flag(int value) {
 	atomic_set(&panel_dead, value);
}
EXPORT_SYMBOL(set_panel_dead_flag);

static int mtk_drm_esd_recover(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	int ret = 0;
	struct cmdq_pkt *cmdq_handle = NULL;
	int index = drm_crtc_index(crtc);

	atomic_set(&panel_dead, 1);
	CRTC_MMP_EVENT_START(index, esd_recovery, 0, 0);
	if (crtc->state && !crtc->state->active) {
		DDPMSG("%s: crtc is inactive\n", __func__);
		return 0;
	}
	CRTC_MMP_MARK(index, esd_recovery, 0, 1);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s: invalid output comp\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	mtk_drm_trace_begin("esd recover");
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	if (mtk_crtc->is_mml) {
		mtk_crtc_pkt_create(&cmdq_handle, crtc, mtk_crtc->gce_obj.client[CLIENT_CFG]);
		mtk_crtc_mml_racing_stop_sync(crtc, cmdq_handle,
					      mtk_crtc_is_frame_trigger_mode(crtc) ? true : false);
		/* flush cmdq with stop_vdo_mode before it set DSI_START to 0 */
	}

	mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, CONNECTOR_PANEL_DISABLE, NULL);

	mtk_drm_crtc_disable(crtc, true);
	CRTC_MMP_MARK(index, esd_recovery, 0, 2);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_MMQOS_SUPPORT)) {
	//	if (drm_crtc_index(crtc) == 0)
	//		mtk_disp_set_hrt_bw(mtk_crtc,
	//			mtk_crtc->qos_ctx->last_hrt_req);
	}

	mtk_drm_crtc_enable(crtc);
	CRTC_MMP_MARK(index, esd_recovery, 0, 3);

	if (mtk_crtc->is_mml) {
		if (!kref_read(&mtk_crtc->mml_ir_sram.ref))
			mtk_crtc_alloc_sram(mtk_crtc, mtk_crtc->mml_ir_sram.expiry_hrt_idx);
		mtk_crtc_mml_racing_resubmit(crtc, NULL);
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_PANEL_ENABLE, NULL);

	CRTC_MMP_MARK(index, esd_recovery, 0, 4);

#ifdef CONFIG_MI_DISP_ESD_CHECK
	mtk_ddp_comp_io_cmd(output_comp, NULL, ESD_RESTORE_BACKLIGHT, NULL);
#endif

	mtk_crtc_hw_block_ready(crtc);
	if (mtk_crtc_is_frame_trigger_mode(crtc)) {
		struct cmdq_pkt *cmdq_handle;

		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);

		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);

		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
	CRTC_MMP_MARK(index, esd_recovery, 0, 5);

done:
	atomic_set(&panel_dead, 0);
	CRTC_MMP_EVENT_END(index, esd_recovery, 0, ret);
	mtk_drm_trace_end();

	return 0;
}

int mtk_drm_esd_testing_process(struct mtk_drm_esd_ctx *esd_ctx, bool need_lock)
{
		struct mtk_drm_private *private = NULL;
		struct drm_crtc *crtc = NULL;
		struct mtk_drm_crtc *mtk_crtc = NULL;
		int ret = 0;
		int i = 0;
		int recovery_flg = 0;
		unsigned int crtc_idx;

		if (!esd_ctx) {
			DDPPR_ERR("%s invalid ESD context, stop thread\n", __func__);
			return -EINVAL;
		}

		if (!esd_ctx->chk_active)
			return 0;

		crtc = esd_ctx->crtc;
		if (!crtc) {
			DDPPR_ERR("%s invalid CRTC context, stop thread\n", __func__);
			return -EINVAL;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		if (!mtk_crtc) {
			DDPPR_ERR("%s invalid mtk_crtc stop thread\n", __func__);
			return -EINVAL;
		}
		crtc_idx = drm_crtc_index(crtc);

		private = crtc->dev->dev_private;
		if (need_lock) {
			DDP_COMMIT_LOCK(&private->commit.lock, __func__, __LINE__);
			DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
			CRTC_MMP_MARK(crtc_idx, esd_check, 0x10CF, 0);
		}

		i = 0; /* repeat */
		do {
			mtk_drm_trace_begin("esd loop:%d", i);
			ret = mtk_drm_esd_check(crtc);
			if (!ret) /* success */
				break;

			DDPPR_ERR("[ESD%u]esd check fail, will do esd recovery. try=%d\n",
				crtc_idx, i);
			mtk_drm_esd_recover(crtc);
			recovery_flg = 1;
			mtk_drm_trace_end();
		} while (++i < ESD_TRY_CNT);

		if (ret != 0) {
			DDPPR_ERR(
				"[ESD%u]after esd recovery %d times, still fail, disable esd check\n",
				crtc_idx, ESD_TRY_CNT);
			mtk_disp_esd_check_switch(crtc, false);

			if (need_lock) {
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
				DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, __LINE__);
			}
			return 0;
		} else if (recovery_flg && ret == 0) {
			DDPPR_ERR("[ESD%u] esd recovery success\n", crtc_idx);
			recovery_flg = 0;
		}
		mtk_drm_trace_end("esd");

		if (need_lock) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, __LINE__);
		}

		return 0;
}

static void mtk_esd_timer_do(struct timer_list *esd_timer)
{
	//wake up interrupt
	struct mtk_drm_esd_ctx *esd_ctx =
		container_of(esd_timer, struct mtk_drm_esd_ctx, esd_timer);
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int index = 0;

	if (!esd_ctx) {
		DDPPR_ERR("%s invalid ESD_CTX\n", __func__);
		return;
	}

	if (esd_ctx->crtc)
		mtk_crtc = to_mtk_crtc(esd_ctx->crtc);
	if (mtk_crtc)
		index = drm_crtc_index(&mtk_crtc->base);
	CRTC_MMP_MARK(index, target_time, 0x10000, 1);
	atomic_set(&esd_ctx->target_time, 1);
	wake_up_interruptible(&esd_ctx->check_task_wq);
}

static void init_esd_timer(struct mtk_drm_esd_ctx *esd_ctx)
{
	if (unlikely(!esd_ctx)) {
		DDPPR_ERR("%s invalid ESD context\n", __func__);
		return;
	}

	timer_setup(&esd_ctx->esd_timer, mtk_esd_timer_do, 0);
	mod_timer(&esd_ctx->esd_timer, jiffies + (1*HZ));
}

static int mtk_drm_esd_check_worker_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 87};
	struct mtk_drm_esd_ctx *esd_ctx = (struct mtk_drm_esd_ctx *)data;
	int ret = 0, index = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	if (!esd_ctx) {
		DDPPR_ERR("%s invalid ESD context, stop thread\n", __func__);

		return -EINVAL;
	}
	if (esd_ctx->crtc)
		index = drm_crtc_index(esd_ctx->crtc);

	while (1) {
		msleep(ESD_CHECK_PERIOD);
		if (esd_ctx->chk_en == 0)
			continue;

		esd_ctx->chk_retry = 0;
		do {
			init_esd_timer(esd_ctx);
			atomic_set(&esd_ctx->target_time, 0);

			ret = wait_event_interruptible(
				esd_ctx->check_task_wq,
				atomic_read(&esd_ctx->check_wakeup) &&
				(atomic_read(&esd_ctx->target_time) ||
					esd_ctx->chk_mode == READ_EINT));
			if (ret < 0) {
				DDPINFO("[ESD]check thread waked up accidently\n");
				continue;
			}
			CRTC_MMP_MARK(index, esd_check, 0x57A7, esd_ctx->chk_retry);
			del_timer_sync(&esd_ctx->esd_timer);
			mtk_drm_esd_testing_process(esd_ctx, true);
		} while (esd_ctx->chk_retry > 0);

		if (atomic_read(&esd_ctx->target_time)) {
			CRTC_MMP_MARK(index, target_time, 0x10000, 0);
			atomic_set(&esd_ctx->target_time, 0);
		}

		/* 2. other check & recovery */
		if (kthread_should_stop())
			break;
	}
	return 0;
}

void mtk_disp_esd_check_switch(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = NULL;
	struct mtk_ddp_comp *output_comp;
	int index = drm_crtc_index(crtc);

	output_comp = (mtk_crtc) ? mtk_ddp_comp_request_output(mtk_crtc) : NULL;

	if (!mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_ESD_CHECK_RECOVERY))
		return;

	DDPINFO("%s %u, esd chk active: %d\n", __func__, index, enable);

	if (output_comp && mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI) {
		struct mtk_dsi *mtk_dsi = container_of(output_comp, struct mtk_dsi, ddp_comp);

		if (mtk_dsi)
			esd_ctx = mtk_dsi->esd_ctx;
	}

	if (esd_ctx) {
		mtk_crtc->esd_ctx = esd_ctx;
	} else {
		DDPPR_ERR("%s:%d invalid ESD context, crtc id:%d\n",
				__func__, __LINE__, index);
		return;
	}
	esd_ctx->chk_active = enable;
	if (enable)
		esd_ctx->crtc = crtc;
	else
		esd_ctx->crtc = NULL;

	atomic_set(&esd_ctx->check_wakeup, enable);
	if (enable)
		wake_up_interruptible(&esd_ctx->check_task_wq);
}

static void mtk_disp_esd_chk_deinit(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_esd_ctx *esd_ctx = mtk_crtc->esd_ctx;

	if (unlikely(!esd_ctx)) {
		DDPPR_ERR("%s:invalid ESD context\n", __func__);
		return;
	}

	/* Stop ESD task */
	mtk_disp_esd_check_switch(crtc, false);

	/* Stop ESD kthread */
	kthread_stop(esd_ctx->disp_esd_chk_task);

	kfree(esd_ctx);
}

static void mtk_disp_esd_chk_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext;
	struct mtk_drm_esd_ctx *esd_ctx;
	struct mtk_ddp_comp *output_comp;

	if (IS_ERR_OR_NULL(mtk_crtc))
		return;

	output_comp = (mtk_crtc) ? mtk_ddp_comp_request_output(mtk_crtc) : NULL;
	if (unlikely(!output_comp)) {
		DDPMSG("%s:invalid output comp\n", __func__);
		return;
	}

	panel_ext = mtk_crtc->panel_ext;
	if (!(panel_ext && panel_ext->params)) {
		DDPMSG("can't find panel_ext handle\n");
		return;
	}

	if (_lcm_need_esd_check(panel_ext) == 0)
		return;

	DDPINFO("create ESD thread\n");
	/* primary display check thread init */
	esd_ctx = kzalloc(sizeof(*esd_ctx), GFP_KERNEL);
	if (!esd_ctx) {
		DDPPR_ERR("allocate ESD context failed!\n");
		return;
	}
	mtk_crtc->esd_ctx = esd_ctx;
	if (output_comp && mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI) {
		struct mtk_dsi *dsi = container_of(output_comp, struct mtk_dsi, ddp_comp);

		if (dsi)
			dsi->esd_ctx = esd_ctx;
	}

	esd_ctx->eint_irq = -1;
	esd_ctx->chk_en = 1;
	esd_ctx->crtc = crtc;
	esd_ctx->disp_esd_chk_task = kthread_create(
		mtk_drm_esd_check_worker_kthread, esd_ctx, "disp_echk");

	init_waitqueue_head(&esd_ctx->check_task_wq);
	init_waitqueue_head(&esd_ctx->ext_te_wq);
	atomic_set(&esd_ctx->check_wakeup, 0);
	atomic_set(&esd_ctx->ext_te_event, 0);
	atomic_set(&esd_ctx->target_time, 0);
	if (panel_ext->params->cust_esd_check == 1)
		esd_ctx->chk_mode = READ_LCM;
	else
		esd_ctx->chk_mode = READ_EINT;
	mtk_drm_request_eint(crtc);

	wake_up_process(esd_ctx->disp_esd_chk_task);
}

void mtk_disp_chk_recover_deinit(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	output_comp = (mtk_crtc) ? mtk_ddp_comp_request_output(mtk_crtc) : NULL;

	/* only support ESD check for DSI output interface */
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_ESD_CHECK_RECOVERY) &&
			output_comp && mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI)
		mtk_disp_esd_chk_deinit(crtc);
}

void mtk_disp_chk_recover_init(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	output_comp = (mtk_crtc) ? mtk_ddp_comp_request_output(mtk_crtc) : NULL;

	/* only support ESD check for DSI output interface */
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_ESD_CHECK_RECOVERY) &&
			output_comp && mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI)
		mtk_disp_esd_chk_init(crtc);
}
