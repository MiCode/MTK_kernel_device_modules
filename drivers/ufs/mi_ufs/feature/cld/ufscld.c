
/*
 * mi_cld1.c
 *
 *  Created on: 2020-10-20
 *      Author: shane
 */
#include "ufscld.h"
#include "ufscld_sysfs.h"
#include "../../include/mi_ufshcd.h"
#include "../../core/mi_ufsctx.h"
#include "../../core/mi_ufshcd-priv.h"
#include "../../../ufs-mediatek.h"

struct ufscld_dev *cld;

extern struct ufscld_ops hid_ops;
extern struct ufscld_ops hynix_cld_ops;
extern struct ufscld_ops samsung_cld_ops;
extern struct ufscld_ops kioxia_cld_ops;
extern struct ufscld_ops micron_cld_ops;
extern struct ufscld_ops ymtc_cld_ops;

extern int Samsung_init_idn(struct ufscld_dev *cld);
extern int micron_init_idn(struct ufscld_dev *cld);

struct vendor_ops cld_ops_arry[] = {
		{"SAMSUNG", &samsung_cld_ops},
		{"SKhynix", &hynix_cld_ops},
		{"KIOXIA", &kioxia_cld_ops},
		{"MICRON", &micron_cld_ops},
		{"XBSTOR", &ymtc_cld_ops},
};
inline bool ufscld_schedule_delayed_work(struct delayed_work *dwork,
                                       unsigned long delay)
{
	return queue_delayed_work(system_freezable_wq, dwork, delay);
}

int ufscld_init_ops(struct ufscld_dev *cld)
{
	int ret = 0;
	int i = 0;
	struct ufs_hba *hba = cld->hba;
	memcpy(&cld->stdinq, hba->ufs_device_wlun->inquiry + 8, sizeof(cld->stdinq));
	for (i = 0; i < sizeof(cld_ops_arry)/sizeof(cld_ops_arry[0]); i++) {
		if (strncmp((char *)cld->stdinq.vendor_id, (char *)cld_ops_arry[i].vendor_id, strlen((char *)cld_ops_arry[i].vendor_id)) == 0) {
			cld->cld_ops = cld_ops_arry[i].cld_ops;
			cld->vendor_ops = &cld_ops_arry[i];
		}
	}

	if (strncmp((char *)cld->stdinq.vendor_id, "SAMSUNG", 7) == 0) {
		if (strncmp((char *)cld->stdinq.product_id+8, "K", strlen("K"))== 0) {
			INFO_MSG("Samsung V8 SAVONA UFS, execute JEDEC HID");
			cld->cld_ops = &hid_ops;
		} else {
			ret = Samsung_init_idn(cld);
			INFO_MSG("Samsung lower v8 savona master  UFS, execute SS HID");
			if (ret) {
				ERR_MSG("SAMSUNG init cld idn fail");
			}
		} 
	} else if (strncmp((char *)cld->stdinq.vendor_id, "MICRON", 6) == 0) {
		ret = micron_init_idn(cld);
		if (ret) {
			ERR_MSG("MICRON init cld idn fail");
			}
		} 
	return ret;
}

int ufscld_is_not_present(struct ufscld_dev *cld)
{
	enum UFSCLD_STATE cur_state = ufscld_get_state(cld);
	if (cur_state != CLD_PRESENT) {
		INFO_MSG("cld_state != CLD_PRESENT (%d)", cur_state);
		return -ENODEV;
	}
	return 0;
}
inline int ufscld_get_state(struct ufscld_dev *cld)
{
	if(!cld){
		INFO_MSG("cld is not init, please check.");
		return CLD_NEED_INIT;
	}
	return atomic_read(&cld->cld_state);
}
inline void ufscld_set_state(struct ufscld_dev *cld, int state)
{
	atomic_set(&cld->cld_state, state);
}
extern void ufshcd_scsi_block_requests(struct ufs_hba *hba);
extern int ufshcd_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us);
extern void ufshcd_scsi_unblock_requests(struct ufs_hba *hba);
void ufscld_block_enter_suspend(struct ufscld_dev *cld)
{
	struct ufs_hba *hba = cld->hba;
	unsigned long flags;
	if (unlikely(cld->block_suspend))
		return;
	cld->block_suspend = true;
	ufshcd_rpm_get_sync(hba);
	ufshcd_hold(hba);
	spin_lock_irqsave(hba->host->host_lock, flags);
	CLD_DEBUG(cld,
		"dev->power.usage_count %d sdev_ufs_device->sdev_gendev.power.usage_count %d hba->clk_gating.active_reqs %d",
		atomic_read(&hba->dev->power.usage_count),
		atomic_read(&hba->ufs_device_wlun->sdev_gendev.power.usage_count),
		hba->clk_gating.active_reqs);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}
