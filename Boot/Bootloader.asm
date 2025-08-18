%define BOOTLOADER_VALIDATION_HEADER      "BIOLOAD!"
%define BOOTLOADER_VALIDATION_HEADER_SIZE  0x8
%define NUMBER_OF_BOOTLOADER_BYTES         8192 ; Let's give the entire bootloader 16 sectors. 8KB should be more than enough.
%define SBOOT_ADDRESS                      0x7E00 ; Post 512 bytes after 512 byte worth of MBR

;; SECTOR 1: BOOT SECTOR (MBR, LBA 0x0)
[bits 16]
[org 0x7C00]

; BIOFS compatible disk.
jmp short BLMain ; 2 bytes for short jump to bootloader code.
; NOTE: Myth File System Tool will override the 3 fields below, these are prototype-esque.
db "MYTH"        ; Indicate existence of the Myth File System on this disk. 
dw  4096         ; Bytes per file system block. Can be changed during MakeFs phase.
dq  2            ; Block address of the metadata block. Can be changed during MakeFs phase.

; Code of BL begins here, the first ever instruction JMP SHORT jumps right here, skipping past FS early configuration.
BLMain:

; Immediately save DL to BOOT_DRIVE.
mov [BOOT_DRIVE], dl

; Stack setup.
mov bp, 0xB000
mov sp, bp

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

mov cx, 8
mov di, SBOOT_ADDRESS
mov si, BOOTLOADER_VALIDATION_HEADER_TEXT
repe cmpsb
jnz FailValidateBootloaderHeader

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

%include "Boot/A20.asm"

MSG_NOTIFY_LOADED_BOOTLODAER: db "The Biological Bootloader (BIOBoot) has been successfully loaded into memory.", 0xA, 0x0

times NUMBER_OF_BOOTLOADER_BYTES-($-$$) db 0 ; Pad image with zeroes.
