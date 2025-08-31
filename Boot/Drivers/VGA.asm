VGA_FRAMEBUFFER_ADDRESS equ 0xB8000
VGA_FRAMEBUFFER_WIDTH   equ 80
VGA_FRAMEBUFFER_HEIGHT  equ 25
VGA_FRAMEBUFFER_CELLS   equ VGA_FRAMEBUFFER_WIDTH * VGA_FRAMEBUFFER_HEIGHT
VGA_BYTES_PER_CELL      equ 2 ; 1 byte for character code, 1 byte for color
VGA_FRAMEBUFFER_SIZE    equ VGA_FRAMEBUFFER_CELLS * VGA_BYTES_PER_CELL
VGA_FRAMEBUFFER_END     equ VGA_FRAMEBUFFER_ADDRESS + VGA_FRAMEBUFFER_SIZE

; Gets the current offset of the VGA text cursor.
;   Return Value:
;     CX -> The cursor offset.
VGA_GetCursorOffset:
    pushfq
    push ax
    push dx
    xor rcx, rcx ; Offset = 0
    
    ; Get lower byte.
    mov dx, 0x3D4
    mov al, 0x0F
    out dx, al

    ; Read lower byte.
    mov dx, 0x3D5
    in al, dx
    mov cl, al

    ; Get higher byte.
    mov dx, 0x3D4
    mov al, 0x0E
    out dx, al

    ; Read higher byte.
    mov dx, 0x3D5
    in al, dx
    mov ch, al

    pop ax
    pop dx
    popfq
    ret

; Gets the current 2D position the VGA text cursor.
;   Return Value:
;     DX -> X position.
;     AX -> Y position.
VGA_GetCursorPosition:
    pushfq
    push cx

    call VGA_GetCursorOffset ; CX = Offset
    mov ax, cx               ; AX = CX(Offset)

    ; Formulas:
    ; X = Offset % Width
    ; Y = Offset / Width
    
    xor dx, dx ; Clear HIGH WORD of DIV.
    mov cx, VGA_FRAMEBUFFER_WIDTH
    div cx ; AX / CX

    pop cx
    popfq
    ret

; Modifies the offset of the VGA text cursor, using a direct offset.
;   Parameters:
;     BX -> New cursor offset.
VGA_SetCursorOffset:
    pushfq
    push ax
    push dx

    mov dx, 0x03D4
    mov al, 0x0F
    out dx, al

    inc dl
    mov al, bl
    out dx, al

    dec dl
    mov al, 0x0E
    out dx, al

    inc dl
    mov al, bh
    out dx, al

    pop dx
    pop ax
    popfq
    ret

; Modifies the offset of the VGA text cursor, using a 2D coordinate system.
;   Parameters:
;     BX -> New X position.
;     AX -> New Y position.
VGA_SetCursorPosition:
    pushfq
    push bx
    push dx

    mov dl, VGA_FRAMEBUFFER_WIDTH
    mul dl
    add bx, ax

    ; Y * W + X in BX
    call VGA_SetCursorOffset

    pop dx
    pop bx
    popfq
    ret

VGA_Clear:
    pushfq
    push  ax
    push  bx
    push rcx
    push rdi

    mov ax, 0x0f20 ; Space, White on Black
    mov ecx, VGA_FRAMEBUFFER_SIZE
    lea edi, VGA_FRAMEBUFFER_ADDRESS
    cld
    rep stosw

    xor bx, bx
    call VGA_SetCursorOffset

    pop rdi
    pop rcx
    pop  bx
    pop  ax
    popfq
    ret

VGA_Scroll:
    pushfq
    push  ax
    push rcx
    push rdi
    push rsi

    ; Move each row into the position of the row above (overwriting the top row)
    mov rdi, VGA_FRAMEBUFFER_ADDRESS                                                   ; Start of the 1st row.
    mov rsi, VGA_FRAMEBUFFER_ADDRESS + (VGA_FRAMEBUFFER_WIDTH * VGA_BYTES_PER_CELL)    ; Start of the 2nd row.
    mov rcx, (VGA_FRAMEBUFFER_SIZE - (VGA_FRAMEBUFFER_WIDTH * VGA_BYTES_PER_CELL)) / 2 ; No. WORDs to copy.
    rep movsw

    ; Clear out the last row.
    mov rdi, VGA_FRAMEBUFFER_END - (VGA_FRAMEBUFFER_WIDTH * VGA_BYTES_PER_CELL) ; Last row start.
    mov ax,  0x0f20 ; Space, white on black.
    mov rcx, (VGA_FRAMEBUFFER_WIDTH * VGA_BYTES_PER_CELL) / 2
    rep stosw

    pop rsi
    pop rdi
    pop rcx
    pop  ax
    popfq
    ret

