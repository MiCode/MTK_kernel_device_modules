# SPDX-License-Identifier: GPL-2.0

$(info "CONFIG_MITEE_SUPPORT=$(CONFIG_MITEE_SUPPORT)")

$(info "Welcome to MiTee...")
obj-$(CONFIG_MITEE_SUPPORT) += mitee.o
mitee-y :=	src/bench.o \
			src/call.o \
			src/core.o \
			src/device.o \
			src/mitee_memlog.o \
			src/rpc.o \
			src/bench.o \
			src/shm_pool.o \
			src/supp.o \
			src/tee/tee_core.o \
			src/tee/tee_shm_pool.o \
			src/tee/tee_shm.o
