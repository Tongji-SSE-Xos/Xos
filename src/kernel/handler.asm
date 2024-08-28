[bits 32]
; 中断处理入口

extern handler_table
extern task_signal

section .text

%macro INTERRUPT_HANDLER 2
interrupt_handler_%1:
    ; xchg bx, bx
%ifn %2
    push 0x20222202
%endif
    push %1               ; 将中断向量压入栈，传递给中断入口
    jmp interrupt_entry   ; 跳转到通用中断处理入口
%endmacro

interrupt_entry:

    ; 保存上下文环境
    push ds
    push es
    push fs
    push gs
    pusha

    ; 获取中断向量号
    mov eax, [esp + 12 * 4]

    ; 将中断向量号传递给处理函数
    push eax

    ; 调用相应的中断处理函数，从 handler_table 中查找
    call [handler_table + eax * 4]

global interrupt_exit
interrupt_exit:

    ; 恢复栈指针，清理参数
    add esp, 4

    ; 执行信号处理
    call task_signal

    ; 恢复上下文环境
    popa
    pop gs
    pop fs
    pop es
    pop ds

    ; 调整栈指针，跳过中断向量和错误码
    add esp, 8
    iret

; 定义各个中断处理入口
INTERRUPT_HANDLER 0x00, 0  ; 除零错误
INTERRUPT_HANDLER 0x01, 0  ; 调试异常
INTERRUPT_HANDLER 0x02, 0  ; 不可屏蔽中断
INTERRUPT_HANDLER 0x03, 0  ; 断点异常

INTERRUPT_HANDLER 0x04, 0  ; 溢出异常
INTERRUPT_HANDLER 0x05, 0  ; 边界检查异常
INTERRUPT_HANDLER 0x06, 0  ; 无效操作码
INTERRUPT_HANDLER 0x07, 0  ; 设备不可用

INTERRUPT_HANDLER 0x08, 1  ; 双重故障
INTERRUPT_HANDLER 0x09, 0  ; 协处理器段溢出
INTERRUPT_HANDLER 0x0A, 1  ; 无效TSS
INTERRUPT_HANDLER 0x0B, 1  ; 段不存在

INTERRUPT_HANDLER 0x0C, 1  ; 堆栈段故障
INTERRUPT_HANDLER 0x0D, 1  ; 通用保护异常
INTERRUPT_HANDLER 0x0E, 1  ; 页面错误
INTERRUPT_HANDLER 0x0F, 0  ; 保留

INTERRUPT_HANDLER 0x10, 0  ; x87浮点异常
INTERRUPT_HANDLER 0x11, 1  ; 对齐检查
INTERRUPT_HANDLER 0x12, 0  ; 机器检查
INTERRUPT_HANDLER 0x13, 0  ; SIMD浮点异常

INTERRUPT_HANDLER 0x14, 0  ; 虚拟化异常
INTERRUPT_HANDLER 0x15, 1  ; 控制保护异常
INTERRUPT_HANDLER 0x16, 0  ; 保留
INTERRUPT_HANDLER 0x17, 0  ; 保留

INTERRUPT_HANDLER 0x18, 0  ; 保留
INTERRUPT_HANDLER 0x19, 0  ; 保留
INTERRUPT_HANDLER 0x1A, 0  ; 保留
INTERRUPT_HANDLER 0x1B, 0  ; 保留

INTERRUPT_HANDLER 0x1C, 0  ; 保留
INTERRUPT_HANDLER 0x1D, 0  ; 保留
INTERRUPT_HANDLER 0x1E, 0  ; 保留
INTERRUPT_HANDLER 0x1F, 0  ; 保留

