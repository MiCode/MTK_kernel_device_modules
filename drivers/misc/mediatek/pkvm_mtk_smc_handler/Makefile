obj-$(CONFIG_MTK_PKVM_MTK_SMC_HANDLER) += pkvm_mtk_smc_handler.o


$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S

pkvm_mtk_smc_handler-objs += pkvm_mtk_smc_handler_host.o hyp/kvm_nvhe.o
