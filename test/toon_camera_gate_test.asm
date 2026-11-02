OPTION CASEMAP:NONE
EXTERN mt2mp_toon_camera_gate:PROC
EXTERN mt2mp_npc_camera_gate:PROC
EXTERN mt2mp_toon_travel_gate:PROC
EXTERN mt2mp_toon_attribute_gate:PROC
EXTERN gate_route:QWORD
EXTERN gate_flags_before:QWORD
EXTERN gate_flags_after:QWORD
EXTERN gate_registers:QWORD
EXTERN gate_xmm:BYTE
.const
pattern dq 0123456789abcdefh, 0fedcba987654321h
.code
gate_test_original PROC
    mov qword ptr [gate_route],1
    ret
gate_test_original ENDP
gate_test_deferred PROC
    mov qword ptr [gate_route],2
    ret
gate_test_deferred ENDP
gate_test_call PROC
    push rbx
    sub rsp,0c8h
    movdqu [rsp+020h],xmm6
    movdqu [rsp+030h],xmm7
    movdqu [rsp+040h],xmm8
    movdqu [rsp+050h],xmm9
    movdqu [rsp+060h],xmm10
    movdqu [rsp+070h],xmm11
    movdqu [rsp+080h],xmm12
    movdqu [rsp+090h],xmm13
    movdqu [rsp+0a0h],xmm14
    movdqu [rsp+0b0h],xmm15
    mov rbx,rcx
    mov [rsp+0c0h],rdx
    movdqu xmm0,oword ptr [pattern]
    movdqu xmm1,oword ptr [pattern]
    movdqu xmm2,oword ptr [pattern]
    movdqu xmm3,oword ptr [pattern]
    movdqu xmm4,oword ptr [pattern]
    movdqu xmm5,oword ptr [pattern]
    movdqu xmm6,oword ptr [pattern]
    movdqu xmm7,oword ptr [pattern]
    movdqu xmm8,oword ptr [pattern]
    movdqu xmm9,oword ptr [pattern]
    movdqu xmm10,oword ptr [pattern]
    movdqu xmm11,oword ptr [pattern]
    movdqu xmm12,oword ptr [pattern]
    movdqu xmm13,oword ptr [pattern]
    movdqu xmm14,oword ptr [pattern]
    movdqu xmm15,oword ptr [pattern]
    mov rax,101h
    mov rcx,102h
    mov rdx,103h
    mov r8,104h
    mov r9,105h
    mov r10,106h
    mov r11,107h
    cmp qword ptr [rsp+0c0h],0
    stc
    pushfq
    pop qword ptr [gate_flags_before]
    call mt2mp_toon_camera_gate
    mov [gate_registers+0],rax
    mov [gate_registers+8],rcx
    mov [gate_registers+16],rdx
    mov [gate_registers+24],r8
    mov [gate_registers+32],r9
    mov [gate_registers+40],r10
    mov [gate_registers+48],r11
    pushfq
    pop qword ptr [gate_flags_after]
    movdqu oword ptr [gate_xmm+000h],xmm0
    movdqu oword ptr [gate_xmm+010h],xmm1
    movdqu oword ptr [gate_xmm+020h],xmm2
    movdqu oword ptr [gate_xmm+030h],xmm3
    movdqu oword ptr [gate_xmm+040h],xmm4
    movdqu oword ptr [gate_xmm+050h],xmm5
    movdqu oword ptr [gate_xmm+060h],xmm6
    movdqu oword ptr [gate_xmm+070h],xmm7
    movdqu oword ptr [gate_xmm+080h],xmm8
    movdqu oword ptr [gate_xmm+090h],xmm9
    movdqu oword ptr [gate_xmm+0a0h],xmm10
    movdqu oword ptr [gate_xmm+0b0h],xmm11
    movdqu oword ptr [gate_xmm+0c0h],xmm12
    movdqu oword ptr [gate_xmm+0d0h],xmm13
    movdqu oword ptr [gate_xmm+0e0h],xmm14
    movdqu oword ptr [gate_xmm+0f0h],xmm15
    movdqu xmm6,[rsp+020h]
    movdqu xmm7,[rsp+030h]
    movdqu xmm8,[rsp+040h]
    movdqu xmm9,[rsp+050h]
    movdqu xmm10,[rsp+060h]
    movdqu xmm11,[rsp+070h]
    movdqu xmm12,[rsp+080h]
    movdqu xmm13,[rsp+090h]
    movdqu xmm14,[rsp+0a0h]
    movdqu xmm15,[rsp+0b0h]
    add rsp,0c8h
    pop rbx
    ret