INTERRUPT_HANDLER 0x20, 0  ; 时钟中断
INTERRUPT_HANDLER 0x21, 0  ; 键盘中断
INTERRUPT_HANDLER 0x22, 0  ; 级联中断
INTERRUPT_HANDLER 0x23, 0  ; 串口2
INTERRUPT_HANDLER 0x24, 0  ; 串口1
INTERRUPT_HANDLER 0x25, 0  ; 声卡中断
INTERRUPT_HANDLER 0x26, 0  ; 软盘中断
INTERRUPT_HANDLER 0x27, 0  ; 保留
INTERRUPT_HANDLER 0x28, 0  ; 实时时钟
INTERRUPT_HANDLER 0x29, 0  ; 保留
INTERRUPT_HANDLER 0x2A, 0  ; 保留
INTERRUPT_HANDLER 0x2B, 0  ; 网卡中断
INTERRUPT_HANDLER 0x2C, 0  ; 保留
INTERRUPT_HANDLER 0x2D, 0  ; 保留
INTERRUPT_HANDLER 0x2E, 0  ; 硬盘主通道
INTERRUPT_HANDLER 0x2F, 0  ; 硬盘从通道

; 中断入口函数指针表
section .data
global handler_entry_table
handler_entry_table:
    dd interrupt_handler_0x00
    dd interrupt_handler_0x01
    dd interrupt_handler_0x02
    dd interrupt_handler_0x03
    dd interrupt_handler_0x04
    dd interrupt_handler_0x05
    dd interrupt_handler_0x06
    dd interrupt_handler_0x07
    dd interrupt_handler_0x08
    dd interrupt_handler_0x09
    dd interrupt_handler_0x0A
    dd interrupt_handler_0x0B
    dd interrupt_handler_0x0C
    dd interrupt_handler_0x0D
    dd interrupt_handler_0x0E
    dd interrupt_handler_0x0F
    dd interrupt_handler_0x10
    dd interrupt_handler_0x11
    dd interrupt_handler_0x12
    dd interrupt_handler_0x13
    dd interrupt_handler_0x14
    dd interrupt_handler_0x15
    dd interrupt_handler_0x16
    dd interrupt_handler_0x17
    dd interrupt_handler_0x18
    dd interrupt_handler_0x19
    dd interrupt_handler_0x1A
    dd interrupt_handler_0x1B
    dd interrupt_handler_0x1C
    dd interrupt_handler_0x1D
    dd interrupt_handler_0x1E
    dd interrupt_handler_0x1F
    dd interrupt_handler_0x20
    dd interrupt_handler_0x21
    dd interrupt_handler_0x22
    dd interrupt_handler_0x23
    dd interrupt_handler_0x24
    dd interrupt_handler_0x25
    dd interrupt_handler_0x26
    dd interrupt_handler_0x27
    dd interrupt_handler_0x28
    dd interrupt_handler_0x29
    dd interrupt_handler_0x2A
    dd interrupt_handler_0x2B
    dd interrupt_handler_0x2C
    dd interrupt_handler_0x2D
    dd interrupt_handler_0x2E
    dd interrupt_handler_0x2F

section .text

extern syscall_check
extern syscall_table
global syscall_handler
syscall_handler:
    ; xchg bx, bx

    ; 验证系统调用号
    push eax
    call syscall_check
    add esp, 4

    push 0x20222202

    push 0x80

    ; 保存上下文环境
    push ds
    push es
    push fs
    push gs
    pusha

    push 0x80    ; 向中断处理函数传递系统调用中断向量
    ; xchg bx, bx

    ; 保存系统调用的参数
    push ebp    ; 参数6
    push edi    ; 参数5
    push esi    ; 参数4
    push edx    ; 参数3
    push ecx    ; 参数2
    push ebx    ; 参数1

    ; 调用系统调用处理函数，从 syscall_table 中查找
    call [syscall_table + eax * 4]

    ; 恢复栈，清理参数
    add esp, 6 * 4

    ; 设置返回值到 eax，并修改栈中的 eax
    mov dword [esp + 8 * 4], eax

    ; 跳转到中断退出处理
    jmp interrupt_exit
