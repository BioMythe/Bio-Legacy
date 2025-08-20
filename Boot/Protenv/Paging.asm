;;;
; Very simple boot time paging mechanism.
; At some point, kernel will remake the entire paging structure.
; This is only to get us to Long Mode, since paging is a prerequisite.
;;;

PML4_ADDRESS equ 0x2000 ; Level-4 Page Map Table. Not 0x1000 because BIOBoot allocates 8192 bytes for itselfs code.
PDPT_ADDRESS equ 0x3000 ; Page Directory Pointer Table
PDT_ADDRESS  equ 0x4000 ; Page Directory Table
PT_ADDRESS   equ 0x5000 ; Page Table

; Only the 48 bits of the Page Table are actually used.
; Some configurationing can be done to increase it, but our OS doesn't make use of that.
PT_ADDRESS_MASK equ 0xFFFFFFFFFF000
PT_PRESENT      equ 1
PT_READABLE     equ 2

ENTRIES_PER_PT  equ 512 ; Number of entries for each kind of page table
BYTES_PER_ENTRY equ 8   ; Each entry is a QWORD.
PAGE_SIZE       equ ENTRIES_PER_PT * BYTES_PER_ENTRY

; Initializes and sets up Long Mode x64 paging structures.
InitPageStructure:
    pushfd
    push eax
    push ebx
    push ecx
    push edi

    ; CR3 stores the page table addresses.
    mov edi, PML4_ADDRESS
    mov cr3, edi

    ; Zero out the memory we allocated for paging structures.
    xor eax, eax       ; EAX = 0
    mov ecx, PAGE_SIZE ; COUNTER = PAGE_SIZE (4096; 512 * 8)
    rep stosd          ; Write byte EAX(0) to EDI(PML4_ADDRESS) for ECX(PAGE_SIZE) times.

    ; PML4 -> PDPT
    mov edi, cr3       ; Reset EDI back to the page table address.
    mov dword [edi], PDPT_ADDRESS & PT_ADDRESS_MASK | PT_PRESENT | PT_READABLE

    ; PDPT -> PDT
    mov edi, PDPT_ADDRESS
    mov dword [edi], PDT_ADDRESS  & PT_ADDRESS_MASK | PT_PRESENT | PT_READABLE

    ; PDT -> PT
    mov edi, PDT_ADDRESS
    mov dword [edi], PT_ADDRESS   & PT_ADDRESS_MASK | PT_PRESENT | PT_READABLE

    ; PT -> Fill Physical Pages
    mov edi, PT_ADDRESS
    mov ebx, PT_PRESENT | PT_READABLE
    mov ecx, ENTRIES_PER_PT

    .PhysicalPageLoop:
        mov dword [edi], ebx
        add ebx, PAGE_SIZE
        add edi, BYTES_PER_ENTRY
        loop .PhysicalPageLoop
    
    pop edi
    pop ecx
    pop ebx
    pop eax
    popfd
    ret
