TARGET  := OneNet_Mini_Bare
BUILD   := build
OBJDIR  := $(BUILD)/obj
OUTDIR  := output/gcc

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

CPU     := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFS    := -DSTM32F10X_HD -DUSE_STDPERIPH_DRIVER
INCS    := -Icore -Ifwlib/inc -Ihardware/inc
ifeq ($(RELEASE),1)
OPT     := -Os -g0
else
OPT     := -O0 -g3
endif
CFLAGS  := $(CPU) $(DEFS) $(INCS) $(OPT) -ffunction-sections -fdata-sections -Wall
ASFLAGS := $(CPU) $(DEFS) $(INCS)
LDFLAGS := $(CPU) -T linker/STM32F103RE.ld -Wl,--gc-sections \
           -Wl,-Map=$(OUTDIR)/$(TARGET).map \
           -specs=nano.specs -specs=nosys.specs

C_SRCS := \
 core/core_cm3.c \
 core/system_stm32f10x.c \
 core/stm32f10x_it.c \
 core/syscalls.c \
 fwlib/src/misc.c \
 fwlib/src/stm32f10x_gpio.c \
 fwlib/src/stm32f10x_rcc.c \
 fwlib/src/stm32f10x_usart.c \
 hardware/src/delay.c \
 hardware/src/usart.c \
 user/main.c

ASM_SRCS := startup/startup_stm32f103xe.s

C_OBJS   := $(patsubst %.c,$(OBJDIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.s,$(OBJDIR)/%.o,$(ASM_SRCS))
OBJS     := $(C_OBJS) $(ASM_OBJS)

ELF := $(OUTDIR)/$(TARGET).elf
HEX := $(OUTDIR)/$(TARGET).hex
BIN := $(OUTDIR)/$(TARGET).bin
EMU_ELF := $(BUILD)/emu/$(TARGET).elf

.PHONY: all clean size emu

all: $(HEX) $(BIN)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -x assembler-with-cpp -c $< -o $@

$(ELF): $(OBJS) linker/STM32F103RE.ld
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

size: $(ELF)
	$(SIZE) $(ELF)

# Optional: run the image in QEMU's STM32F100 model (8 KB RAM).
# The normal STM32F103RE image uses STM32F103RE.ld and must not be used here.
$(EMU_ELF): $(OBJS) linker/STM32F100_EMU.ld
	@mkdir -p $(dir $@)
	$(CC) $(CPU) -T linker/STM32F100_EMU.ld -Wl,--gc-sections \
	       -specs=nano.specs -specs=nosys.specs -o $@ $(OBJS)

emu: $(EMU_ELF)
	qemu-system-arm -M stm32vldiscovery -kernel $(EMU_ELF) \
	                -nographic -serial stdio -monitor none

clean:
	rm -rf $(BUILD) $(ELF) $(HEX) $(BIN) $(OUTDIR)/$(TARGET).map
