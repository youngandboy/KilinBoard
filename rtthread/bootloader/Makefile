# Minimal STM32F103 bootloader for kyLin-V3.1.
# Build inside WSL: make

ROOT    := ../..
CORE    := $(ROOT)/core
FWLIB   := $(ROOT)/fwlib
STARTUP := $(ROOT)/startup/startup_stm32f103xe.s
BUILD   := build
TARGET  := $(BUILD)/bootloader

CFLAGS  := -mcpu=cortex-m3 -mthumb -Os -g3 -ffunction-sections -fdata-sections \
           -Wall -Wextra -DSTM32F10X_HD -DUSE_STDPERIPH_DRIVER \
           -DHSE_VALUE=12000000 -I. -I.. -I$(CORE) -I$(FWLIB)/inc
LDFLAGS := -T link.ld -nostdlib -Wl,--gc-sections \
           -Wl,-Map=$(TARGET).map -Wl,--print-memory-usage -lgcc

C_SRCS := \
    main.c \
    boot_time.c \
    boot_uart.c \
    boot_flash.c \
    boot_ymodem.c \
    $(CORE)/core_cm3.c \
    $(CORE)/system_stm32f10x.c \
    $(FWLIB)/src/stm32f10x_rcc.c \
    $(FWLIB)/src/stm32f10x_gpio.c \
    $(FWLIB)/src/stm32f10x_usart.c \
    $(FWLIB)/src/stm32f10x_flash.c \
    $(FWLIB)/src/stm32f10x_iwdg.c

ASM_SRCS := $(STARTUP)

OBJS := $(addprefix $(BUILD)/,$(notdir $(C_SRCS:.c=.o))) \
        $(addprefix $(BUILD)/,$(notdir $(ASM_SRCS:.s=.o)))

vpath %.c $(sort $(dir $(C_SRCS)))
vpath %.s $(sort $(dir $(ASM_SRCS)))

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	arm-none-eabi-gcc $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	arm-none-eabi-gcc $(CFLAGS) -x assembler-with-cpp -c $< -o $@

$(TARGET).elf: $(OBJS) link.ld
	arm-none-eabi-gcc $(OBJS) $(LDFLAGS) -o $@
	arm-none-eabi-size $@

$(TARGET).hex: $(TARGET).elf
	arm-none-eabi-objcopy -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	arm-none-eabi-objcopy -O binary $< $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
