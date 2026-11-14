// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Author: Yunfei Wang <yf.wang@mediatek.com>
 */

#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#include "mtk-iommu-util.h"

static bool smmu_v3_enable;

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC) && IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_IOMMU)
static const struct mtk_iommu_ops *iommu_ops;

int mtk_iommu_set_ops(const struct mtk_iommu_ops *ops)
{
	if (iommu_ops == NULL)
		iommu_ops = ops;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_set_ops);

int mtk_iommu_update_pm_status(u32 type, u32 id, bool pm_sta)
{
	if (iommu_ops && iommu_ops->update_pm_status)
		return iommu_ops->update_pm_status(type, id, pm_sta);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_iommu_update_pm_status);

void mtk_iommu_set_pm_ops(const struct mtk_iommu_mm_pm_ops *ops)
{
	if (iommu_ops && iommu_ops->set_pm_ops)
		iommu_ops->set_pm_ops(ops);
}
EXPORT_SYMBOL_GPL(mtk_iommu_set_pm_ops);

#endif /* CONFIG_DEVICE_MODULES_MTK_IOMMU */

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC) && IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
static const struct mtk_smmu_ops *smmu_ops;
static struct smmu_tbu_data *smmu_tbu_datas;

static int mtk_smmu_tbu_data_init(void)
{
	u32 i;

	smmu_tbu_datas = kcalloc(SMMU_TYPE_NUM, sizeof(*smmu_tbu_datas), GFP_KERNEL);
	if (!smmu_tbu_datas)
		return -ENOMEM;

	for (i = 0; i < SMMU_TYPE_NUM; i++) {
		struct smmu_tbu_data *data = &smmu_tbu_datas[i];

		INIT_LIST_HEAD(&data->tbu_devices);
		spin_lock_init(&data->tbu_lock);
		data->type = i;
	}

	return 0;
}

static void mtk_smmu_tbu_data_exit(void)
{
	if (smmu_tbu_datas) {
		kfree(smmu_tbu_datas);
		smmu_tbu_datas = NULL;
	}
}

static struct mtk_smmu_data *mtk_smmu_data_get(u32 smmu_type)
{
	if (smmu_ops && smmu_ops->get_smmu_data)
		return smmu_ops->get_smmu_data(smmu_type);

	return NULL;
}

int mtk_smmu_set_ops(const struct mtk_smmu_ops *ops)
{
	if (smmu_ops == NULL)
		smmu_ops = ops;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_set_ops);

int mtk_smmu_rpm_get(u32 smmu_type)
{
	struct mtk_smmu_data *data = mtk_smmu_data_get(smmu_type);

	if (data && smmu_ops && smmu_ops->smmu_power_get)
		return smmu_ops->smmu_power_get(&data->smmu);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_smmu_rpm_get);

int mtk_smmu_rpm_put(u32 smmu_type)
{
	struct mtk_smmu_data *data = mtk_smmu_data_get(smmu_type);

	if (data && smmu_ops && smmu_ops->smmu_power_put)
		return smmu_ops->smmu_power_put(&data->smmu);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_smmu_rpm_put);

int mtk_smmu_register_tbu(struct smmu_tbu_device *tbu)
{
	struct smmu_tbu_data *tbu_data;
	unsigned long flags;

	if (!smmu_tbu_datas)
		return -ENOENT;

	if (!tbu || tbu->type >= SMMU_TYPE_NUM)
		return -EINVAL;

	tbu_data = &smmu_tbu_datas[tbu->type];

	spin_lock_irqsave(&tbu_data->tbu_lock, flags);
	list_add(&tbu->node, &tbu_data->tbu_devices);
	spin_unlock_irqrestore(&tbu_data->tbu_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_register_tbu);

int mtk_smmu_unregister_tbu(struct smmu_tbu_device *tbu)
{
	struct smmu_tbu_data *tbu_data;
	unsigned long flags;

	if (!smmu_tbu_datas)
		return -ENOENT;

	if (!tbu || tbu->type >= SMMU_TYPE_NUM)
		return -EINVAL;

	tbu_data = &smmu_tbu_datas[tbu->type];

	spin_lock_irqsave(&tbu_data->tbu_lock, flags);
	list_del(&tbu->node);
	spin_unlock_irqrestore(&tbu_data->tbu_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_unregister_tbu);

struct smmu_tbu_data *mtk_smmu_tbu_data_get(u32 smmu_type)
{
	if (!smmu_tbu_datas || smmu_type >= SMMU_TYPE_NUM)
		return NULL;

	return &smmu_tbu_datas[smmu_type];
}
EXPORT_SYMBOL_GPL(mtk_smmu_tbu_data_get);
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */

static int __init mtk_iommu_util_init(void)
{
	smmu_v3_enable = smmu_v3_enabled();
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC) && IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
	if (smmu_v3_enable)
		ret = mtk_smmu_tbu_data_init();
#endif
	return ret;
}

static void __exit mtk_iommu_util_exit(void)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC) && IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
	if (smmu_v3_enable)
		mtk_smmu_tbu_data_exit();
#endif
}

module_init(mtk_iommu_util_init);
module_exit(mtk_iommu_util_exit);

MODULE_DESCRIPTION("MediaTek IOMMU export API implementations");
MODULE_LICENSE("GPL");
