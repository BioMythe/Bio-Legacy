; GDT32 STRUCTURE BEGIN: GDT STRUCTURE FOR TEMPORARY PROTECTED MODE STAY
GDT32_Start:
GDT32_Null:
    dq 0 ; 8 bytes
GDT32_Code:
    dw 0xFFFF    ; Segment Limit: Lower  16-bits
    dw 0x0000    ; Segment Base:  Lower  16-bits
    db 0x00      ; Segment Base:  Middle  8-bits
    db 10011011b ; Access Byte: Present, Kernel Privileges, Code/Data Segment, Executable,
                 ; Only Kernel TPL, Readable, Accessed (set in advance to avoid triple faults just in case.)
    db 11001111b ; Flags & Limit: Upper 4-bits. Flags: Reserved(unset), Long Mode = 0, DB=Protected, Granular
    db 0x00      ; Segment Base: Upper 8-bits
GDT32_Data:
    dw 0xFFFF    ; Segment Limit: Lower  16-bits
    dw 0x0000    ; Segment Base:  Lower  16-bits
    db 0x00      ; Segment Base:  Middle  8-bits
    db 10010011b ; Access Byte: Present, Kernel Privileges, Code/Data Segment, Non-Executable
                 ; Grows Up (standard), Writable, Accessed (set in advance to avoid triple faults just in case.)
    db 11001111b ; Flags & Limit: Upper 4-bits. Flags: Reserved(unset), Long Mode = 0, DB=Protected, Granular
    db 0x00      ; Segment Base: Upper 8-bits
GDT32_End:
; GDT32 STRUCTURE FINISH

; Pointer to this will be loaded into the GDT register
GDTR32:
    dw GDT32_End - GDT32_Start - 1 ; 1 byte less than actual size
    dd GDT32_Start

GDT32_CODE_SEGMENT equ 0000000000001000b ; Index 1 within GDT
GDT32_DATA_SEGMENT equ 0000000000010000b ; Index 2 within GDT