; Prints a null-terminated string.
;   Parameters:
;     RSI -> Pointer to the string.
VGA_Print:
    pushfq
    push rax
    push rbx
    push rcx
    push rdx
    push rsi

    ; CX = Cursor Tracker
    movzx rcx, cx ; Extend CX into QWORD.
    call VGA_GetCursorOffset

    ; RBX = Framebuffer Address
    mov rbx, VGA_FRAMEBUFFER_ADDRESS

    ; RBX += RCX * BYTES_PER_CELL
    xor rdx, rdx
    mov rax, VGA_BYTES_PER_CELL
    mul rcx
    add rbx, rax

    .Loop:
        mov al, [rsi]  ; Load current char into AL.
        cmp al, byte 0 ; Null terminator hit?
        je .Done       ; If so, goto Exit label.
        inc rsi        ; Increase string pointer to next char.

        ; Check for LF.
        cmp al, 0x0A
        je .FeedLine

        mov [rbx], al  ; Move into video memory.
        inc rbx        ; Increase video memory pointer.

        mov byte [rbx], 0x0F ; White on black.
        inc rbx              ; Next cell.

        inc cx ; Cursor Tracker
        cmp cx, VGA_FRAMEBUFFER_CELLS ; Last cell was written just to?
        jne .Loop ; If not, continue write loop.

        ; Cells full, scroll screen up.
        call VGA_Scroll
        
        mov cx, VGA_FRAMEBUFFER_CELLS - VGA_FRAMEBUFFER_WIDTH ; Last row, first cell in the row.
        xchg bx, cx ; VGA_SetCursorOffset needs in BX.
            call VGA_SetCursorOffset
        xchg bx, cx
        sub rbx, VGA_FRAMEBUFFER_WIDTH * VGA_BYTES_PER_CELL ; Adjust Framebuffer Pointer
        
        jmp .Loop ; Char loop
    .FeedLine:
        ; Flush current tracker value first to have all functions below work correctly because they rely on port value and not our CX.
        xchg bx, cx
            call VGA_SetCursorOffset
        xchg bx, cx

        ; DX = Cursor X, AX = Cursor Y.
        call VGA_GetCursorPosition
        inc ax ; Advance to the next row.
                  ; IMPORTANT: I HONESTLY DONT KNOW WHY THE HELL WE NEED TO ADD 2 INSTEAD OF 1. IT JUST WORKS IDK. WILL FUCK US LATER.

        cmp ax, VGA_FRAMEBUFFER_HEIGHT ; Is this after the last row (out of bounds)?
                                         ; IMPORTANT: ONCE AGAIN, I DONT KNOW WHY THE FUCK I NEED TO MUL BY 2. IT JUST WORKS.
        jne .Proceed ; If not, proceed.

        call VGA_Scroll ; We need to scroll up since no more lines/rows are left.
        mov ax, VGA_FRAMEBUFFER_HEIGHT - 1 ; Last row.

    .Proceed:
        xor bx, bx ; X = 0
        call VGA_SetCursorPosition ; Set cursor position to the updated coordinates in (BX, AX).
        
        ; Reload Cursor Tracker.
        call VGA_GetCursorOffset ; CX = Cursor Tracker
        movzx rcx, cx ; Extend CX to QWORD space (zero out upper bits)

        ; Adjust the video memory pointer in RBX.
        ; We need: RBX = FB + (CX * 2)
        mov rbx, VGA_FRAMEBUFFER_ADDRESS ; RBX = FB. We need RBX += CX*2
        push rcx ; store original rcx
            shl rcx, 1 ; CX *= 2
            add rbx, rcx ; RBX += CX*2
        pop rcx ; restore rcx

        jmp .Loop ; Go to char loop again.
    .Done:
        xchg bx, cx
            call VGA_SetCursorOffset ; VGA_SetCursorOffset needs in BX.
        xchg bx, cx

        pop rsi
        pop rdx
        pop rcx
        pop rbx
        pop rax
        popfq
        ret