void ufscld_allow_enter_suspend(struct ufscld_dev *cld)
{
	struct ufs_hba *hba = cld->hba;
	unsigned long flags;
	if (unlikely(!cld->block_suspend))
		return;
	cld->block_suspend = false;
	ufshcd_release(hba);
	ufshcd_rpm_put_noidle(hba);
	spin_lock_irqsave(hba->host->host_lock, flags);
	CLD_DEBUG(cld,
		"dev->power.usage_count %d sdev_ufs_device->sdev_gendev.power.usage_count %d hba->clk_gating.active_reqs %d",
		atomic_read(&hba->dev->power.usage_count),
		atomic_read(&hba->ufs_device_wlun->sdev_gendev.power.usage_count),
		hba->clk_gating.active_reqs);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}
int ufscld_get_frag_level(struct ufscld_dev *cld, int *frag_level)
{
	return cld->cld_ops->cld_get_frag_level(cld, frag_level);
}
int ufscld_get_operation_status(struct ufscld_dev *cld, int *op_status)
{
	return cld->cld_ops->cld_operation_status(cld, (u32 *)op_status);
}
static int ufscld_issue_enable(struct ufscld_dev *cld)
{
	if (cld->cld_ops->cld_set_trigger(cld, 1))
		return -EINVAL;
	return 0;
}
static int ufscld_issue_disable(struct ufscld_dev *cld)
{
	int frag_level;
	if (cld->cld_ops->cld_set_trigger(cld, 0))
		return -EINVAL;
	if (cld->cld_ops->cld_get_frag_level(cld, &frag_level))
		return -EINVAL;
	CLD_DEBUG(cld, "Frag_lv %d \n", frag_level);
	return 0;
}
static void trigger_uevent(struct ufscld_dev *cld, int trigger)
{
	char *cld_trigger_on[2]  = { "CLD_TRIGGER=ON", NULL };
	char *cld_trigger_off[2] = { "CLD_TRIGGER=OFF", NULL };
	if (trigger) {
		kobject_uevent_env(&cld->hba->dev->kobj, KOBJ_CHANGE, cld_trigger_on);
		INFO_MSG("UFSCLD: sent uevent %s\n", cld_trigger_on[0]);
	} else {
		kobject_uevent_env(&cld->hba->dev->kobj, KOBJ_CHANGE, cld_trigger_off);
		INFO_MSG("UFSCLD: sent uevent %s\n", cld_trigger_off[0]);
	}
}
/*
 * The CLD feature can be executed in the SPM by using a kernel thread.
 * By strongly checking the conditions,
 * it constraints to be performed only in the RPM.
 *
 * If the return value is not err, pm_runtime_put_noidle() must be called later.
 *
 * IMPORTANT : ufscld_hold_runtime_pm() & ufscld_release_runtime_pm() pair.
 */
int ufscld_hold_runtime_pm(struct ufscld_dev *cld)
{
	struct ufs_hba *hba = cld->hba;
	struct device *dev = &hba->ufs_device_wlun->sdev_gendev;
	if (ufscld_get_state(cld) == CLD_SUSPEND) {
		/* Check that device was suspended by System PM */
		if (!hba->pm_op_in_progress && dev->power.is_suspended)
			return -EACCES;
		/*
		 * Double Check for safe (SPM vs RPM)
		 * If it success, device was suspended by Runtime PM
		 */
		ufshcd_rpm_get_sync(cld->hba);
		/*
		 * Guaranteed that ufsf_resume() is completed
		 */
		down(&hba->host_sem);
		up(&hba->host_sem);
		/* If it success, device was suspended by Runtime PM */
		if (ufscld_get_state(cld) == CLD_PRESENT &&
		    hba->curr_dev_pwr_mode == UFS_ACTIVE_PWR_MODE &&
		    hba->uic_link_state == UIC_LINK_ACTIVE_STATE)
			goto resume_success;
		INFO_MSG("RPM resume failed. Maybe it was SPM suspend");
		INFO_MSG("UFS state (POWER = %d LINK = %d)",
			 hba->curr_dev_pwr_mode, hba->uic_link_state);
		ufshcd_rpm_put_noidle(cld->hba);
		return -ENODEV;
	}
	if (ufscld_is_not_present(cld))
		return -ENODEV;
	ufshcd_rpm_get_sync(cld->hba);
resume_success:
	return 0;
}
inline void ufscld_release_runtime_pm(struct ufscld_dev *cld)
{
	struct ufs_hba *hba = cld->hba;
	ufshcd_rpm_put_noidle(hba);
}
/*
 * Lock status: cld_sysfs lock was held when called.
 */
