// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/module.h>
#include "mtk-base-afe.h"
#include "mtk-sram-manager.h"

#include "mtk-scp-ultra.h"
#include "mtk-afe-fe-dai.h"
#include "mtk-afe-external.h"

static struct mtk_base_scp_ultra *local_base_scp_ultra;

int set_scp_ultra_base(struct mtk_base_scp_ultra *scp_ultra)
{
	if (!scp_ultra) {
		pr_err("%s(), scp_ultra is NULL", __func__);
		return -1;
	}

	local_base_scp_ultra = scp_ultra;
	return 0;

}
EXPORT_SYMBOL_GPL(set_scp_ultra_base);

void *get_scp_ultra_base(void)
{
	if (!local_base_scp_ultra)
		pr_err("%s(), local_base_scp_ultra is NULL", __func__);

	return local_base_scp_ultra;
}
EXPORT_SYMBOL_GPL(get_scp_ultra_base);

int mtk_scp_ultra_allocate_mem(struct snd_pcm_substream *substream,
			       dma_addr_t *phys_addr,
			       unsigned char **virt_addr,
			       unsigned int size)
{
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	struct audio_ultra_dram *ultra_resv_mem = &scp_ultra->ultra_reserve_dram;
#endif
	int buf_offset;

	if (id == scp_ultra->scp_ultra_dl_memif_id) {
		buf_offset = ULTRA_BUF_OFFSET;
	} else if (id == scp_ultra->scp_ultra_ul_memif_id) {
		buf_offset = ULTRA_BUF_OFFSET * 2;
	}  else {
		dev_err(scp_ultra->dev, "%s(), wrong memif id\n", __func__);
		return -EINVAL;
	}

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->bytes = size;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	dma_buf->addr =
		ultra_resv_mem->phy_addr + buf_offset;
	dma_buf->area =
		ultra_resv_mem->vir_addr + buf_offset;
	*phys_addr = dma_buf->addr;
	*virt_addr = dma_buf->area;
#else
	pr_debug("%s(), error, id %d, set addr\n",
			__func__, id);
	return -EINVAL;
#endif

	dev_info(scp_ultra->dev,
		"%s(),mem_id=%d, allcate ultra VA:0x%lx,PA:0x%pad,size:%lu,using_sram=0\n",
		__func__,
		id,
		(unsigned long)dma_buf->area,
		&dma_buf->addr,
		dma_buf->bytes);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_ultra_allocate_mem);

int notify_ultrasound_event(struct notifier_block *nb, unsigned long event, void *v)
{
	int status = NOTIFY_DONE; //default don't care it.

	if (event == NOTIFIER_ULTRASOUND_ALLOCATE_MEM) {
		struct snd_pcm_substream *substream;

		substream = (struct snd_pcm_substream *)v;
		pr_debug("%s(), ultrasound received afe notify init event.\n", __func__);
		if (mtk_scp_ultra_allocate_mem(substream,
				&substream->runtime->dma_addr,
				&substream->runtime->dma_area,
				substream->runtime->dma_bytes) == 0)
			status = NOTIFY_STOP;
		else
			status = NOTIFY_BAD;

	}
	return status;
}

bool is_ultra_reserved_dram_addr_valid(struct snd_pcm_substream *substream)
{
	unsigned long start = (unsigned long)substream->runtime->dma_area;
	size_t size = substream->runtime->dma_bytes;
	unsigned long end = start + size - 1;
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	bool found = false;
	struct audio_ultra_dram *ultra_resv_mem;

	if (scp_ultra == NULL)
		return false;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	ultra_resv_mem = &scp_ultra->ultra_reserve_dram;
	if (ultra_resv_mem->size == 0)
		return false;
	if ((start >= (unsigned long)ultra_resv_mem->vir_addr) &&
	    (start <= (unsigned long)(ultra_resv_mem->vir_addr + ultra_resv_mem->size)) ) {
		if (end <= (unsigned long)(ultra_resv_mem->vir_addr + ultra_resv_mem->size))
			found = true;
	}
#endif
	return found;
}
EXPORT_SYMBOL(is_ultra_reserved_dram_addr_valid);

/* define a notifier_block */
static struct notifier_block ultrasound_init_notifier = {
	.notifier_call = notify_ultrasound_event,
};

static int __init mtk_ultrasound_init(void)
{
	register_afe_allocate_mem_notifier(&ultrasound_init_notifier);
	return 0;
}

static void __exit mtk_ultrasound_exit(void)
{
	unregister_afe_allocate_mem_notifier(&ultrasound_init_notifier);
}

module_init(mtk_ultrasound_init);
module_exit(mtk_ultrasound_exit);

MODULE_DESCRIPTION("Mediatek scp ultra platform driver");
MODULE_AUTHOR("Ning Li <Ning.Li@mediatek.com>");
MODULE_LICENSE("GPL v2");
