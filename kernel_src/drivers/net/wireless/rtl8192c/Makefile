EXTRA_CFLAGS += $(USER_EXTRA_CFLAGS)
#EXTRA_CFLAGS += -O3
#EXTRA_CFLAGS += -Wall
#EXTRA_CFLAGS += -Wextra
#EXTRA_CFLAGS += -Werror
#EXTRA_CFLAGS += -pedantic
#EXTRA_CFLAGS += -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes

EXTRA_CFLAGS += -Wno-unused-variable
EXTRA_CFLAGS += -Wno-unused-value
EXTRA_CFLAGS += -Wno-unused-label
EXTRA_CFLAGS += -Wno-unused-parameter
EXTRA_CFLAGS += -Wno-unused-function
EXTRA_CFLAGS += -Wno-unused

EXTRA_CFLAGS += -Wno-uninitialized

EXTRA_CFLAGS += -I$(src)/include

CONFIG_AUTOCFG_CP = n

CONFIG_RTL8192C = y
CONFIG_RTL8192D = n
CONFIG_RTL8723A = n

CONFIG_USB_HCI = y
CONFIG_PCI_HCI = n
CONFIG_SDIO_HCI = n

CONFIG_MP_INCLUDED = n
CONFIG_POWER_SAVING = n
CONFIG_USB_AUTOSUSPEND = n
CONFIG_HW_PWRP_DETECTION = n
CONFIG_WIFI_TEST = y
CONFIG_BT_COEXISTENCE = n
CONFIG_RTL8192CU_REDEFINE_1X1 = n
CONFIG_INTEL_WIDI = n
CONFIG_WAKE_ON_WLAN = n

CONFIG_DRVEXT_MODULE = n

export TopDIR ?= $(shell pwd)


ifeq ($(CONFIG_RTL8192C), y)

RTL871X = rtl8192c

ifeq ($(CONFIG_USB_HCI), y)
MODULE_NAME = 8192cu
FW_FILES := hal/$(RTL871X)/usb/Hal8192CUHWImg.o
ifneq ($(CONFIG_WAKE_ON_WLAN), n)
FW_FILES += hal/$(RTL871X)/usb/Hal8192CUHWImg_wowlan.o
endif
endif
ifeq ($(CONFIG_PCI_HCI), y)
MODULE_NAME = 8192ce
FW_FILES := hal/$(RTL871X)/pci/Hal8192CEHWImg.o
endif

CHIP_FILES := \
	hal/$(RTL871X)/$(RTL871X)_sreset.o \
	hal/$(RTL871X)/$(RTL871X)_xmit.o
CHIP_FILES += $(FW_FILES)
endif

ifeq ($(CONFIG_RTL8192D), y)

RTL871X = rtl8192d

ifeq ($(CONFIG_USB_HCI), y)
MODULE_NAME = 8192du
FW_FILES := hal/$(RTL871X)/usb/Hal8192DUHWImg.o
ifneq ($(CONFIG_WAKE_ON_WLAN), n)
FW_FILES += hal/$(RTL871X)/usb/Hal8192DUHWImg_wowlan.o
endif
endif
ifeq ($(CONFIG_PCI_HCI), y)
MODULE_NAME = 8192de
FW_FILES := hal/$(RTL871X)/pci/Hal8192DEHWImg.o
endif

CHIP_FILES := \
	hal/$(RTL871X)/$(RTL871X)_xmit.o
CHIP_FILES += $(FW_FILES)
endif

ifeq ($(CONFIG_RTL8723A), y)

RTL871X = rtl8723a

ifeq ($(CONFIG_SDIO_HCI), y)
MODULE_NAME = 8723as
FW_FILES := hal/$(RTL871X)/sdio/Hal8723SHWImg.o
endif

ifeq ($(CONFIG_USB_HCI), y)
MODULE_NAME = 8723au
FW_FILES := hal/$(RTL871X)/usb/Hal8723UHWImg.o
endif

ifeq ($(CONFIG_PCI_HCI), y)
MODULE_NAME = 8723ae
FW_FILES := hal/$(RTL871X)/pci/Hal8723EHWImg.o
endif

PWRSEQ_FILES := hal/HalPwrSeqCmd.o \
				hal/$(RTL871X)/Hal8723PwrSeq.o

CHIP_FILES += $(FW_FILES) $(PWRSEQ_FILES)

endif

ifeq ($(CONFIG_SDIO_HCI), y)
HCI_NAME = sdio
endif

ifeq ($(CONFIG_USB_HCI), y)
HCI_NAME = usb
endif

ifeq ($(CONFIG_PCI_HCI), y)
HCI_NAME = pci
endif


_OS_INTFS_FILES :=	os_dep/osdep_service.o \
			os_dep/linux/os_intfs.o \
			os_dep/linux/$(HCI_NAME)_intf.o \
			os_dep/linux/$(HCI_NAME)_ops_linux.o \
			os_dep/linux/ioctl_linux.o \
			os_dep/linux/xmit_linux.o \
			os_dep/linux/mlme_linux.o \
			os_dep/linux/recv_linux.o \
			os_dep/linux/ioctl_cfg80211.o \
			os_dep/linux/rtw_android.o