int ufscld_trigger_on(struct ufscld_dev *cld)
{
	int ret = 0;
	if (unlikely(cld->cld_trigger))
		return 0;
	cld->cld_trigger = true;
	ufscld_block_enter_suspend(cld);
	cld->start_time_stamp = ktime_to_ms(ktime_get());
	cld->start_work_time = ktime_to_ms(ktime_get());
	CLD_DEBUG(cld, "at %lld trigger 0 -> 1", cld->start_work_time);
	ufscld_schedule_delayed_work(&cld->cld_trigger_work, 0);
	return ret;
}
/*
 * Lock status: cld_sysfs lock was held when called.
 */
int ufscld_trigger_off(struct ufscld_dev *cld)
{
	int ret = 0;
	if (unlikely(!cld->cld_trigger))
		return 0;
	cld->cld_trigger = false;
	CLD_DEBUG(cld, "cld_trigger 1 -> 0");
	ufscld_issue_disable(cld);
	trigger_uevent(cld, 0);
	ufscld_allow_enter_suspend(cld);
	return ret;
}
static void ufscld_trigger_work_fn(struct work_struct *dwork)
{
	struct ufscld_dev *cld;
	int ret;
	enum CLD_STATUS op_status;
	ktime_t work_delta_time = 0;
	cld = container_of(dwork, struct ufscld_dev, cld_trigger_work.work);
	if (ufscld_is_not_present(cld)) {
		ERR_MSG("ufscld state is = %d\n", ufscld_get_state(cld));
		return;
	}
	ret = ufscld_hold_runtime_pm(cld);
	if (ret) {
		ERR_MSG("ufscld_hold_runtime_pm fail");
		return;
	}
	mutex_lock(&cld->sysfs_lock);
	if (!cld->cld_trigger) { // if  host manual stop cld
		CLD_DEBUG(cld, "cld_trigger == false, return");
		goto pm_put;
	}
	ret = ufscld_issue_enable(cld);
	INFO_MSG("ufscld_issue_enable ret=%d\n", ret);
	if (ret) {
		goto re_sched;
	}
	ufscld_get_operation_status(cld, (int *)&op_status);
	work_delta_time = ktime_to_ms(ktime_get()) - cld->start_work_time;
	INFO_MSG("ufscld status %d, work_delta_time %lld ms\n", op_status, work_delta_time);
	if (CLD_STATUS_IDLE == op_status || work_delta_time >= CLD_TRIGGER_WORKER_TIMEOUT_MS) { //cld entry idel or timeout
		WARN_MSG("CLD off with status (%d), duration (%lld ms)", op_status, work_delta_time);
		ufscld_trigger_off(cld);
		goto pm_put;
	} else if (CLD_STATUS_PROGRESSING == op_status || CLD_STATUS_INTERRUPT_BY_HOST == op_status) {
		CLD_DEBUG(cld, "cld_REQUIRED, so sched (%d ms)",
				cld->cld_trigger_delay);
	} else {
		CLD_DEBUG(cld, "issue_cld ERR(%X), so resched for retry",
			  ret);
	}
re_sched:
	ufscld_schedule_delayed_work(&cld->cld_trigger_work,
			      msecs_to_jiffies(cld->cld_trigger_delay));
pm_put:
	mutex_unlock(&cld->sysfs_lock);
	ufscld_release_runtime_pm(cld);
	CLD_DEBUG(cld, "end cld_trigger_work_fn");
}
void ufscld_suspend(struct ufscld_dev *cld)
{
	if (!cld)
		return;
	if (unlikely(cld->cld_trigger))
		ERR_MSG("cld_trigger was set to block the suspend. so weird");
	ufscld_set_state(cld, CLD_SUSPEND);
	cancel_delayed_work_sync(&cld->cld_trigger_work);
}
void ufscld_resume(struct ufscld_dev *cld)
{
	if (!cld)
		return;
	if (unlikely(cld->cld_trigger))
		ERR_MSG("cld_trigger need to off");
	ufscld_set_state(cld, CLD_PRESENT);
	if(cld->cld_trigger){
		CLD_DEBUG(cld, "CLD trigger = %d, resume cld_trigger_work", cld->cld_trigger);
		ufscld_schedule_delayed_work(&cld->cld_trigger_work, 0);
	}
}
/*
 * this function is called in irq context.
 * so cancel_delayed_work_sync() do not use due to waiting.
 */
