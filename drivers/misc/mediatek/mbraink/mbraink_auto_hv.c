// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include "mbraink_auto_hv.h"

static struct nbl_trace g_nbl_trace = {0};

int mbraink_auto_cpuload_init(void)
{
	struct nbl_trace *tc = &g_nbl_trace;
	int ret = 0;

	if (!tc->rmem) {
		struct device_node *np = of_find_compatible_node(NULL, NULL,
				"mediatek,nbl_trace");
		if (!np) {
			pr_info("%s: failed to find nbl_trace node in device tree\n", __func__);
			return ret;
		}

		tc->rmem = of_reserved_mem_lookup(np);
		if (!tc->rmem) {
			pr_info("%s: failed to get reserved memory\n", __func__);
			return ret;
		}
	}

	tc->buf = ioremap_cache(tc->rmem->base, tc->rmem->size);
	if (!tc->buf) {
		pr_info("%s: failed to remap memory\n", __func__);
		return ret;
	}

	tc->cntfrq = arch_timer_get_cntfrq();

	return ret;

unmap:
	iounmap(tc->buf);
	return ret;
}

void mbraink_auto_cpuload_deinit(void)
{
	struct nbl_trace *tc = &g_nbl_trace;

	if (tc->buf)
		iounmap(tc->buf);
}

int mbraink_auto_vcpu_reader(struct nbl_trace *tc, struct nbl_trace_buf_trans *vcpu_loading_buf, void *vcpu_data_buffer)
{
	struct trace_vcpu_ringbuf_hdr *rb_hdr = &tc->buf->exec.rb_hdr;
	int raw_record_length = vcpu_loading_buf->length;
	void *vcpu_data_buf = vcpu_data_buffer;
	u8 *buf;
	struct timespec64 ts;
	u64 rd, wr, time_in_ms, cntvct, freq;
	size_t len, total_len = 0;

	if (!rb_hdr->enable) {
		pr_info("%s: vcpu record is not enabled\n", __func__);
		return 0;
	}

	rd = 0;
	wr = rb_hdr->write_pos % rb_hdr->rec_cnt;
	/* record length is bigger than write_pos */
	if (raw_record_length > wr) {
		/* write pos has overflowed */
		if (rb_hdr->write_pos >= rb_hdr->rec_cnt) {
			rd = rb_hdr->rec_cnt - (raw_record_length - wr);
			buf = (u8 *)&tc->buf->exec.recs[rd];
			len = (rb_hdr->rec_cnt - rd) * rb_hdr->rec_size;
			memcpy(vcpu_data_buf, buf, len);
			total_len += len;
		}
		/* if write_pos not overflow. we only have write_pos to copy. so set raw_record_length to wr */
		raw_record_length = wr;
	}
	rd = wr - raw_record_length;
	buf = (u8 *)&tc->buf->exec.recs[rd];
	len = (wr - rd) * rb_hdr->rec_size;
	memcpy(vcpu_data_buf + total_len, buf, len);
	total_len += len;

end:
	// reset pos
	rb_hdr->write_pos = 0;
	vcpu_loading_buf->length = len / rb_hdr->rec_size;
	vcpu_loading_buf->cntcvt = __arch_counter_get_cntvct();
	vcpu_loading_buf->cntfrq = arch_timer_get_cntfrq();
	ktime_get_real_ts64(&ts);
	vcpu_loading_buf->current_time = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;

	pr_notice("%s: read HV cpu loading data. data length %d\n", __func__, vcpu_loading_buf->length);
	return 0;
}

int mbraink_auto_get_vcpu_record(struct nbl_trace_buf_trans *vcpu_loading_buf, void *vcpu_data_buffer)
{
	struct nbl_trace *tc = &g_nbl_trace;

	if (tc->buf && vcpu_loading_buf)
		return mbraink_auto_vcpu_reader(tc, vcpu_loading_buf, vcpu_data_buffer);
	pr_info("%s: trace buffer not initialized!\n", __func__);
	return -1;
}

int mbraink_auto_set_vcpu_record(int enable)
{
	struct nbl_trace *tc = &g_nbl_trace;
	struct trace_vcpu_ringbuf_hdr *rb_hdr = NULL;

	if (tc->buf) {
		rb_hdr = &tc->buf->exec.rb_hdr;
		rb_hdr->write_pos = 0;
		rb_hdr->enable = enable;
		return 0;
	}
	pr_info("%s: trace buffer not initialized!\n", __func__);
	return -1;
}
