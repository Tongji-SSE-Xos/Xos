[bits 32]

section .text ; 代码段

global inb ; 将 inb 导出
inb:
    push ebp
    mov ebp, esp ; 建立新的栈帧

    xor eax, eax ; 清空 eax
    mov edx, [ebp + 8] ; 获取端口号到 edx
    in al, dx ; 从端口号 dx 读取 8 位到 al

    nop ; 添加一些延迟
    nop ; 添加一些延迟
    nop ; 添加一些延迟

    leave ; 恢复栈帧
    ret

global outb
outb:
    push ebp
    mov ebp, esp ; 建立新的栈帧

    mov edx, [ebp + 8] ; 获取端口号到 edx
    mov eax, [ebp + 12] ; 获取要写入的值到 eax
    out dx, al ; 将 al 中的 8 位数据写入端口 dx

    nop ; 添加一些延迟
    nop ; 添加一些延迟
    nop ; 添加一些延迟

    leave ; 恢复栈帧
    ret

global inw
inw:
    push ebp
    mov ebp, esp ; 建立新的栈帧

    xor eax, eax ; 清空 eax
    mov edx, [ebp + 8] ; 获取端口号到 edx
    in ax, dx ; 从端口号 dx 读取 16 位到 ax

    nop ; 添加一些延迟
    nop ; 添加一些延迟
    nop ; 添加一些延迟

    leave ; 恢复栈帧
    ret

global outw
outw:
    push ebp
    mov ebp, esp ; 建立新的栈帧

    mov edx, [ebp + 8] ; 获取端口号到 edx
    mov eax, [ebp + 12] ; 获取要写入的值到 eax
    out dx, ax ; 将 ax 中的 16 位数据写入端口 dx

    nop ; 添加一些延迟
    nop ; 添加一些延迟
    nop ; 添加一些延迟

    leave ; 恢复栈帧
    ret

global inl ; 将 inl 导出
inl:
    push ebp
    mov ebp, esp ; 建立新的栈帧

    xor eax, eax ; 清空 eax
    mov edx, [ebp + 8] ; 获取端口号到 edx
    in eax, dx ; 从端口号 dx 读取 32 位到 eax

    nop ; 添加一些延迟
    nop ; 添加一些延迟
    nop ; 添加一些延迟

    leave ; 恢复栈帧
    ret

global outl
outl:
    push ebp
    mov ebp, esp ; 建立新的栈帧

    mov edx, [ebp + 8] ; 获取端口号到 edx
    mov eax, [ebp + 12] ; 获取要写入的值到 eax
    out dx, eax ; 将 eax 中的 32 位数据写入端口 dx

    nop ; 添加一些延迟
    nop ; 添加一些延迟
    nop ; 添加一些延迟

    leave ; 恢复栈帧
    ret

