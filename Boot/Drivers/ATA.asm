;
; Very bare-bones ATA driver. Used to parse the file system and load the kernel.
; Only supports sending IDENTIFY commands to the main device's master and also reading sectors with LBA48.
;

%define ATA_PORT_DATA         0x1F0
%define ATA_PORT_ERROR        0x1F1
%define ATA_PORT_SECTOR_COUNT 0x1F2
%define ATA_PORT_LBA_LOW      0x1F3
%define ATA_PORT_LBA_MID      0x1F4
%define ATA_PORT_LBA_HIGH     0x1F5
%define ATA_PORT_DRIVE        0x1F6
%define ATA_PORT_STATUS_CMD   0x1F7 ; Read = Status, Write = Command
%define ATA_PORT_ALT_STATUS   0x3F6 ; Read = Alternate Status, Write = Device Control

%define ATA_STATUS_ERR        0x01
%define ATA_STATUS_DRQ        0x08  
%define ATA_STATUS_DSC        0x10
%define ATA_STATUS_DF         0x20
%define ATA_STATUS_DRDY       0x40
%define ATA_STATUS_BSY        0x80

%define ATA_COMMAND_IDENTIFY  0xEC

%define ATA_DRIVE_MASTER      0xA0
%define ATA_DRIVE_SLAVE       0xB0

; Sends IDENTIFY command to primary device.
; Parameters:
;   RDI -> Memory address to write the result of the command.
; Return Value:
;   RAX -> 0 on success, 1 if no device found, 2 if device error occured, 3 if DRQ timed out.
IdentifyATA:
    pushfq
    push rcx
    push rdx

    ; Select master drive.
    mov dx, ATA_PORT_DRIVE
    mov al, ATA_DRIVE_MASTER
    out dx, al

    ; Read alternate status 4 times due to I/O delay (per ATA specification practice)
    mov dx, ATA_PORT_ALT_STATUS
    in al, dx
    in al, dx
    in al, dx
    in al, dx

    ; Clear sector count and LBA registers
    mov al, 0
    mov dx, ATA_PORT_SECTOR_COUNT
    out dx, al
    mov dx, ATA_PORT_LBA_LOW
    out dx, al
    mov dx, ATA_PORT_LBA_MID
    out dx, al
    mov dx, ATA_PORT_LBA_HIGH
    out dx, al

    ; IDENTIFY command.
    mov al, ATA_COMMAND_IDENTIFY
    mov dx, ATA_PORT_STATUS_CMD
    in al, dx

    ; Read status, 0 -> no device.
    mov dx, ATA_PORT_STATUS_CMD
    in al, dx
    test al, al
    jz .NoDevice

    ; Poll: wait for BSY clear then DRQ/ERR set
    .PollBSY:
        in al, dx
        test al, ATA_STATUS_BSY
        jnz .PollBSY

        test al, ATA_STATUS_ERR
        jnz .DeviceError

        ; Wait for DRQ, loop cunter.
        mov ecx, 100000
    .WaitDRQ:
        in al, dx
        test al, ATA_STATUS_DRQ
        jnz .ReadData
        test al, ATA_STATUS_ERR
        jnz .DeviceError
        loop .WaitDRQ

        .TimeoutDRQ:
            mov rax, 3
            jmp .Exit
    .ReadData:
        mov rcx, 256 ; 256 words, *2=512 bytes
        mov rdx, ATA_PORT_DATA
        rep insw

        xor rax, rax ; RAX = 0, success code.
        jmp .Exit

    .NoDevice:
        mov rax, 1
        jmp .Exit

    .DeviceError:
        mov rax, 2
        jmp .Exit

    .Exit:
        mov dx, ATA_PORT_ALT_STATUS
        in al, dx

        pop rdx
        pop rcx
        popfq
        ret
    