gate_test_call ENDP
gate_test_travel_call PROC
    push rbx
    sub rsp,0c8h
    movdqu [rsp+020h],xmm6
    movdqu [rsp+030h],xmm7
    movdqu [rsp+040h],xmm8
    movdqu [rsp+050h],xmm9
    movdqu [rsp+060h],xmm10
    movdqu [rsp+070h],xmm11
    movdqu [rsp+080h],xmm12
    movdqu [rsp+090h],xmm13
    movdqu [rsp+0a0h],xmm14
    movdqu [rsp+0b0h],xmm15
    mov rbx,rcx
    mov [rsp+0c0h],rdx
    movdqu xmm0,oword ptr [pattern]
    movdqu xmm1,oword ptr [pattern]
    movdqu xmm2,oword ptr [pattern]
    movdqu xmm3,oword ptr [pattern]
    movdqu xmm4,oword ptr [pattern]
    movdqu xmm5,oword ptr [pattern]
    movdqu xmm6,oword ptr [pattern]
    movdqu xmm7,oword ptr [pattern]
    movdqu xmm8,oword ptr [pattern]
    movdqu xmm9,oword ptr [pattern]
    movdqu xmm10,oword ptr [pattern]
    movdqu xmm11,oword ptr [pattern]
    movdqu xmm12,oword ptr [pattern]
    movdqu xmm13,oword ptr [pattern]
    movdqu xmm14,oword ptr [pattern]
    movdqu xmm15,oword ptr [pattern]
    mov rax,101h
    mov rcx,102h
    mov rdx,103h
    mov r8,104h
    mov r9,105h
    mov r10,106h
    mov r11,107h
    cmp qword ptr [rsp+0c0h],0
    stc
    pushfq
    pop qword ptr [gate_flags_before]
    call mt2mp_toon_travel_gate
    mov [gate_registers+0],rax
    mov [gate_registers+8],rcx
    mov [gate_registers+16],rdx
    mov [gate_registers+24],r8
    mov [gate_registers+32],r9
    mov [gate_registers+40],r10
    mov [gate_registers+48],r11
    pushfq
    pop qword ptr [gate_flags_after]
    movdqu oword ptr [gate_xmm+000h],xmm0
    movdqu oword ptr [gate_xmm+010h],xmm1
    movdqu oword ptr [gate_xmm+020h],xmm2
    movdqu oword ptr [gate_xmm+030h],xmm3
    movdqu oword ptr [gate_xmm+040h],xmm4
    movdqu oword ptr [gate_xmm+050h],xmm5
    movdqu oword ptr [gate_xmm+060h],xmm6
    movdqu oword ptr [gate_xmm+070h],xmm7
    movdqu oword ptr [gate_xmm+080h],xmm8
    movdqu oword ptr [gate_xmm+090h],xmm9
    movdqu oword ptr [gate_xmm+0a0h],xmm10
    movdqu oword ptr [gate_xmm+0b0h],xmm11
    movdqu oword ptr [gate_xmm+0c0h],xmm12
    movdqu oword ptr [gate_xmm+0d0h],xmm13
    movdqu oword ptr [gate_xmm+0e0h],xmm14
    movdqu oword ptr [gate_xmm+0f0h],xmm15
    movdqu xmm6,[rsp+020h]
    movdqu xmm7,[rsp+030h]
    movdqu xmm8,[rsp+040h]
    movdqu xmm9,[rsp+050h]
    movdqu xmm10,[rsp+060h]
    movdqu xmm11,[rsp+070h]
    movdqu xmm12,[rsp+080h]
    movdqu xmm13,[rsp+090h]
    movdqu xmm14,[rsp+0a0h]
    movdqu xmm15,[rsp+0b0h]
    add rsp,0c8h
    pop rbx
    ret
