%define A20_CHECK_ADDRESS     0x0500
%define A20_CHECK_LOWER_VALUE 0xB1
%define A20_CHECK_UPPER_VALUE 0xC4

; Checks the state of the 21st bus address line.
;   Return Value (AX) -> 0 if disabled, 1 if enabled.
GetStateA20:
    call IsQEMU
    cmp ax, 1
    je .CheckQEMU

    call GetStateA20_Wraparound
    ret

    .CheckQEMU:
        call GetStateA20_QEMU
        ret

; Checks the state of the 21st bus address line.
; QEMU doesn't emulate memory wraparound with A20=0 (from "info registers" dump) for some reason,
; this uses the fast A20 gate which QEMU supports to check A20.
;   Return Value (AX) -> 0 if disabled, 1 if enabled.
GetStateA20_QEMU:
    in al, 0x92
    test al, 2
    jnz .Enabled

    mov ax, 0
    ret

    .Enabled:
        mov ax, 1
        ret

; Checks the state of the 21st bus address line using memory wraparound method.
;   Return Value (AX) -> 0 if disabled, 1 if enabled.
GetStateA20_Wraparound:
    pushf
    push ds
    push es
    push di
    push si
    cli

    xor ax, ax
    mov es, ax ; ES = 0x0000

    not ax
    mov ds, ax ; DS = 0xFFFF

    mov di, A20_CHECK_ADDRESS        ; es:di = 0x0000:ADDRESS
    mov si, A20_CHECK_ADDRESS + 0x10 ; ds:si = 0xFFFF:(ADDRESS+16), 1MiB above with Real Mode addressing

    ; Keep track original values to restore later.
    mov al, byte [es:di]
    push ax
    mov al, byte [ds:si]
    push ax

    mov byte [es:di], A20_CHECK_LOWER_VALUE
    mov byte [ds:si], A20_CHECK_UPPER_VALUE

    ; Check what byte resides at the under MB location.
    ; If the A20 line is disabled, the lower MB value would be overwritten
    ; when the read to the upper MB line was issued.
    cmp byte [es:di], byte A20_CHECK_UPPER_VALUE
    
    ; Restore original memory contents.
    pop ax
    mov byte [ds:si], al
    pop ax
    mov byte [es:di], al

    mov ax, 0 ; don't use "xor ax, ax" it messes with the jump due to FLAGS change.
    je .ExitA20

    ; If the jump above didn't happen, A20 is enabled to set return value to 1.
    mov ax, 1

    .ExitA20:
        sti
        pop si
        pop di
        pop es
        pop ds
        popf
        ret

