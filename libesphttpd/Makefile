CWD				:= $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# SDK / Compiler settings
SDK_DIR			:= /opt/esp-open-sdk/esp_iot_sdk_v1.4.0
CC				:= xtensa-lx106-elf-gcc
OBJCOPY			:= xtensa-lx106-elf-objcopy
AR				:= xtensa-lx106-elf-ar
CFLAGS			:= -O2 -fdata-sections -ffunction-sections -g -std=c99 -Wpointer-arith -Wall -Wno-implicit-function-declaration -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals -D__ets__ -D_STDINT_H -DICACHE_FLASH

# Directory structure
TARGET			:= libesphttpd.a
TARGET_ESPFS	:= libwebpages-espfs.a
MODULES			:= espfs core util
OBJDIR			:= obj
WWWDIR			:= ../html

SRCS			:= $(foreach mod,$(MODULES),$(wildcard $(mod)/*.c))
OBJS			:= $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))
DEPS			:= $(addprefix $(OBJDIR),$(notdir $(SRCS:.c=.d)))
INCLUDES		:= -I ./include $(addprefix -I,$(MODULES))
WEBPAGES		:= webpages.espfs

all: $(OBJDIR) $(TARGET) $(TARGET_ESPFS)
	@echo libesphttpd compiled

$(TARGET): $(OBJS)
	$(AR) cr $@ $^

define compilemod
$(OBJDIR)/%.o: $1%.c
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $$< -o $$@
endef
$(foreach mod,$(sort $(dir $(SRCS))),$(eval $(call compilemod,$(mod))))

$(OBJDIR):
	mkdir -p $(OBJDIR)

### espfs ###
$(TARGET_ESPFS): $(OBJDIR)/$(WEBPAGES)
	cd $(OBJDIR) ; $(OBJCOPY) -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.irom0.literal \
		--redefine-sym _binary_webpages_espfs_start=webpages_espfs_start \
		--redefine-sym _binary_webpages_espfs_end=webpages_espfs_end \
		--redefine-sym _binary_webpages_espfs_size=webpages_espfs_size \
		$(WEBPAGES) webpages.espfs.o
	$(AR) cru $@ $(OBJDIR)/webpages.espfs.o

$(OBJDIR)/$(WEBPAGES): $(WWWDIR) espfs/mkespfsimage/mkespfsimage
	cd $(WWWDIR); find . | $(CWD)espfs/mkespfsimage/mkespfsimage > $(CWD)$(OBJDIR)/$(WEBPAGES); cd ..

espfs/mkespfsimage/mkespfsimage: espfs/mkespfsimage/
	$(MAKE) -C espfs/mkespfsimage

### cleaning ###
.PHONY: clean

clean:
	$(RM) -r $(OBJDIR) $(TARGET) $(TARGET_ESPFS)
	$(MAKE) -C espfs/mkespfsimage clean

-include $(DEPS)
