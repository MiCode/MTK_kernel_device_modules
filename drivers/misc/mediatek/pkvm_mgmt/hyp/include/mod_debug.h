/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

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
