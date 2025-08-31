BOOT_PATH  ?= Boot
TOOLS_PATH ?= Tools

BUILD_PATH         ?= Binaries
BOOT_BUILD_PATH    ?= $(BUILD_PATH)/$(BOOT_PATH)
TOOLS_BUILD_PATH   ?= $(BUILD_PATH)/$(TOOLS_PATH)

FS_BLOCK_SIZE      ?= 4096 # For the Myth Filesystem.
OS_IMAGE_SIZE      ?= 512  # In MiB!
BOOTLOADER_SIZE    := 8192 # Raw byte size of the bootloader. Value taken from Bootloader.asm. Be sure to update in both places if changed.
BOOTLOADER_BLOCKS  := $(shell echo $$(( ($(BOOTLOADER_SIZE) / $(FS_BLOCK_SIZE)) ? ($(BOOTLOADER_SIZE) / $(FS_BLOCK_SIZE)) : 1 )))
OS_MEMORY_SIZE     ?= 512M # RAM size

OS_IMAGE           ?= $(BUILD_PATH)/BIO.img
BOOTLOADER_NAME    ?= BIOBoot
BOOTLOADER_UNIT    ?= $(BOOT_PATH)/$(BOOTLOADER_NAME).asm
BOOTLOADER_BINARY  ?= $(BOOT_BUILD_PATH)/$(BOOTLOADER_NAME).bin
BOOTLOADER_SOURCES := $(shell find $(BOOT_PATH) -name *.asm)

RM    ?= rm
CP    ?= cp
DD    ?= dd
ASM   ?= nasm
ECHO  ?= echo
QEMU  ?= qemu-system-x86_64
MYTH  ?= $(TOOLS_BUILD_PATH)/Myth
MKDIR ?= mkdir

os-image: $(OS_IMAGE)
$(OS_IMAGE): tools boot
	@$(MKDIR) -p $(BUILD_PATH)
# CREATE OS IMAGE
	@$(ECHO) Creating OS image...
	@$(DD) if=/dev/zero of=$(OS_IMAGE) bs=1M count=$(OS_IMAGE_SIZE)
# WRITE BOOTLOADER
	@$(ECHO) Writing bootloader sectors onto OS image...
	@$(DD) if=$(BOOTLOADER_BINARY) of=$(OS_IMAGE) conv=notrunc bs=1 count=$(BOOTLOADER_SIZE)
# WRITE FILESYSTEM
	@$(ECHO) Making Myth Filesytem on OS image...
	@$(MYTH) MakeFS $(OS_IMAGE) $(FS_BLOCK_SIZE) $(BOOTLOADER_BLOCKS) "BIO Operating System"

run: os-image
	@$(ECHO) Booting up QEMU instance using the OS image...
	@$(QEMU) -monitor stdio -m $(OS_MEMORY_SIZE) -drive format=raw,file=$(OS_IMAGE),if=virtio

boot: $(BOOTLOADER_BINARY)
$(BOOTLOADER_BINARY): $(BOOTLOADER_UNIT) $(BOOTLOADER_SOURCES)
	@$(MKDIR) -p $(BOOT_BUILD_PATH)
	@$(ECHO) Compiling BIOBoot: '$<' to '$@'
	@$(ASM) $< -f bin -o $@ 

tools: myth

myth:
	@$(MAKE) -C $(TOOLS_PATH)/Myth BUILD_PATH=$(abspath $(TOOLS_BUILD_PATH))

clean:
	@$(RM) -rf $(BUILD_PATH)
	@$(ECHO) Deleted $(BUILD_PATH) directory.