gate_test_travel_call ENDP
gate_test_attribute_call PROC
    push rbx
    sub rsp,0c8h
    movdqu [rsp+020h],xmm6
    movdqu [rsp+030h],xmm7
    movdqu [rsp+040h],xmm8
    movdqu [rsp+050h],xmm9
    movdqu [rsp+060h],xmm10
    movdqu [rsp+070h],xmm11
    movdqu [rsp+080h],xmm12
    movdqu [rsp+090h],xmm13
    movdqu [rsp+0a0h],xmm14
    movdqu [rsp+0b0h],xmm15
    mov rbx,rcx
    mov [rsp+0c0h],rdx
    movdqu xmm0,oword ptr [pattern]
    movdqu xmm1,oword ptr [pattern]
    movdqu xmm2,oword ptr [pattern]
    movdqu xmm3,oword ptr [pattern]
    movdqu xmm4,oword ptr [pattern]
    movdqu xmm5,oword ptr [pattern]
    movdqu xmm6,oword ptr [pattern]
    movdqu xmm7,oword ptr [pattern]
    movdqu xmm8,oword ptr [pattern]
    movdqu xmm9,oword ptr [pattern]
    movdqu xmm10,oword ptr [pattern]
    movdqu xmm11,oword ptr [pattern]
    movdqu xmm12,oword ptr [pattern]
    movdqu xmm13,oword ptr [pattern]
    movdqu xmm14,oword ptr [pattern]
    movdqu xmm15,oword ptr [pattern]
    mov rax,101h
    mov rcx,102h
    mov rdx,103h
    mov r8,104h
    mov r9,105h
    mov r10,106h
    mov r11,107h
    cmp qword ptr [rsp+0c0h],0
    stc
    pushfq
    pop qword ptr [gate_flags_before]
    call mt2mp_toon_attribute_gate
    mov [gate_registers+0],rax
    mov [gate_registers+8],rcx
    mov [gate_registers+16],rdx
    mov [gate_registers+24],r8
    mov [gate_registers+32],r9
    mov [gate_registers+40],r10
    mov [gate_registers+48],r11
    pushfq
    pop qword ptr [gate_flags_after]
    movdqu oword ptr [gate_xmm+000h],xmm0
    movdqu oword ptr [gate_xmm+010h],xmm1
    movdqu oword ptr [gate_xmm+020h],xmm2
    movdqu oword ptr [gate_xmm+030h],xmm3
    movdqu oword ptr [gate_xmm+040h],xmm4
    movdqu oword ptr [gate_xmm+050h],xmm5
    movdqu oword ptr [gate_xmm+060h],xmm6
    movdqu oword ptr [gate_xmm+070h],xmm7
    movdqu oword ptr [gate_xmm+080h],xmm8
    movdqu oword ptr [gate_xmm+090h],xmm9
    movdqu oword ptr [gate_xmm+0a0h],xmm10
    movdqu oword ptr [gate_xmm+0b0h],xmm11
    movdqu oword ptr [gate_xmm+0c0h],xmm12
    movdqu oword ptr [gate_xmm+0d0h],xmm13
    movdqu oword ptr [gate_xmm+0e0h],xmm14
    movdqu oword ptr [gate_xmm+0f0h],xmm15
    movdqu xmm6,[rsp+020h]
    movdqu xmm7,[rsp+030h]
    movdqu xmm8,[rsp+040h]
    movdqu xmm9,[rsp+050h]
    movdqu xmm10,[rsp+060h]
    movdqu xmm11,[rsp+070h]
    movdqu xmm12,[rsp+080h]
    movdqu xmm13,[rsp+090h]
    movdqu xmm14,[rsp+0a0h]
    movdqu xmm15,[rsp+0b0h]
    add rsp,0c8h
    pop rbx
    ret
