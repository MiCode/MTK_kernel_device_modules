obj-$(CONFIG_MTK_PKVM_SMMU) += pkvm_smmu.o

$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

#subdir-ccflags-y += -I$(srctree)/arch/arm64/kvm/hyp/include
clean-files := hyp/hyp.lds hyp/hyp-reloc.S

# pkvm_uart-y := pkvm_uart_host.o hyp/kvm_nvhe.o
pkvm_smmu-objs += pkvm_smmu_host.o hyp/kvm_nvhe.o