_HAL_INTFS_FILES :=	hal/hal_intf.o \
			hal/hal_com.o \
			hal/dm.o \
			hal/$(RTL871X)/$(RTL871X)_hal_init.o \
			hal/$(RTL871X)/$(RTL871X)_phycfg.o \
			hal/$(RTL871X)/$(RTL871X)_rf6052.o \
			hal/$(RTL871X)/$(RTL871X)_dm.o \
			hal/$(RTL871X)/$(RTL871X)_rxdesc.o \
			hal/$(RTL871X)/$(RTL871X)_cmd.o \
			hal/$(RTL871X)/$(HCI_NAME)/$(HCI_NAME)_halinit.o \
			hal/$(RTL871X)/$(HCI_NAME)/rtl$(MODULE_NAME)_led.o \
			hal/$(RTL871X)/$(HCI_NAME)/rtl$(MODULE_NAME)_xmit.o \
			hal/$(RTL871X)/$(HCI_NAME)/rtl$(MODULE_NAME)_recv.o

ifeq ($(CONFIG_SDIO_HCI), y)
_HAL_INTFS_FILES += hal/$(RTL871X)/$(HCI_NAME)/$(HCI_NAME)_ops.o
else
_HAL_INTFS_FILES += hal/$(RTL871X)/$(HCI_NAME)/$(HCI_NAME)_ops_linux.o
endif

ifeq ($(CONFIG_MP_INCLUDED), y)
_HAL_INTFS_FILES += hal/$(RTL871X)/$(RTL871X)_mp.o
endif

_HAL_INTFS_FILES += $(CHIP_FILES)


ifeq ($(CONFIG_AUTOCFG_CP), y)
$(shell cp $(TopDIR)/autoconf_$(RTL871X)_$(HCI_NAME)_linux.h $(TopDIR)/include/autoconf.h)
endif


ifeq ($(CONFIG_USB_HCI), y)
ifeq ($(CONFIG_USB_AUTOSUSPEND), y)
EXTRA_CFLAGS += -DCONFIG_USB_AUTOSUSPEND
endif
endif

ifeq ($(CONFIG_POWER_SAVING), y)
EXTRA_CFLAGS += -DCONFIG_POWER_SAVING
endif

ifeq ($(CONFIG_HW_PWRP_DETECTION), y)
EXTRA_CFLAGS += -DCONFIG_HW_PWRP_DETECTION
endif

ifeq ($(CONFIG_WIFI_TEST), y)
EXTRA_CFLAGS += -DCONFIG_WIFI_TEST
endif

ifeq ($(CONFIG_BT_COEXISTENCE), y)
EXTRA_CFLAGS += -DCONFIG_BT_COEXISTENCE
endif

ifeq ($(CONFIG_RTL8192CU_REDEFINE_1X1), y)
EXTRA_CFLAGS += -DRTL8192C_RECONFIG_TO_1T1R
endif

ifeq ($(CONFIG_WAKE_ON_WLAN), y)
EXTRA_CFLAGS += -DCONFIG_WAKE_ON_WLAN
endif

ifeq ($(CONFIG_INTEL_WIDI), y)
EXTRA_CFLAGS += -DCONFIG_INTEL_WIDI
endif

ifeq ($(CONFIG_ANDROID), y)
EXTRA_CFLAGS += -DCONFIG_PLATFORM_ANDROID -DCONFIG_PLATFORM_SHUTTLE
EXTRA_CFLAGS += -DRTW_ENABLE_WIFI_CONTROL_FUNC
endif
EXTRA_CFLAGS += -DCONFIG_LITTLE_ENDIAN 

ifneq ($(USER_MODULE_NAME),)
MODULE_NAME := $(USER_MODULE_NAME)
endif

ifeq ($(CONFIG_MP_INCLUDED), y)
MODULE_NAME := $(MODULE_NAME)_mp
EXTRA_CFLAGS += -DCONFIG_MP_INCLUDED
endif




rtk_core :=	core/rtw_cmd.o \
		core/rtw_security.o \
		core/rtw_debug.o \
		core/rtw_io.o \
		core/rtw_ioctl_query.o \
		core/rtw_ioctl_set.o \
		core/rtw_ieee80211.o \
		core/rtw_mlme.o \
		core/rtw_mlme_ext.o \
		core/rtw_wlan_util.o \
		core/rtw_pwrctrl.o \
		core/rtw_rf.o \
		core/rtw_recv.o \
		core/rtw_sta_mgt.o \
		core/rtw_ap.o \
		core/rtw_xmit.o	\
		core/rtw_p2p.o \
		core/rtw_tdls.o \
		core/rtw_br_ext.o \
		core/rtw_iol.o \
		core/rtw_sreset.o

$(MODULE_NAME)-y += $(rtk_core)

$(MODULE_NAME)-$(CONFIG_INTEL_WIDI) += core/rtw_intel_widi.o

$(MODULE_NAME)-y += core/efuse/rtw_efuse.o

$(MODULE_NAME)-y += $(_HAL_INTFS_FILES)

$(MODULE_NAME)-y += $(_OS_INTFS_FILES)

$(MODULE_NAME)-$(CONFIG_MP_INCLUDED) += core/rtw_mp.o \
					core/rtw_mp_ioctl.o

obj-$(CONFIG_RTL8192CU) := $(MODULE_NAME).o