; Tries all possible ways to enable the A20 line.
; If fail, halts forever.
EnsureA20:
    pushf
    pusha

    call GetStateA20
    cmp  ax, 1
    je .AlreadyEnabled

    .Method_BIOS:
        mov si, MSG_NOTIFY_A20_METHOD_BIOS
        call Print

        mov ax, 0x2403 ; A20 Gate Support
        int 0x15
        jb .Method_BIOS_NotSupported
        cmp ah, 0
        jnz .Method_BIOS_NotSupported

        mov ax, 0x2402
        int 0x15
        jb .Method_BIOS_Failed
        cmp ah, 0
        jnz .Method_BIOS_Failed

        cmp al, 1
        jz  .AlreadyEnabled

        mov ax, 0x2401
        int 0x15
        jb .Method_BIOS_Failed
        cmp ah, 0
        jnz .Method_BIOS_Failed

        ; BIOS returned result may be incorrect, manually check once more.
        call GetStateA20
        cmp ax, 0
        je .Method_BIOS_Failed

        jmp .Success

    .Method_KeyboardController:
        mov si, MSG_NOTIFY_A20_METHOD_KEYBOARD_CONTROLLER
        call Print
        cli

        call .KBC_Wait
        mov al, 0xAD
        out 0x64, al

        call .KBC_Wait
        mov al, 0xD0
        out 0x64, al

        call .KBC_Wait2
        in al, 0x60
        push eax

        call .KBC_Wait
        mov al, 0xD1
        out 0x64, al

        call .KBC_Wait
        pop eax
        or al, 2
        out 0x60, al

        call .KBC_Wait
        mov al, 0xAE
        out 0x64, al

        ; Interrupt restore
        sti

        call GetStateA20
        cmp ax, 0
        je .Method_KeyboardController_Failed

        jmp .Success

        .KBC_Wait:
            in al, 0x64
            test al, 2
            jnz .KBC_Wait
            ret
            
        .KBC_Wait2:
            in al, 0x64
            test al, 1
            jz .KBC_Wait2
            ret

    .Method_FastGate:
        mov si, MSG_NOTIFY_A20_METHOD_FAST_GATE
        call Print

        in al, 0x92
        test al, 2
        jnz .Vldt
        or al, 2
        and al, 0xFE
        out 0x92, al
        .Vldt:
            call GetStateA20
            cmp ax, 0
            je .Method_FastGate_Failed
            jmp .Success

    .Method_PortEE:
        mov si, MSG_NOTIFY_A20_METHOD_PORT_EE
        call Print

        in al, 0xEE
        call GetStateA20
        cmp ax, 0
        je .Method_PortEE_Failed
        jmp .Success

    .AlreadyEnabled:
        mov si, MSG_NOTIFY_A20_ALREADY_ENABLED
        call Print
        popa
        popf
        ret

    .Method_BIOS_NotSupported:
        mov si, MSG_NOTIFY_A20_ALREADY_ENABLED
        call Print
        jmp .Method_KeyboardController

    .Method_BIOS_Failed:
        mov si, MSG_ERROR_A20_BIOS_FAIL
        call Print
        jmp .Method_KeyboardController

    .Method_KeyboardController_Failed:
        mov si, MSG_ERROR_A20_KEYBOARD_CONTROLLER_FAIL
        call Print
        jmp .Method_FastGate

    .Method_FastGate_Failed:
        mov si, MSG_ERROR_A20_FAST_GATE
        call Print
        jmp .Method_PortEE

    .Method_PortEE_Failed:
        mov si, MSG_ERROR_A20_PORT_EE
        call Print
        jmp .Failure

    .Success:
        mov si, MSG_NOTIFY_A20_ENABLE_SUCCESS
        call Print
        jmp .Exit
    
    .Failure:
        mov si, MSG_ERROR_A20_FAIL
        call Print
        jmp Halt
    
    .Exit:
        popa
        popf
        ret

MSG_NOTIFY_A20_ALREADY_ENABLED: db "The A20 line is already enabled.", 0xA, 0x0
MSG_ERROR_A20_BIOS_NO_SUPPORT: db "The BIOS doesn't support enabling A20 through INT 15h.", 0xA, 0x0
MSG_ERROR_A20_BIOS_FAIL: db "Failed to enable the A20 line using the BIOS.", 0xA, 0x0
MSG_ERROR_A20_KEYBOARD_CONTROLLER_FAIL: db "Failed to enable the A20 line using the keyboard controller.", 0xA, 0x0
MSG_ERROR_A20_FAST_GATE: db "Failed to enable the A20 line using the Fast A20 Gate.", 0xA, 0x0
MSG_ERROR_A20_PORT_EE: db "Failed to enable the A20 line using port I/O on port 0xEE.", 0xA, 0x0
MSG_ERROR_A20_FAIL: db "Couldn't enable the A20 line.", 0xA, 0x0
MSG_NOTIFY_A20_ENABLE_SUCCESS: db "The A20 line has been enabled successfully.", 0xA, 0x0

MSG_NOTIFY_A20_METHOD_BIOS: db "EnsureA20: Trying BIOS method...", 0xA, 0x0
MSG_NOTIFY_A20_METHOD_KEYBOARD_CONTROLLER: db "EnsureA20: Trying keyboard controller method...", 0xA, 0x0
MSG_NOTIFY_A20_METHOD_FAST_GATE: db "EnsureA20: Trying Fast Gate method...", 0xA, 0x0
MSG_NOTIFY_A20_METHOD_PORT_EE: db "EnsureA20: Trying Port 0xEE method...", 0xA, 0x0

%include "Realenv/Environment.asm"
