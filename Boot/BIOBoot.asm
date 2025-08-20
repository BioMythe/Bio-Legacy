%define BOOTLOADER_VALIDATION_HEADER      "BIOLOAD!" ; Magic identifier for post 512 byte mark.
%define BOOTLOADER_VALIDATION_HEADER_SIZE  0x8       ; Size of the string above.
%define NUMBER_OF_BOOTLOADER_BYTES         0x2000    ; 8KB of space allocated for the bootloader.
                                                     ; The binary file is to be padded up to this amount of bytes.
%define SBOOT_ADDRESS                      0x7E00    ; SBOOT (Secondary Boot) part of the bootloader.
                                                     ; 0x7C00 + 512, the post 512 byte mark address.

;; SECTOR 1: BOOT SECTOR (MBR, LBA 0x0)
[bits 16]
[org 0x7C00]

; Myth File System Configuration Chunk
jmp short Realenv ; 2 bytes for short jump to bootloader code.
dd 0              ; Four byte magic string "MYTH"
dw 0              ; Bytes per file system block.
dq 0              ; Metadata block address in file system block addressing.
; None of the Configuration Chunk values (except JMP SHORT) are given here, because
; the Myth File System Tool will write those when it's invoked during the MakeFS phase.
; Only the short jump is here because, well, MakeFS isn't able to do that and won't overwrite that field.

; Code of BL begins here, the first ever instruction JMP SHORT jumps right here, skipping past the FS Configuration Chunk.
Realenv: ; Real Environment

; Immediately save DL to BOOT_DRIVE.
mov [BOOT_DRIVE], dl

; Stack setup.
mov bp, 0xB000
mov sp, bp

; AMD recommends reporting the execution mode of the OS to the BIOS for it to
; properly optimize the system for it. This is recommended so we'll do it.
mov ax, 0xEC00 ; BIOS function
mov bl, 2      ; Values: 1 -> Protected Mode, 2 -> Long Mode, 3 -> Both. BIO is a 64-bit OS so we choose 2.
int 0x15       ; Interrupt to notify BIOS.

; Check if extended 13h functions are supported
mov ah, 0x41
mov dl, [BOOT_DRIVE]
mov bx, 0x55AA
int 0x13
jc NoExtendedFunctionality

; CX contains the bitmask fields after INT 13h, AH=41h:
;   1 – Device Access using the packet structure
;   2 – Drive Locking and Ejecting
;   4 – Enhanced Disk Drive Support (EDD)
; We need to check bit 1 as we make use of Extended Read Drive Parameters and Extended Read Sectors from Drive
test cx, 0x0001
jc LacklusterExtendedFunctionality

; Read drive information to get sector size of the disk.
mov dl, [BOOT_DRIVE]
mov si, 0x7C00 + 1024 ; Safe place to temporarily store our struc
mov word [si+DriveParameterPacket.PacketSize], DriveParameterPacket_size
call GetDriveParameters

; How many sectors to read in order to read the entire bootloader into memory.
mov ax, NUMBER_OF_BOOTLOADER_BYTES
mov bx, [si+DriveParameterPacket.BytesPerSector]
xor dx, dx
add ax, bx
dec ax
div bx ; AX will contain the result, which is number of sectors for the bootloader.
mov cx, ax ; Transfer ax to cx because we'll need to set AH for BIOS function.

mov dl, [BOOT_DRIVE]
mov si, 0x7C00 + 1024 ; Safe place to temporarily store our struc
mov byte [si+DiskAddressPacket.PacketSize], DiskAddressPacket_size
mov byte [si+DiskAddressPacket.Reserved],    0x0
mov word [si+DiskAddressPacket.SectorCount], cx  ; CX currently contains no. sectors needed. We include MBR because we want to read the entire MBR
                                                 ; in case that BIOS originlly only loaded 512 bytes worth of it.
mov dword [si+DiskAddressPacket.LoadToAddr], 0x7C00
mov dword [si+DiskAddressPacket.ReadLBA],    0x0 ; Low  DWORD
mov dword [si+DiskAddressPacket.ReadLBA+4],  0x0 ; High DWORD
                                                 ; QWORD=Sector 0 (includes MBR)
call ReadDisk

; Validate the Identification Magic for SBOOT.
mov cx, BOOTLOADER_VALIDATION_HEADER_SIZE
mov di, SBOOT_ADDRESS
mov si, BOOTLOADER_VALIDATION_HEADER_TEXT
repe cmpsb
jnz FailValidateBootloaderHeader

; Jump to SBOOT, skipping past the Identification Magic.
jmp SBOOT_ADDRESS+BOOTLOADER_VALIDATION_HEADER_SIZE

Halt:
    cli
    hlt
    jmp Halt

; Prints the string 'si' points to.
; Input string shouldn't contain carriage returns (0xD, "CR"), this function handles new lines with just line feeds (0xA, "LF").
Print:
    push ax
    push si
    
    mov ah, 0x0E
    .WriteLoop:
        mov al, [si]
        cmp al, byte 0
        je .WriteDone

        cmp al, byte 0xA ; Is this a new line character?
                         ; We check because end line format is CRLF and just LF isn't enough. This function compensates for that
                         ; and out input string do not need to explictly contain CR themselves.
        je .FeedLine

        inc si
        int 0x10
        jmp .WriteLoop
    .FeedLine:
        mov al, 0xD ; Carriage return
        int 0x10
        mov al, 0xA ; Line feed
        int 0x10
        inc si
        jmp .WriteLoop
    .WriteDone:
        pop si
        pop ax
        ret

