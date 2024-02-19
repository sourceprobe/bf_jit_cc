.intel_syntax noprefix

.global tape
.global i
.global pc
.global plus
.global plus_end
.global minus
.global minus_end
.global right
.global right_end
.global left
.global left_end
.global dot
.global dot_end
.global prologue
.global prologue_end
.global bleft
.global bleft_end
.global bright
.global bright_end

# prologue is expected to ret.
prologue:
    ret
prologue_end:
    ret

plus:
    movsxd rsi, dword ptr [i]
    inc byte ptr [tape+rsi]
plus_end:
    ret

minus:
    movsxd rsi, dword ptr [i]
    dec byte ptr [tape+rsi]
minus_end:
    ret

right:
    # inc rsi
    inc dword ptr [i]
right_end:
    ret

left:
    dec dword ptr [i]
left_end:
    ret

dot:
    # write(1, &tape[i], 1 /*single byte*/);
    mov rax, 1 # write syscall
    mov rdi, 1 # stdout
    movsxd rsi, dword ptr [i] # tape[i]
    lea rsi, [tape+rsi]
    mov rdx, 1 # length = one byte
    syscall
dot_end:
    ret

# branch operators
# jump instructions encode relative offset into the instruction,
# so these instructions will need to be patched by the compiler.
# offsets are relative to next instruction.
#
# Jumps are written as large constants, rather than labels, to ensure
# rel32 jump variants are assembled.

# [ : jump if tape[i] == 0 
bleft:
    movsxd rsi, dword ptr [i]
    mov al, byte ptr [tape+rsi]
    test al, al
    jz 0x123456 # 0f 84 rel32
bleft_end:
    ret

# ] : jump if tape[i] != 0
bright:
    movsxd rsi, dword ptr [i]
    mov al, byte ptr [tape+rsi]
    test al, al
    jnz 0x123456 # 0f 85 rel32
bright_end:
    ret

