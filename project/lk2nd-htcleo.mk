# top level project rules for the qsd8250_surf project
#
LOCAL_DIR := $(GET_LOCAL_DIR)

TARGET := htcleo

MODULES += app/aboot

BOARD_HTC_LEO := true

#DEFINES += WITH_DEBUG_DCC=1
#DEFINES += WITH_DEBUG_UART=1
DEFINES += WITH_DEBUG_FBCON=1


# Defines from LK2ND-base.mk

# Use maximum verbosity
DEBUG := 2
DEFINES += LK_LOG_BUF_SIZE=16384

# Disable various stupid stuff that we don't really want or need
DEFINES += DEFAULT_UNLOCK=1 DISABLE_LOCK=1 DISABLE_DEVINFO=1
DISABLE_RECOVERY_MESSAGES := 1

# Enable fastboot display menu
ENABLE_FBCON_DISPLAY_MSG := 1

ifeq ($(DISPLAY_USE_CONTINUOUS_SPLASH)$(LK1ST_PANEL),)
    $(info NOTE: Display support is disabled without panel/display selection)
    DEFINES := $(filter-out DISPLAY_SPLASH_SCREEN=1, $(DEFINES))
    ENABLE_FBCON_DISPLAY_MSG := 0
endif

# Some old platforms are missing the proper defines
ifeq ($(ENABLE_FBCON_DISPLAY_MSG),1)
DEFINES += FBCON_DISPLAY_MSG=1
endif
ifeq ($(EMMC_BOOT),1)
DEFINES += _EMMC_BOOT=1
endif

# Stop trying to read battery voltage, probably doesn't work on most devices...
DEFINES := $(filter-out CHECK_BAT_VOLTAGE=1, $(DEFINES))

