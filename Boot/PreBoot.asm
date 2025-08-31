; BIO PreBoot: The Master Boot Record (MBR) part of BIOBoot.
;; SECTOR 1: BOOT SECTOR (MBR, LBA 0x0)
[bits 16]
[org 0x7C00]

; Myth File System Configuration Chunk
jmp short PreBoot ; 2 bytes for short jump to bootloader code.
dd 0              ; Four byte magic string "MYTH"
dw 0              ; Bytes per file system block.
dq 0              ; Metadata block address in file system block addressing.
; None of the Configuration Chunk values (except JMP SHORT) are given here, because
; the Myth File System Tool will write those when it's invoked during the MakeFS phase.
; Only the short jump is here because, well, MakeFS isn't able to do that and won't overwrite that field.

; Code of BL begins here, the first ever instruction JMP SHORT jumps right here, skipping past the FS Configuration Chunk.
PreBoot: ; Real Environment

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
mov si, 0x7C00 + 1024 ; Safe place to temporarily store our struc (will be overwritten by loaded data anyway ): sad face... )
mov byte [si+DiskAddressPacket.PacketSize],  DiskAddressPacket_size
mov byte [si+DiskAddressPacket.Reserved],    0x0
mov word [si+DiskAddressPacket.SectorCount], cx  ; CX currently contains no. sectors needed. We include MBR because we want to read the entire MBR
                                                 ; in case that BIOS originlly only loaded 512 bytes worth of it.
mov dword [si+DiskAddressPacket.LoadToAddr], 0x7C00
mov dword [si+DiskAddressPacket.ReadLBA],    0x0 ; Low  DWORD
mov dword [si+DiskAddressPacket.ReadLBA+4],  0x0 ; High DWORD
                                                 ; QWORD=Sector 0 (includes MBR)
call ReadDisk

; Validate the Identification Magic for BASEBOOT.
mov cx, BOOTLOADER_VALIDATION_SIZE
mov di, LOAD_ADDRESS
mov si, BOOTLOADER_VALIDATION_HEADER_TEXT
repe cmpsb
jnz FailValidateBootloaderHeader

; Validate the Idenfitication Tail for BASEBOOT.
mov cx, BOOTLOADER_VALIDATION_SIZE
mov di, BOOTLOADER_ORIGIN + NUMBER_OF_BOOTLOADER_BYTES - BOOTLOADER_VALIDATION_SIZE
mov si, BOOTLOADER_VALIDATION_TAIL_TEXT
repe cmpsb
jnz FailValidateBootloaderTail

; Jump to SBOOT, skipping past the Identification Magic.
jmp LOAD_ADDRESS+BOOTLOADER_VALIDATION_SIZE

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
    mov si, MSG_ERROR_BOOTLOADER_VALIDATE
    call Print
    mov si, MSG_ERROR_BOOTLOADER_VALIADATE_HEADER
    call Print
    jmp Halt

FailValidateBootloaderTail:
    mov si, MSG_ERROR_BOOTLOADER_VALIDATE
    call Print
    mov si, MSG_ERROR_BOOTLOADER_VALIADATE_TAIL
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

MSG_NO_EXTENDED_FUNCTIONALITY:         db "Disk interface too old.", 0xA, 0x0
MSG_LACKLUSTER_EXTENDED_FUNCTIONALITY: db "Packstruc interface absent.", 0xA, 0x0
MSG_ERROR_GET_DRIVE_PARAMETERS:        db "GetDriveParameters failed.", 0xA, 0x0
MSG_ERROR_READ_DISK:                   db "ReadDisk failed.", 0xA, 0x0

MSG_ERROR_BOOTLOADER_VALIDATE:         db "Integrity of the Bootloader has been compromised: ", 0x0
MSG_ERROR_BOOTLOADER_VALIADATE_HEADER: db "HEADER", 0xA, 0x0
MSG_ERROR_BOOTLOADER_VALIADATE_TAIL:   db "TAIL", 0xA, 0x0

BOOTLOADER_VALIDATION_HEADER_TEXT: db BOOTLOADER_VALIDATION_HEADER
BOOTLOADER_VALIDATION_TAIL_TEXT:   db BOOTLOADER_VALIDATION_TAIL

times 510-($-$$) db 0 ; Pad zeroes
dw 0xAA55
