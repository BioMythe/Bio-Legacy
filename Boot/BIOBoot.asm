; This is the main bootloader assembly unit that will be turned into a raw bootloader binary file.
;

; Common definitions between MBR and BaseLoader.
%define BOOTLOADER_ORIGIN                  0x7C00    ; The address that BIOS loads the MBR to.
%define BOOTLOADER_VALIDATION_HEADER      "BIOLOAD!" ; Magic identifier for post 512 byte mark.
%define NUMBER_OF_BOOTLOADER_BYTES         0x2000    ; 8KB of space allocated for the bootloader.
                                                     ; The binary file is to be padded up to this amount of bytes.
%define LOAD_ADDRESS                       0x7E00    ; Address to load the rest of the bootloader to.
                                                     ; 0x7C00 + 512, the post 512 byte mark address.
%define BOOTLOADER_VALIDATION_TAIL        "MYTHICAL" ; Bootloader verification value at the end of the binary, like header but at the end.
%define BOOTLOADER_VALIDATION_SIZE         0x8       ; Size of both BOOTLOADER_VALIDATION_HEADER and BOOTLOADER_VALIDATION_SIZE strings.

%include "Boot/PreBoot.asm"    ; Include the Master Boot Record first.
%include "Boot/BaseLoader.asm" ; Bootstrap stage loader. Switches from Real Mode to Protected Mode and to Long Mode.
%include "Boot/LateLoader.asm" ; Long Mode stage loader. Parses the file system and loads the kernel.

times (NUMBER_OF_BOOTLOADER_BYTES-BOOTLOADER_VALIDATION_SIZE)-($-$$) db 0 ; Pad image with zeroes.
; The validation tail.
db BOOTLOADER_VALIDATION_TAIL