gate_test_attribute_call ENDP
gate_test_npc_call PROC
    push rbx
    sub rsp,0c8h
    movdqu [rsp+020h],xmm6
    movdqu [rsp+030h],xmm7
    movdqu [rsp+040h],xmm8
    movdqu [rsp+050h],xmm9
    movdqu [rsp+060h],xmm10
    movdqu [rsp+070h],xmm11
    movdqu [rsp+080h],xmm12
    movdqu [rsp+090h],xmm13
    movdqu [rsp+0a0h],xmm14
    movdqu [rsp+0b0h],xmm15
    mov rbx,rcx
    mov [rsp+0c0h],rdx
    movdqu xmm0,oword ptr [pattern]
    movdqu xmm1,oword ptr [pattern]
    movdqu xmm2,oword ptr [pattern]
    movdqu xmm3,oword ptr [pattern]
    movdqu xmm4,oword ptr [pattern]
    movdqu xmm5,oword ptr [pattern]
    movdqu xmm6,oword ptr [pattern]
    movdqu xmm7,oword ptr [pattern]
    movdqu xmm8,oword ptr [pattern]
    movdqu xmm9,oword ptr [pattern]
    movdqu xmm10,oword ptr [pattern]
    movdqu xmm11,oword ptr [pattern]
    movdqu xmm12,oword ptr [pattern]
    movdqu xmm13,oword ptr [pattern]
    movdqu xmm14,oword ptr [pattern]
    movdqu xmm15,oword ptr [pattern]
    mov rax,101h
    mov rcx,102h
    mov rdx,103h
    mov r8,104h
    mov r9,105h
    mov r10,106h
    mov r11,107h
    cmp qword ptr [rsp+0c0h],0
    stc
    pushfq
    pop qword ptr [gate_flags_before]
    call mt2mp_npc_camera_gate
    mov [gate_registers+0],rax
    mov [gate_registers+8],rcx
    mov [gate_registers+16],rdx
    mov [gate_registers+24],r8
    mov [gate_registers+32],r9
    mov [gate_registers+40],r10
    mov [gate_registers+48],r11
    pushfq
    pop qword ptr [gate_flags_after]
    movdqu oword ptr [gate_xmm+000h],xmm0
    movdqu oword ptr [gate_xmm+010h],xmm1
    movdqu oword ptr [gate_xmm+020h],xmm2
    movdqu oword ptr [gate_xmm+030h],xmm3
    movdqu oword ptr [gate_xmm+040h],xmm4
    movdqu oword ptr [gate_xmm+050h],xmm5
    movdqu oword ptr [gate_xmm+060h],xmm6
    movdqu oword ptr [gate_xmm+070h],xmm7
    movdqu oword ptr [gate_xmm+080h],xmm8
    movdqu oword ptr [gate_xmm+090h],xmm9
    movdqu oword ptr [gate_xmm+0a0h],xmm10
    movdqu oword ptr [gate_xmm+0b0h],xmm11
    movdqu oword ptr [gate_xmm+0c0h],xmm12
    movdqu oword ptr [gate_xmm+0d0h],xmm13
    movdqu oword ptr [gate_xmm+0e0h],xmm14
    movdqu oword ptr [gate_xmm+0f0h],xmm15
    movdqu xmm6,[rsp+020h]
    movdqu xmm7,[rsp+030h]
    movdqu xmm8,[rsp+040h]
    movdqu xmm9,[rsp+050h]
    movdqu xmm10,[rsp+060h]
    movdqu xmm11,[rsp+070h]
    movdqu xmm12,[rsp+080h]
    movdqu xmm13,[rsp+090h]
    movdqu xmm14,[rsp+0a0h]
    movdqu xmm15,[rsp+0b0h]
    add rsp,0c8h
    pop rbx
    ret
gate_test_npc_call ENDP
END
