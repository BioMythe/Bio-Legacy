BUILD_PATH ?= Build
TARGET_BIN ?= BIOBoot.bin

# The assembly unit to assemble into an actual binary.
# All other assembly units are just to be included in the main one.
MAIN_ASSEMBLY_UNIT ?= BIOBoot.asm

RM    ?= rm
ASM   ?= nasm
ECHO  ?= echo
MKDIR ?= mkdir

$(BUILD_PATH)/$(TARGET_BIN): $(MAIN_ASSEMBLY_UNIT) *.asm
	@$(MKDIR) -p $(dir $@)
	@$(ECHO) Assembling bootloader binary $@
	@$(ASM) -f bin $< -o $@
