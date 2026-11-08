; Register-preserving observers, following Glint Spotter's MIT tail-thunk
; approach (Copyright 2026 Seth). See third_party/licenses/glint-spotter.LICENSE.
EXTERN gc_HeadingCamera:PROC
EXTERN gc_HeadingTick:PROC
EXTERN gc_HeadingAim:PROC
EXTERN gc_HeadingAction:PROC
EXTERN gc_headingCameraOriginal:QWORD
EXTERN gc_headingTickOriginal:QWORD
EXTERN gc_headingAimOriginal:QWORD
EXTERN gc_headingTwoTargetOriginal:QWORD
.code
OBSERVE MACRO name, callback, destination
name PROC FRAME
    pushfq
    .allocstack 8
    sub rsp, 208
    .allocstack 208
    .endprolog
    mov [rsp+32], rax
    mov [rsp+40], rcx
    mov [rsp+48], rdx
    mov [rsp+56], r8
    mov [rsp+64], r9
    mov [rsp+72], r10
    mov [rsp+80], r11
    movups [rsp+96], xmm0
    movups [rsp+112], xmm1
    movups [rsp+128], xmm2
    movups [rsp+144], xmm3
    movups [rsp+160], xmm4
    movups [rsp+176], xmm5
    call callback
    movups xmm5, [rsp+176]
    movups xmm4, [rsp+160]
    movups xmm3, [rsp+144]
    movups xmm2, [rsp+128]
    movups xmm1, [rsp+112]
    movups xmm0, [rsp+96]
    mov r11, [rsp+80]
    mov r10, [rsp+72]
    mov r9, [rsp+64]
    mov r8, [rsp+56]
    mov rdx, [rsp+48]
    mov rcx, [rsp+40]
    mov rax, [rsp+32]
    add rsp, 208
    popfq
    jmp qword ptr [destination]
name ENDP
ENDM
OBSERVE gc_HeadingCameraThunk, gc_HeadingCamera, gc_headingCameraOriginal
OBSERVE gc_HeadingTickThunk, gc_HeadingTick, gc_headingTickOriginal
OBSERVE gc_HeadingAimThunk, gc_HeadingAim, gc_headingAimOriginal
OBSERVE gc_HeadingActionThunk, gc_HeadingAction, gc_headingAimOriginal
OBSERVE gc_HeadingTwoTargetThunk, gc_HeadingAction, gc_headingTwoTargetOriginal
END
