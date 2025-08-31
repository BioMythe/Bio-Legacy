;--
; BIO LateLoader (BIO LateBoot)
; Long Mode stage of BIOBoot.
; It has one ultimate goal to achieve:
; !!! To parse the file system and to load the kernel into memory and to jump there there. !!!
;--
[bits 64]

LateLoad: ; Long Environment
    ; Update segment registers.
    mov ax, GDT64_DATA_SEGMENT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Setup stack.
    mov rbp, 0x9000
    mov rsp, rbp

    call VGA_Clear
    mov rsi, STR_BOOTLOADER_LONG
    call VGA_Print

    mov rdi, 0x10000
    call IdentifyATA

    cmp rax, 0
    jne IdentifyFail

    mov rsi, STR_ATA_SUCCESS_ID
    call VGA_Print

    jmp Halt64

IdentifyFail:
    cmp rax, 1
    je .NoDevice
    cmp rax, 2
    je .DevErr
    cmp rax, 3
    je .Timeout

    mov rsi, STR_ATA_WTF
    jmp .Do

    .NoDevice:
        mov rsi, STR_ATA_NO_DEVICE
        jmp .Do
    .DevErr:
        mov rsi, STR_ATA_DEVICE_ERROR
        jmp .Do
    .Timeout:
        mov rsi, STR_ATA_DRQ_TIMEOUT
        jmp .Do

    .Do:
        call VGA_Print
        jmp Halt64

Halt64:
    cli
    hlt
    jmp Halt64

%include "Boot/Drivers/ATA.asm"
%include "Boot/Drivers/VGA.asm"

STR_BOOTLOADER_LONG:  db "BIOBoot has successfully entered Long Mode. Now operating in 64-bits, welcome home.", 0xA, 0x0
STR_ATA_NO_DEVICE:    db "No ATA device found.", 0xA, 0x0
STR_ATA_DEVICE_ERROR: db "An error with the ATA device occurred.", 0xA, 0x0
STR_ATA_DRQ_TIMEOUT:  db "ATA device wait for DRQ to be set timed out.", 0xA, 0x0
STR_ATA_WTF:          db "ATA error: what the fuck?", 0xA, 0x0
STR_ATA_SUCCESS_ID:   db "The ATA device has been successfully identified.", 0xA, 0x0

