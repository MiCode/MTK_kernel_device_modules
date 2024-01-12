# SPDX-License-Identifier: GPL-2.0

$(info "Welcome to MiTee...")
subdir-ccflags-y  += -I$(srctree)/$(src)/tee
obj-m += mitee.o
mitee-y :=	\
	core.o \
	call.o \
	rpc.o \
	supp.o \
	smc_abi.o \
	ffa_abi.o \
	device.o \
	bench.o \
	mitee_memlog.o \
	tee/tee_core.o \
	tee/tee_shm.o \
	tee/tee_shm_pool.o
