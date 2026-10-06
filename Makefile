# Komut satırı derlemesi (STM32CubeIDE dışında). Kullanım:
#   make                      -> build/stm_motor_kontrol.elf/.hex/.bin
#   make GCC_PATH=<arm-none-eabi bin klasörü>
TARGET   = stm_motor_kontrol
BUILD    = build

PREFIX   = $(if $(GCC_PATH),$(GCC_PATH)/,)arm-none-eabi-
CC       = $(PREFIX)gcc
OBJCOPY  = $(PREFIX)objcopy
SIZE     = $(PREFIX)size

C_SOURCES = $(wildcard Core/Src/*.c) $(wildcard Drivers/STM32G4xx_HAL_Driver/Src/*.c)
ASM_SOURCES = Core/Startup/startup_stm32g431xx.s

MCU      = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS     = -DUSE_HAL_DRIVER -DSTM32G431xx
INCS     = -ICore/Inc \
           -IDrivers/STM32G4xx_HAL_Driver/Inc \
           -IDrivers/STM32G4xx_HAL_Driver/Inc/Legacy \
           -IDrivers/CMSIS/Device/ST/STM32G4xx/Include \
           -IDrivers/CMSIS/Include

CFLAGS   = $(MCU) $(DEFS) $(INCS) -Os -g3 -std=gnu11 -Wall -Wextra \
           -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS  = $(MCU) -TSTM32G431RBTX_FLASH.ld --specs=nano.specs --specs=nosys.specs \
           -Wl,-Map=$(BUILD)/$(TARGET).map,--cref -Wl,--gc-sections -Wl,--print-memory-usage

OBJS = $(addprefix $(BUILD)/,$(notdir $(C_SOURCES:.c=.o))) \
       $(addprefix $(BUILD)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(CC) -c $(MCU) -x assembler-with-cpp $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)

.PHONY: all clean
