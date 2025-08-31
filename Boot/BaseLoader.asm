; This are of the bootloader is post the 512-byte mark. The first 512 bytes of the bootloader immediately tries to jump here
; with proper configuration to avoid the 510 byte limit due to the 0xAA55 magic signature.

; Contain the VALIDATION HEADER, MBR will check for this and jump to actual code as LOADED+HEADERSIZE, right below this line of code.
db BOOTLOADER_VALIDATION_HEADER

EarlyBoot_Realenv: ; Early Loader, Real Environment
; Let's notify user about bootloader loading success.
mov si, MSG_NOTIFY_LOADED_BOOTLODAER
call Print

; Ensure A20 is enabled, enable if necessary and halt if fail.
call EnsureA20

; Notify jump.
mov si, MSG_NOTIFY_JUMPING_TO_PROTECT
call Print

; Long live Protected Mode! Your purpose shall be to ascend your noteworthy successor Long Mode from his enclosure.
cli ; Step 1 of Entering Protected Mode - Clear Interrupts
    ; Step 2 of Entering Protected Mode - Enable A20, we already ensured it with a call to EnsureA20.
lgdt [GDTR32] ; Step 3 - Load the Global Descriptor Table

; Step 4 - Toggle the Protected Mode Enable bit on CR0. It is the least significant bit, so we can just and with 0x1 to mask it.
mov eax, cr0
or al, 0x1
mov cr0, eax

; Step 5 - FAR JUMP TO 32-BIT CODE.
jmp GDT32_CODE_SEGMENT:EarlyBoot_Protenv

%include "Boot/Realenv/GDT32.asm"
%include "Boot/Realenv/A20.asm"

MSG_NOTIFY_LOADED_BOOTLODAER:  db "The Biological Bootloader (BIOBoot) has been fully loaded into memory, post 512-bytes achieved.", 0xA, 0x0
MSG_NOTIFY_JUMPING_TO_PROTECT: db "Jumping to Protected Mode. If your system hangs for a long time, it probably means that an error has occured, which may be along the lines of:", 0xA, "-> Your CPU doesn't support necessary instructions (CPUID).", 0xA, "-> Your CPU doesn't support Long Mode, as BIO is a 64-bit operating system.", 0xA, 0x0

[bits 32]

EarlyBoot_Protenv: ; Early Loader, Protected Environment
    ; Update all segment registers to our GDT's data segment.
    ; Otherwise, CPU thinks they are real mode registers and we'll probably triple fault at some point.
    mov ax, GDT32_DATA_SEGMENT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Setup the stack.
    mov ebp, 0x1000000
    mov esp, ebp

    ; Make sure the CPU supports the CPUID instruction.
    call CheckCPUID
    cmp eax, 0
    je Halt32

    ; Get Extended CPUID Feature Flags.
    mov eax, 0x80000001
    cpuid

    ; EDX bit 29 contains Long Mode support bit.
    ; Only testing this is enough, PAE bit is always set of LM bit is set too, since it's a prerequisite.
    test edx, 1 << 29
    jz Halt32
    
    ; Initialialize page structures.
    call InitPageStructure

    ; Enable PAE (Physical Address Extension). It is bit 5 in Control Register 4.
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable Long Mode.
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8 ; Bit 8 = Long Mode Enable
    wrmsr
    ; This puts us in "Compatability Mode". To get to the real Long Mode, we need to load a 64-bit GDT.

    ; Enable paging.
    mov eax, cr0
    or eax, 1 << 31 ; Bit 31 = Paging Enable
                    ; No need to set Protected Mode Enable, it is already set, duh.
    mov cr0, eax

    lgdt [GDTR64]
    jmp GDT64_CODE_SEGMENT:LateLoad

    jmp Halt32

Halt32:
    cli
    hlt
    jmp Halt32

%include "Boot/Protenv/Paging.asm"
%include "Boot/Protenv/CPUID.asm"
%include "Boot/Protenv/GDT64.asm"