void ufscld_on_idle(struct ufs_hba *hba)
{
	if(!cld)
		return;
	if (!cld->cld_trigger)
		return;// cld already done or not triggered.
	if (ufscld_get_state(cld) != CLD_PRESENT || hba->outstanding_reqs) {
			CLD_DEBUG(cld, "outstanding_reqs=%lu\n", hba->outstanding_reqs);
			return; // HID is not avalible or this is not the last cmd.
	}
	if (!cld->cld_ops) // cld no supported
		return;
	/*
	 * When cld_trigger_work will be scheduled,
	 * check cld_trigger under sysfs_lock.
	 */
	if (delayed_work_pending(&cld->cld_trigger_work))
		cancel_delayed_work(&cld->cld_trigger_work);
	ufscld_schedule_delayed_work(&cld->cld_trigger_work, msecs_to_jiffies(1000));// if there is no new cmd in 1S.
}
void ufscld_init(struct ufs_hba *hba)
{
	int ret = 0;
	cld = kzalloc(sizeof(struct ufscld_dev), GFP_KERNEL);
	cld->hba = hba;
	cld->cld_trigger = false;
	INFO_MSG("start ufscld_init");
	ret = ufscld_init_ops(cld);
	if (ret) {
		ERR_MSG("CLD get cld ops fail.\n");
		return;
	} else {
		INFO_MSG("CLD init ops succeed\n");
	}
	cld->cld_trigger_delay = CLD_TRIGGER_WORKER_DELAY_MS_DEFAULT;
	INIT_DELAYED_WORK(&cld->cld_trigger_work, ufscld_trigger_work_fn);
	cld->cld_debug = false;
	cld->block_suspend = false;
	cld->start_time_stamp = 0;
	cld->start_work_time = 0;
	ret = ufscld_create_sysfs(cld);
	if (ret) {
		ERR_MSG("ufscld sysfs create failed.\n");
		ufscld_set_state(cld, CLD_FAILED);
		kfree(cld);
		return;
	}
	INFO_MSG("UFS cld create sysfs finished.\n");
	ufscld_set_state(cld, CLD_PRESENT);
}
void ufscld_remove(struct ufs_hba *hba)
{
	if (!cld)
		return;
	INFO_MSG("start cld release");
	ufscld_set_state(cld, CLD_FAILED);
	cancel_delayed_work_sync(&cld->cld_trigger_work);
	mutex_lock(&cld->sysfs_lock);
	ufscld_allow_enter_suspend(cld);
	ufscld_trigger_off(cld);
	ufscld_remove_sysfs(cld);
	mutex_unlock(&cld->sysfs_lock);
	kfree(cld);
	INFO_MSG("CLD released\n");
}
void ufscld_reset_host(struct ufs_hba *hba)
{
	if (!hba)
		return;
	ufscld_set_state(cld, CLD_RESET);
	cancel_delayed_work_sync(&cld->cld_trigger_work);
}
void ufscld_reset(struct ufs_hba *hba)
{
	ufscld_set_state(cld, CLD_PRESENT);
	/*
	 * cld_trigger will be checked under sysfs_lock in worker.
	 */
	if (cld->cld_trigger)
		ufscld_schedule_delayed_work(&cld->cld_trigger_work, 0);
	INFO_MSG("reset completed.");
}


