; Checks whether or not the CPU supports the CPUID instruction.
;   Return Value:
;     EAX -> 1 if supported, 0 if not.
CheckCPUID:
    pushfd
    pop eax

    mov ecx, eax
    xor eax, 1 << 21

    push eax
    popfd
    pushfd
    pop eax

    push ecx
    popfd

    xor eax, ecx
    mov eax, 0 ; Assume unsupported
    jnz .Supported
    
    .Exit:
        ret

    .Supported:
        mov eax, 1
        jmp .Exit
