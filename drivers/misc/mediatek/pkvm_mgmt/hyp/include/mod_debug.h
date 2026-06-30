/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_PKVM_MOD_DEBUG__
#define __MTK_PKVM_MOD_DEBUG__

#define MOD_PUTS(str) {			\
	CALL_FROM_OPS(puts, str);	\
}

#define MOD_PUTS1(str, a1) {		\
	CALL_FROM_OPS(puts, str " {");	\
	CALL_FROM_OPS(putx64, (u64)a1);	\
	CALL_FROM_OPS(puts, "}");	\
}

#define MOD_PUTS2(str, a1, a2) {	\
	CALL_FROM_OPS(puts, str " {");	\
	CALL_FROM_OPS(putx64, (u64)a1);	\
	CALL_FROM_OPS(putx64, (u64)a2);	\
	CALL_FROM_OPS(puts, "}");	\
}

#define MOD_PUTS3(str, a1, a2, a3) {	\
	CALL_FROM_OPS(puts, str " {");	\
	CALL_FROM_OPS(putx64, (u64)a1);	\
	CALL_FROM_OPS(putx64, (u64)a2);	\
	CALL_FROM_OPS(putx64, (u64)a3);	\
	CALL_FROM_OPS(puts, "}");	\
}

#define MOD_PUTS4(str, a1, a2, a3, a4) {	\
	CALL_FROM_OPS(puts, str " {");		\
	CALL_FROM_OPS(putx64, (u64)a1);		\
	CALL_FROM_OPS(putx64, (u64)a2);		\
	CALL_FROM_OPS(putx64, (u64)a3);		\
	CALL_FROM_OPS(putx64, (u64)a4);		\
	CALL_FROM_OPS(puts, "}");		\
}

#define MOD_PUTS5(str, a1, a2, a3, a4, a5) {	\
	CALL_FROM_OPS(puts, str " {");		\
	CALL_FROM_OPS(putx64, (u64)a1);		\
	CALL_FROM_OPS(putx64, (u64)a2);		\
	CALL_FROM_OPS(putx64, (u64)a3);		\
	CALL_FROM_OPS(putx64, (u64)a4);		\
	CALL_FROM_OPS(putx64, (u64)a5);		\
	CALL_FROM_OPS(puts, "}");		\
}

#define MOD_PUTS6(str, a1, a2, a3, a4, a5, a6) {	\
	CALL_FROM_OPS(puts, str " {");			\
	CALL_FROM_OPS(putx64, (u64)a1);			\
	CALL_FROM_OPS(putx64, (u64)a2);			\
	CALL_FROM_OPS(putx64, (u64)a3);			\
	CALL_FROM_OPS(putx64, (u64)a4);			\
	CALL_FROM_OPS(putx64, (u64)a5);			\
	CALL_FROM_OPS(putx64, (u64)a6);			\
	CALL_FROM_OPS(puts, "}");			\
}

#ifdef CONFIG_TRACING
# include <nvhe/trace.h>

# define trace___hyp_printk(fmt_id, a, b, c, d) CALL_FROM_OPS(tracing_mod_hyp_printk, fmt_id, a, b, c, d)

# define __MOD_PUTS_V2_0(str, arg) \
	trace_hyp_printk(str)
# define __MOD_PUTS_V2_1(str, a1) \
	trace_hyp_printk(str " {0x%llx}", (u64)a1)
# define __MOD_PUTS_V2_2(str, a1, a2) \
	trace_hyp_printk(str " {0x%llx, 0x%llx}", (u64)a1, (u64)a2)
# define __MOD_PUTS_V2_3(str, a1, a2, a3) \
	trace_hyp_printk(str " {0x%llx, 0x%llx, 0x%llx}", (u64)a1, (u64)a2, (u64)a3)
# define __MOD_PUTS_V2_4(str, a1, a2, a3, a4) \
	trace_hyp_printk(str " {0x%llx, 0x%llx, 0x%llx, 0x%llx}", (u64)a1, (u64)a2, (u64)a3, (u64)a4)
#else /* !CONFIG_TRACING */
# define __MOD_PUTS_V2_0(str, arg) \
	MOD_PUTS1(str)
# define __MOD_PUTS_V2_1(str, a1) \
	MOD_PUTS1(str, a1)
# define __MOD_PUTS_V2_2(str, a1, a2) \
	MOD_PUTS2(str, a1, a2)
# define __MOD_PUTS_V2_3(str, a1, a2, a3) \
	MOD_PUTS3(str, a1, a2, a3)
# define __MOD_PUTS_V2_4(str, a1, a2, a3, a4) \
	MOD_PUTS4(str, a1, a2, a3, a4)
#endif /* CONFIG_TRACING */

#define __MOD_PUTS_V2_N(str, ...) \
	CONCATENATE(__MOD_PUTS_V2_, COUNT_ARGS(__VA_ARGS__))(str, ##__VA_ARGS__)

#define MOD_PUTS_V2(str, ...) \
	__MOD_PUTS_V2_N(str, __VA_ARGS__)

#endif /* __MTK_PKVM_MOD_DEBUG__ */
