subdir-ccflags-y += -Wall -Werror -DCONFIG_RT_REGMAP -I "drivers/power/supply/hq_chg/sc_charger/"
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
subdir-ccflags-y += -I$(srctree)/drivers/mfd/
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_class/

obj-$(CONFIG_DUAL_ROLE_USB_INTF)	+= tcpci_dual_role.o class-dual-role.o
obj-$(CONFIG_TCPC_SC660X)		+= tcpc_sc660x.o
obj-$(CONFIG_TCPC_NU660X)		+= tcpc_nu6601.o
obj-$(CONFIG_TCPC_SC660X)		+= rt-regmap.o
obj-$(CONFIG_TCPC_RT1711H)		+= tcpc_ba41a.o

tcpc_class-objs := tcpci_core.o tcpci_typec.o tcpci_timer.o \
		tcpm.o tcpci.o tcpci_alert.o

ifdef CONFIG_USB_POWER_DELIVERY
tcpc_class-objs += tcpci_event.o \
		pd_core.o pd_policy_engine.o pd_process_evt.o \
		pd_dpm_core.o pd_dpm_uvdm.o pd_dpm_alt_mode_dp.o pd_dpm_pdo_select.o\
		pd_dpm_reaction.o \
		pd_process_evt_snk.o pd_process_evt_src.o pd_process_evt_vdm.o \
		pd_process_evt_drs.o pd_process_evt_prs.o pd_process_evt_vcs.o \
		pd_process_evt_dbg.o pd_process_evt_tcp.o pd_process_evt_com.o \
		pd_policy_engine_src.o pd_policy_engine_snk.o pd_policy_engine_ufp.o pd_policy_engine_vcs.o \
		pd_policy_engine_dfp.o pd_policy_engine_dr.o pd_policy_engine_drs.o pd_policy_engine_prs.o \
		pd_policy_engine_dbg.o pd_policy_engine_com.o pd_dpm_alt_mode_dc.o
endif
obj-$(CONFIG_TCPC_CLASS) += tcpc_class.o pd_dbg_info.o rt_pd_manager.o tcpci_late_sync.o
