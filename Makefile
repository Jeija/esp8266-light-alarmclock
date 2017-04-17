# Esptool firmware download options
PORT			?= /dev/ttyUSB0
BAUD			:= 921600

# WiFi Settings and TimezoneDB API key
# Do NOT modify these placeholders, but include your configuration
# in your make commandline:
# make WIFI_SSID="YOURSSID" WIFI_PASS="YOURPASS" TIMEZONEDB_KEY="YOURKEY"
WIFI_SSID		:= placeholder
WIFI_PASS		:= placeholder
TIMEZONEDB_KEY		:= placeholder

# Directory structure
BUILDDIR		:= build
SRCDIR			:= src
OBJDIR			:= obj

# libesphttpd settings
LIBESPHTTPD_DIR		:= libesphttpd
ESPHTTPD_LDSCRIPT	:= $(OBJDIR)/ldscript_memspecific.ld
LIBESPHTTPD		:= $(LIBESPHTTPD_DIR)/libesphttpd.a

# Project settings
TARGET			:= firmware
FLASH_FREQ		:= 80m
FLASH_MODE		:= qio
FLASH_SIZE		:= 512
FLASH_SIZE_BIT		:= 4m
FLASH_OPT		:= --flash_freq $(FLASH_FREQ) --flash_mode $(FLASH_MODE) --flash_size $(FLASH_SIZE_BIT)

# Files
SRCS			:= $(wildcard  $(SRCDIR)/*.c)
OBJS			:= $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))
DEPS			:= $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.d)))

TARGET_ELF		:= $(BUILDDIR)/$(TARGET).elf
TARGET_FLASH		:= $(BUILDDIR)/$(TARGET)-0x00000.bin
TARGET_IROM		:= $(BUILDDIR)/$(TARGET)-0x40000.bin

# SDK / Compiler settings
SDK_DIR			:= /opt/esp-open-sdk/sdk
CC			:= xtensa-lx106-elf-gcc
LD			:= xtensa-lx106-elf-gcc
ESPTOOL			:= esptool.py
CFLAGS			:= -O2 -std=c11 -Wpointer-arith -Wall -Wno-implicit-function-declaration -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals -DICACHE_FLASH
LDLIBS			:= -nostdlib -Wl,--start-group -lc -lgcc -lhal -lmain -lnet80211 -lwps -lwpa -llwip -lpp -lphy -lcrypto -lesphttpd -lwebpages-espfs $(OBJS) -Wl,--end-group
LDFLAGS			:= -T$(SDK_DIR)/ld/eagle.app.v6.ld
INCDIRS			:= -I $(SRCDIR) -I $(LIBESPHTTPD_DIR)/include -I $(SDK_DIR)/include -I $(SDK_DIR)/include/json

all: $(OBJDIR) $(BUILDDIR) $(TARGET_FLASH) $(TARGET_IROM)
	@echo Compilation successful
	@echo Use \'make flash\' to flash firmware

$(TARGET_FLASH) $(TARGET_IROM): $(TARGET_ELF)
	$(ESPTOOL) elf2image $(FLASH_OPT) $^ --output $(BUILDDIR)/$(TARGET)-

$(TARGET_ELF): $(OBJS) $(LIBESPHTTPD) $(ESPHTTPD_LDSCRIPT)
	$(LD) -L$(LIBESPHTTPD_DIR) -L$(SDK_DIR)/lib $(LDFLAGS) $(ESPHTTPD_LDSCRIPT) $(LDLIBS) -o $@

$(ESPHTTPD_LDSCRIPT):
	echo "MEMORY { irom0_0_seg : org = 0x40240000, len = "$$(printf "0x%X" $$(($(FLASH_SIZE)-0x4000)))" }"> $(ESPHTTPD_LDSCRIPT)

$(LIBESPHTTPD):
	SDK_BASE=$(SDK_DIR) USE_OPENSDK=yes $(MAKE) -C $(LIBESPHTTPD_DIR)
			
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(INCDIRS) $(CFLAGS) -D WIFI_SSID=\"$(WIFI_SSID)\" -D WIFI_PASS=\"$(WIFI_PASS)\" -D TIMEZONEDB_KEY=\"$(TIMEZONEDB_KEY)\" -MMD -MP -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

flash: all
	$(ESPTOOL) --port $(PORT) --baud $(BAUD) write_flash $(FLASH_OPT) 0x00000 $(TARGET_FLASH) 0x40000 $(TARGET_IROM)

.PHONY: clean

clean:
	$(RM) -r $(OBJDIR)
	$(RM) -r $(BUILDDIR)
	$(MAKE) -C $(LIBESPHTTPD_DIR) clean

-include $(DEPS)