FailValidateBootloaderHeader:
    mov si, MSG_ERROR_BOOTLOADER_HEADER_VALIDATE
    call Print
    jmp Halt

NoExtendedFunctionality:
    mov si, MSG_NO_EXTENDED_FUNCTIONALITY
    call Print
    jmp Halt

LacklusterExtendedFunctionality:
    mov si, MSG_LACKLUSTER_EXTENDED_FUNCTIONALITY
    call Print
    jmp Halt

; Function that gets the parameters of a disk.
;   Parameters:
;     DL: The drive to read the parameters of.
;     SI: Address to the DriveParameterPacket struc that will receive the result.
GetDriveParameters:
    push ax
    mov ah, 0x48 ; INT 13H, AH=0x48:GetDriveParameters
    
    int 0x13
    jc .Fail

    pop ax
    ret

    .Fail:
        mov si, MSG_ERROR_GET_DRIVE_PARAMETERS
        call Print
        jmp Halt

; Function that reads sector(s) from a disk.
;   Parameters:
;     DL: The drive to read the sectors from.
;     SI: Address to the DiskAddressPacket struc that will receive the result.
ReadDisk:
    push ax
    mov ah, 0x42 ; Extended read

    int 0x13
    jc .Fail

    pop ax
    ret

    .Fail:
        mov si, MSG_ERROR_READ_DISK
        call Print
        jmp Halt

struc DriveParameterPacket
    .PacketSize      resw 1
    .InfoFlags       resw 1
    .CylinderCount   resd 1
    .HeadCount       resd 1
    .SectorsPerTrack resd 1
    .SectorCount     resq 1
    .BytesPerSector  resw 1
    .Unused          resd 1
endstruc

struc DiskAddressPacket
    .PacketSize  resb 1 ; Size of the DiskAddressPacket structure. Always set it to DiskAddressPacket_size.
    .Reserved    resb 1 ; Reserved.
    .SectorCount resw 1 ; Number of sectors to read.
    .LoadToAddr  resd 1 ; Memory address to load the readed sectors into.
    .ReadLBA     resq 1 ; LBA address to start reading from.
endstruc

BOOT_DRIVE db 0 ; We save our boot drive from DL upon boot to here since we'll probably lose it during all boot stuff.

MSG_NO_EXTENDED_FUNCTIONALITY: db "Disk interface too old.", 0xA, 0x0
MSG_LACKLUSTER_EXTENDED_FUNCTIONALITY: db "Packstruc interface absent.", 0xA, 0x0
MSG_ERROR_GET_DRIVE_PARAMETERS: db "GetDriveParameters fail.", 0xA, 0x0
MSG_ERROR_READ_DISK: db "ReadDisk fail.", 0xA, 0x0
MSG_ERROR_BOOTLOADER_HEADER_VALIDATE: db "Failed to validate read BL sectors.", 0xA, 0x0
BOOTLOADER_VALIDATION_HEADER_TEXT: db BOOTLOADER_VALIDATION_HEADER

times 510-($-$$) db 0 ; Pad zeroes
dw 0xAA55

; This are of the bootloader is post the 512-byte mark. The first 512 bytes of the bootloader immediately tries to jump here
; with proper configuration to avoid the 510 byte limit due to the 0xAA55 magic signature.

; Contain the VALIDATION HEADER, MBR will check for this and jump to actual code as LOADED+HEADERSIZE, right below this line of code.
db BOOTLOADER_VALIDATION_HEADER

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
jmp GDT32_CODE_SEGMENT:Protenv

%include "Realenv/GDT32.asm"
%include "Realenv/A20.asm"

MSG_NOTIFY_LOADED_BOOTLODAER:  db "The Biological Bootloader (BIOBoot) has been successfully loaded into memory.", 0xA, 0x0
MSG_NOTIFY_JUMPING_TO_PROTECT: db "Jumping to Protected Mode. If your system hangs for a long time, an error has occured, which may be along the lines of: CPU doesn't support necessary instructions (CPUID) or processor doesn't support Long Mode, as BIO is a 64-bit operating system.", 0xA, 0x0

[bits 32]

Protenv: ; Protected Environment
    ; Update all segment registers to our GDT's data segment.
    ; Otherwise, CPU thinks they are real mode registers and we'll probably triple fault at some point.
    mov ax, GDT32_DATA_SEGMENT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Post 1MiB point.
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
    jmp GDT64_CODE_SEGMENT:Longenv

    jmp Halt32

Halt32:
    cli
    hlt
    jmp Halt32

%include "Protenv/Paging.asm"
%include "Protenv/CPUID.asm"
%include "Protenv/GDT64.asm"

[bits 64]

Longenv: ; Long Environment
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
    mov esi, STR_BOOTLOADER_LONG
    call VGA_Print

    ; TODO: Parse file system, load kernel.

    jmp Halt64

Halt64:
    cli
    hlt
    jmp Halt64

%include "Drivers/VGA.asm"

STR_BOOTLOADER_LONG: db "BIOBoot has successfully entered Long Mode. Now operating in 64-bits, welcome home."

times NUMBER_OF_BOOTLOADER_BYTES-($-$$) db 0 ; Pad image with zeroes.
