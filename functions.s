.align 2
.data
virtual_base: .word 0
h2p_lw_dataA_addr: .word 0
h2p_lw_dataB_addr: .word 0
h2p_lw_wrReg_addr: .word 0
h2p_lw_wrFull_addr: .word 0
h2p_lw_screen_addr: .word 0
h2p_lw_result_pulseCounter_addr: .word 0
fd: .word 0
HW_REGS_BASE:       .word  0xff200       @ Base address
HW_REGS_SPAN:       .word  0x04000000       @ Span size
HW_REGS_MASK:       .word  0x03FFFFFF       @ Mask (SPAN - 1)
@HEX3_HEX0 .word 0x00000020
ALT_LWFPGASLVS_OFST: .word  0xff200000      @ Offset for FPGA slave address
DATA_A_BASE:        .word  0x80
DATA_B_BASE:        .word  0x70
WRREG_BASE:         .word  0xc0
WRFULL_BASE:        .word  0xb0
SCREEN_BASE:        .word  0xa0
RESET_PULSECOUNTER_BASE: .word  0x90
KEYS_BASE: .word 0x0


.align 2
.section .text
.global createMappingMemory
.type createMappingMemory, %function
createMappingMemory:
    push {r4-r7, lr}

    ldr r0, =filename @ carrega o endereço da string "/dev/mem"
    mov r1, #2
    mov r2, #0 @ file mode
    mov r7, #5 @ SC/ Open
    svc 0

    mov r2, r0 @ armazena o fd em r2

    @ carrega endereço de fd
    ldr r3, =fd
    str r2, [r3]

    @ config para mmap
    mov r0, #0
    mov r1, #4096      
    mov r2, #3 @ PROT_READ | PROT_WRITE
    mov r3, #1 @ MAP_SHARED
    ldr r4, =fd             
    ldr r4, [r4]             
    ldr r5, =HW_REGS_BASE        
    ldr r5, [r5]
    mov r7, #192 @ SC/ mmap2
    svc 0                 

    mov r2, r0 @ armazena virtual_base em r2

    @ carrega endereço e armazena o valor de virtual_base
    ldr r3, =virtual_base   
    str r2, [r3]


    pop {r4-r7, lr}

    bx lr        

.section .rodata
filename:
    .asciz "/dev/mem"

.align 2
.section .text
.global sendInstruction
.type sendInstruction, %function
sendInstruction:
    @dataA = r0
    @dataB = r1
    push {r4, lr}

    ldr r3, =virtual_base
    ldr r3, [r3]

    loop:
      @ verifica se wreg esta full
      mov r5, #0xb0
      ldr r4, [r3, r5]

      cmp r4, #0
      bne loop

    mov r2, #0
    mov r4, #0xc0

    str r2, [r3, r4] @ para abertura e armazenamento do dataA e B

    mov r4, #0x80

    str r0, [r3, r4] @ armazena o valor passado de dataA para o endereço de dataA

    mov r4, #0x70

    str r1, [r3, r4] @ armazena o valor passado de dataA para o endereço de dataA

    mov r2, #1

    str r2, [r3, #0xc0] @para abertura e armazenamento do dataA e B

    mov r2, #0

    str r2, [r3, #0xc0] @ para abertura e armazenamento do dataA e B

    pop {r4, lr}
    bx lr
.align 2
.section .text
.global set_background_block
.type set_background_block, %function
isFull:
      @ verifica se wreg esta full
    ldr r3, =virtual_base
    ldr r3, [r3]
    mov r5, #0xb0
    ldr r4, [r3, r5]
    cmp r4, #0
    bne isFull
.align 2
.section .text
.global isFull
.type isFull, %function


set_background_block:
    @ r0 = column, r1= line, r2 = r, r3 = g, sp+4 = b; se tiver outro, o b estará em sp+8 e o novo em sp+4
    push {r4-r6,lr}

    ldr r4, [sp, #16]
    mov r6, #80
    mul r6, r1, r6
    add r6, r6, r0 @ r6 = adress

    @ dataA Builder
    lsl r6, r6, #4
    orr r6, r6, #2

    @ color = r4
    lsl r4, r4, #3
    add r4, r4, r3
    lsl r4, r4, #3
    add r4, r4, r2

    mov r0, r6
    mov r1, r4

    bl sendInstruction

    pop {r4-r6,lr}
    bx lr
          
open_button: 

    ldr r1, =virtual_base
    @ldr r1, [r1]
    @mov r2, #0x0
    ldr r0, [r1, #0x0]
    bx lr
.align 2
.section .text
.global open_button
.type open_button, %function

.align 2
.section .text
.global set_background_color
.type set_background_color, %function

set_background_color:
    @R, G, B
    push {lr}

    @ dataA Builder
    lsl r3, r3, #4

    @ dataB
    lsl r2, r2, #3
    orr r2, r2, r1

    lsl r2, r2, #3
    orr r2, r2, r0

    mov r0, r3
    mov r1, r2
    bl sendInstruction

    pop {lr}
    bx lr

.align 2
.section .text
.global set_sprite
.type set_sprite, %function

set_sprite:
    @registrador, x, y, offset, activation_bit
    push {r4-r6,lr}

    ldr r4, [sp, #16] @activation_bit

    @ dataA Builder
    lsl r0, r0, #4

    @ dataB
    lsl r4, r4, #10
    orr r4, r4, r1
    lsl r4, r4, #10
    orr r4, r4, r2
    lsl r4, r4, #9
    orr r4, r4, r3

    mov r1, r4
    bl sendInstruction

    pop {r4-r6,lr}
    bx lr

.align 2
.section .text
.global set_pixelSprite
.type set_pixelSprite, %function

set_pixelSprite:
    @endereço, color
    push {lr}

    @ dataA Builder
    lsl r0, r0, #4
    add r0, r0, #1

    @ dataB = r1
    bl sendInstruction
    
    pop {lr}
    bx lr




.align 2
.section .text
.global setPolygon
.type setPolygon, %function


setPolygon:
    @address, opcode, color, form, mult, ref_point_x, ref_point_y
    push {r4,r5,lr}

    ldr r4, [sp, #4] @ mult
    ldr r5, [sp, #8] @ref_X

    @ dataA Builder
    lsl r0, r0, #4 @ caso necessário fazer com o addres para o caso 3
    add r0, r0, #3

    @dataB
    lsl r2, r2, #9 @color
    orr r2, r2, r1

    lsl r2, r2, #4 @mult
    orr r2, r2, r3 

    lsl r2, r2, #9 @ref_y
    orr r3, r3, r5

    lsl r2, r2, #9 @ref_x
    orr r2, r2, r4

    mov r0, r0
    mov r1, r2
    bl sendInstruction

    pop {r4,r5,lr}
    bx lr
    
