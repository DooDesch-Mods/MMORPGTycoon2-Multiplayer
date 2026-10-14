OPTION CASEMAP:NONE
.code
mt2mp_free_delete_bridge PROC FRAME
    sub rsp,28h
    .allocstack 28h
    .endprolog
    mov rax,rcx
    mov rcx,rdx
    lea rdx,delete_return
    test r8,r8
    jz delete_call
    mov qword ptr [r8],rdx
delete_call:
    call rax
delete_return:
    add rsp,28h
    ret
mt2mp_free_delete_bridge ENDP

mt2mp_free_sized_bridge PROC FRAME
    sub rsp,28h
    .allocstack 28h
    .endprolog
    mov rax,rcx
    mov rcx,rdx
    mov rdx,r8
    lea r8,sized_return
    test r9,r9
    jz sized_call
    mov qword ptr [r9],r8
sized_call:
    call rax
sized_return:
    add rsp,28h
    ret
mt2mp_free_sized_bridge ENDP
END
