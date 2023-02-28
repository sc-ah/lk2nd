# top level project rules for the qsd8250_surf project
#
LOCAL_DIR := $(GET_LOCAL_DIR)

include $(LOCAL_DIR)/lk2nd-base.mk

TARGET := htcleo

MODULES += app/aboot

BOARD_HTC_LEO := true

#DEFINES += WITH_DEBUG_DCC=1
#DEFINES += WITH_DEBUG_UART=1
DEFINES += WITH_DEBUG_FBCON=1
