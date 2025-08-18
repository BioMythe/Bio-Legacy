; Checks if system is emulated by QEMU.
; Needed for custom A20 gate checking, QEMU doesn't emulate standard wraparound.
;   Return Value (AX) -> 0 if not QEMU, 1 if QEMU.
IsQEMU:
    push dx

    mov dx, 0x5658
    in  eax, dx
    cmp eax, 0
    mov ax, 1 ; ıs QEMU
    je .NotQEMU
    jmp .Exit

    .NotQEMU:
        mov ax, 0

    .Exit:
        pop dx
        ret
