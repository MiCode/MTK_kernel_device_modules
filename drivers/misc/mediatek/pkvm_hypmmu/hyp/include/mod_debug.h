/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#define MOD_PUTS(str) {		\
	mod_ops->puts(str);	\
}

#define MOD_PUTS1(str, a1) {		\
	mod_ops->puts(str " {");	\
	mod_ops->putx64((u64)a1);	\
	mod_ops->puts("}");		\
}

#define MOD_PUTS2(str, a1, a2) {	\
	mod_ops->puts(str " {");	\
	mod_ops->putx64((u64)a1);	\
	mod_ops->putx64((u64)a2);	\
	mod_ops->puts("}");		\
}

#define MOD_PUTS3(str, a1, a2, a3) {	\
	mod_ops->puts(str " {");	\
	mod_ops->putx64((u64)a1);	\
	mod_ops->putx64((u64)a2);	\
	mod_ops->putx64((u64)a3);	\
	mod_ops->puts("}");		\
}

#define MOD_PUTS4(str, a1, a2, a3, a4) {	\
	mod_ops->puts(str " {");		\
	mod_ops->putx64((u64)a1);		\
	mod_ops->putx64((u64)a2);		\
	mod_ops->putx64((u64)a3);		\
	mod_ops->putx64((u64)a4);		\
	mod_ops->puts("}");			\
}

#define MOD_PUTS5(str, a1, a2, a3, a4, a5) {	\
	mod_ops->puts(str " {");		\
	mod_ops->putx64((u64)a1);		\
	mod_ops->putx64((u64)a2);		\
	mod_ops->putx64((u64)a3);		\
	mod_ops->putx64((u64)a4);		\
	mod_ops->putx64((u64)a5);		\
	mod_ops->puts("}");			\
}

#define MOD_PUTS6(str, a1, a2, a3, a4, a5, a6) {	\
	mod_ops->puts(str " {");			\
	mod_ops->putx64((u64)a1);			\
	mod_ops->putx64((u64)a2);			\
	mod_ops->putx64((u64)a3);			\
	mod_ops->putx64((u64)a4);			\
	mod_ops->putx64((u64)a5);			\
	mod_ops->putx64((u64)a6);			\
	mod_ops->puts("}");				\
}
