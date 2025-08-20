GDT64_Start:
GDT64_Null:
    dq 0         ; 8 bytes
GDT64_Code:
    dw 0xFFFF    ; Segment Limit: Lower  16-bits
    dw 0x0000    ; Segment Base:  Lower  16-bits
    db 0x00      ; Segment Base:  Middle  8-bits
    db 10011011b ; Access Byte: Present, Kernel Privileges, Code/Data Segment, Executable,
                 ; Only Kernel TPL, Readable, Accessed (set in advance to avoid triple faults just in case.)
    db 10101111b ; Flags & Limit: Upper 4-bits. Flags: Reserved(unset), Long Mode = 1, DB=0(because LM=1), Granular
    db 0x00      ; Segment Base: Upper 8-bits
GDT64_Data:
    dw 0xFFFF    ; Segment Limit: Lower  16-bits
    dw 0x0000    ; Segment Base:  Lower  16-bits
    db 0x00      ; Segment Base:  Middle  8-bits
    db 10010011b ; Access Byte: Present, Kernel Privileges, Code/Data Segment, Non-Executable
                 ; Grows Up (standard), Writable, Accessed (set in advance to avoid triple faults just in case.)
    db 10101111b ; Flags & Limit: Upper 4-bits. Flags: Reserved(unset), Long Mode = 1, DB=0(because LM=1), Granular
    db 0x00      ; Segment Base: Upper 8-bits
GDT64_End:

; Pointer to this will be loaded into the GDT register
GDTR64:
    dw GDT64_End - GDT64_Start - 1 ; Size:   1 byte less than actual size
    dq GDT64_Start                 ; Offset: For a 64-bit GDT, this is a QWORD.

GDT64_CODE_SEGMENT equ 0000000000001000b ; Index 1 within GDT
GDT64_DATA_SEGMENT equ 0000000000010000b ; Index 2 within GDT
