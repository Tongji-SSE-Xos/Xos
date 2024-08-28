#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define KBD_DATA_PORT 0x60 // 键盘数据端口
#define KBD_CTRL_PORT 0x64 // 键盘控制端口

#define KBD_CMD_SET_LED 0xED // 设置 LED 命令
#define KBD_CMD_ACKNOWLEDGE 0xFA // ACK 响应

#define KEY_NONE_VISIBLE 0 // 不可见字符

#define PRINT_SCREEN_RELEASE_CODE 0xB7

typedef enum
{
    KEY_NONE,
    KEY_ESCAPE,
    KEY_NUM1,
    KEY_NUM2,
    KEY_NUM3,
    KEY_NUM4,
    KEY_NUM5,
    KEY_NUM6,
    KEY_NUM7,
    KEY_NUM8,
    KEY_NUM9,
    KEY_NUM0,
    KEY_DASH,
    KEY_EQUALS,
    KEY_BACKSPACE_KEY,
    KEY_TAB_KEY,
    KEY_LETTER_Q,
    KEY_LETTER_W,
    KEY_LETTER_E,
    KEY_LETTER_R,
    KEY_LETTER_T,
    KEY_LETTER_Y,
    KEY_LETTER_U,
    KEY_LETTER_I,
    KEY_LETTER_O,
    KEY_LETTER_P,
    KEY_LEFT_BRACKET,
    KEY_RIGHT_BRACKET,
    KEY_RETURN,
    KEY_LEFT_CTRL,
    KEY_LETTER_A,
    KEY_LETTER_S,
    KEY_LETTER_D,
    KEY_LETTER_F,
    KEY_LETTER_G,
    KEY_LETTER_H,
    KEY_LETTER_J,
    KEY_LETTER_K,
    KEY_LETTER_L,
    KEY_SEMICOLON_KEY,
    KEY_APOSTROPHE,
    KEY_GRAVE_ACCENT,
    KEY_LEFT_SHIFT,
    KEY_BACKSLASH_KEY,
    KEY_LETTER_Z,
    KEY_LETTER_X,
    KEY_LETTER_C,
    KEY_LETTER_V,
    KEY_LETTER_B,
    KEY_LETTER_N,
    KEY_LETTER_M,
    KEY_COMMA_KEY,
    KEY_PERIOD,
    KEY_SLASH_KEY,
    KEY_RIGHT_SHIFT,
    KEY_ASTERISK,
    KEY_LEFT_ALT,
    KEY_SPACEBAR,
    KEY_CAPS_LOCK,
    KEY_FUNCTION1,
    KEY_FUNCTION2,
    KEY_FUNCTION3,
    KEY_FUNCTION4,
    KEY_FUNCTION5,
    KEY_FUNCTION6,
    KEY_FUNCTION7,
    KEY_FUNCTION8,
    KEY_FUNCTION9,
    KEY_FUNCTION10,
    KEY_NUM_LOCK,
    KEY_SCROLL_LOCK,
    KEY_KEYPAD_7,
    KEY_KEYPAD_8,
    KEY_KEYPAD_9,
    KEY_KEYPAD_MINUS,
    KEY_KEYPAD_4,
    KEY_KEYPAD_5,
    KEY_KEYPAD_6,
    KEY_KEYPAD_PLUS,
    KEY_KEYPAD_1,
    KEY_KEYPAD_2,
    KEY_KEYPAD_3,
    KEY_KEYPAD_0,
    KEY_KEYPAD_PERIOD,
    KEY_54_RESERVED,
    KEY_55_RESERVED,
    KEY_56_RESERVED,
    KEY_FUNCTION11,
    KEY_FUNCTION12,
    KEY_59_RESERVED,
    KEY_LEFT_WINDOWS,
    KEY_RIGHT_WINDOWS,
    KEY_CLIPBOARD_KEY,
    KEY_5D_RESERVED,
    KEY_5E_RESERVED,
    KEY_PRINT_SCREEN_KEY,
} KEY;

static char keymap[][4] = {
    /* 扫描码 无 Shift组合 有 Shift组合 特殊状态 */
    /* ---------------------------------- */
    /* 0x00 */ {INV, INV, false, false},   // 空
    /* 0x01 */ {0x1b, 0x1b, false, false}, // ESC
    /* 0x02 */ {'1', '!', false, false},
    /* 0x03 */ {'2', '@', false, false},
    /* 0x04 */ {'3', '#', false, false},
    /* 0x05 */ {'4', '$', false, false},
    /* 0x06 */ {'5', '%', false, false},
    /* 0x07 */ {'6', '^', false, false},
    /* 0x08 */ {'7', '&', false, false},
    /* 0x09 */ {'8', '*', false, false},
    /* 0x0A */ {'9', '(', false, false},
    /* 0x0B */ {'0', ')', false, false},
    /* 0x0C */ {'-', '_', false, false},
    /* 0x0D */ {'=', '+', false, false},
    /* 0x0E */ {'\b', '\b', false, false}, // 退格
    /* 0x0F */ {'\t', '\t', false, false}, // TAB
    /* 0x10 */ {'q', 'Q', false, false},
    /* 0x11 */ {'w', 'W', false, false},
    /* 0x12 */ {'e', 'E', false, false},
    /* 0x13 */ {'r', 'R', false, false},
    /* 0x14 */ {'t', 'T', false, false},
    /* 0x15 */ {'y', 'Y', false, false},
    /* 0x16 */ {'u', 'U', false, false},
    /* 0x17 */ {'i', 'I', false, false},
    /* 0x18 */ {'o', 'O', false, false},
    /* 0x19 */ {'p', 'P', false, false},
    /* 0x1A */ {'[', '{', false, false},
    /* 0x1B */ {']', '}', false, false},
    /* 0x1C */ {'\n', '\n', false, false}, // 回车
    /* 0x1D */ {INV, INV, false, false},   // 左CTRL
    /* 0x1E */ {'a', 'A', false, false},
    /* 0x1F */ {'s', 'S', false, false},
    /* 0x20 */ {'d', 'D', false, false},
    /* 0x21 */ {'f', 'F', false, false},
    /* 0x22 */ {'g', 'G', false, false},
    /* 0x23 */ {'h', 'H', false, false},
    /* 0x24 */ {'j', 'J', false, false},
    /* 0x25 */ {'k', 'K', false, false},
    /* 0x26 */ {'l', 'L', false, false},
    /* 0x27 */ {';', ':', false, false},
    /* 0x28 */ {'\'', '"', false, false},
    /* 0x29 */ {'`', '~', false, false},
    /* 0x2A */ {INV, INV, false, false},   // 左SHIFT
    /* 0x2B */ {'\\', '|', false, false},
    /* 0x2C */ {'z', 'Z', false, false},
    /* 0x2D */ {'x', 'X', false, false},
    /* 0x2E */ {'c', 'C', false, false},
    /* 0x2F */ {'v', 'V', false, false},
    /* 0x30 */ {'b', 'B', false, false},
    /* 0x31 */ {'n', 'N', false, false},
    /* 0x32 */ {'m', 'M', false, false},
    /* 0x33 */ {',', '<', false, false},
    /* 0x34 */ {'.', '>', false, false},
    /* 0x35 */ {'/', '?', false, false},
    /* 0x36 */ {INV, INV, false, false},   // 右SHIFT
    /* 0x37 */ {'*', '*', false, false},   // 小键盘 *
    /* 0x38 */ {INV, INV, false, false},   // 左ALT
    /* 0x39 */ {' ', ' ', false, false},   // 空格
    /* 0x3A */ {INV, INV, false, false},   // 大写锁定
    /* 0x3B */ {INV, INV, false, false},   // F1
    /* 0x3C */ {INV, INV, false, false},   // F2
    /* 0x3D */ {INV, INV, false, false},   // F3
    /* 0x3E */ {INV, INV, false, false},   // F4
    /* 0x3F */ {INV, INV, false, false},   // F5
    /* 0x40 */ {INV, INV, false, false},   // F6
    /* 0x41 */ {INV, INV, false, false},   // F7
    /* 0x42 */ {INV, INV, false, false},   // F8
    /* 0x43 */ {INV, INV, false, false},   // F9
    /* 0x44 */ {INV, INV, false, false},   // F10
    /* 0x45 */ {INV, INV, false, false},   // 数字锁定
    /* 0x46 */ {INV, INV, false, false},   // 滚动锁定
    /* 0x47 */ {'7', INV, false, false},   // 小键盘 7 - Home
    /* 0x48 */ {'8', INV, false, false},   // 小键盘 8 - 上
    /* 0x49 */ {'9', INV, false, false},   // 小键盘 9 - PageUp
    /* 0x4A */ {'-', '-', false, false},   // 小键盘 -
    /* 0x4B */ {'4', INV, false, false},   // 小键盘 4 - 左
    /* 0x4C */ {'5', INV, false, false},   // 小键盘 5
    /* 0x4D */ {'6', INV, false, false},   // 小键盘 6 - 右
    /* 0x4E */ {'+', '+', false, false},   // 小键盘 +
    /* 0x4F */ {'1', INV, false, false},   // 小键盘 1 - End
    /* 0x50 */ {'2', INV, false, false},   // 小键盘 2 - 下
    /* 0x51 */ {'3', INV, false, false},   // 小键盘 3 - PageDown
    /* 0x52 */ {'0', INV, false, false},   // 小键盘 0 - Insert
    /* 0x53 */ {'.', 0x7F, false, false},  // 小键盘 . - Delete
    /* 0x54 */ {INV, INV, false, false},   //
    /* 0x55 */ {INV, INV, false, false},   //
    /* 0x56 */ {INV, INV, false, false},   //
    /* 0x57 */ {INV, INV, false, false},   // F11
    /* 0x58 */ {INV, INV, false, false},   // F12
    /* 0x59 */ {INV, INV, false, false},   //
    /* 0x5A */ {INV, INV, false, false},   //
    /* 0x5B */ {INV, INV, false, false},   // 左Windows键
    /* 0x5C */ {INV, INV, false, false},   // 右Windows键
    /* 0x5D */ {INV, INV, false, false},   // 应用键
    /* 0x5E */ {INV, INV, false, false},   //

    // Print Screen 是强制定义 本身是 0xB7
    /* 0x5F */ {INV, INV, false, false},   // PrintScreen
};

static lock_t lock;    // 锁
static task_t *waiter; // 等待输入的任务

#define BUFFER_SIZE 64        // 输入缓冲区大小
static char buf[BUFFER_SIZE]; // 输入缓冲区
static fifo_t fifo;           // 循环队列

static bool capslock_active; // 大写锁定状态
static bool scrlock_active;  // 滚动锁定状态
static bool numlock_active;  // 数字锁定状态
static bool extcode_active;  // 扩展码状态

// 键盘状态宏定义
#define ctrl_active (keymap[KEY_CTRL_L][2] || keymap[KEY_CTRL_L][3]) // 用于检查 CTRL 键是否被按下
#define alt_active (keymap[KEY_ALT_L][2] || keymap[KEY_ALT_L][3])     // 用于检查 ALT 键是否被按下
#define shift_active (keymap[KEY_SHIFT_L][2] || keymap[KEY_SHIFT_R][2]) // 用于检查 SHIFT 键是否被按下

static void keyboard_wait() {
    while (inb(KEYBOARD_CTRL_PORT) & 0x02); // 读取键盘状态，直到缓冲区为空
}

static void keyboard_ack() {
    while (inb(KEYBOARD_DATA_PORT) != KEYBOARD_CMD_ACK); // 等待键盘确认收到命令
}

static void update_leds()
{
    // 滚动锁定：第 0 位
    // 数字锁定：第 1 位
    // 大写锁定：第 2 位
    u8 leds = (capslock_active << 2) | (numlock_active << 1) | scrlock_active; // 根据键盘状态设置 LED 状态
    keyboard_wait(); // 等待缓冲区为空
    outb(KEYBOARD_DATA_PORT, KEYBOARD_CMD_LED); // 发送 LED 设置命令
    keyboard_ack(); // 等待键盘确认
    keyboard_wait(); // 等待缓冲区为空
    outb(KEYBOARD_DATA_PORT, leds); // 设置 LED 状态
    keyboard_ack(); // 等待键盘确认
}

extern int tty_rx_notify();

// 键盘中断处理函数，处理按键按下与松开的事件
static void keyboard_handler(int vector) {
    assert(vector == 0x21); // 确保中断向量正确
    outb(0x20, 0x20);       // 发送中断结束信号

    u8 scancode = inb(KEYBOARD_DATA_PORT); // 读取扫描码

    if (scancode == 0xE0 || scancode == 0xE1) {
        extcode_active = true; // 检测到扩展码
        return;
    }

    bool break_code = scancode & 0x80; 
    u16 mapped_code = (extcode_active << 8) | (scancode & 0x7F); 
    extcode_active = false; 

    if (mapped_code == CODE_PRINT_SCREEN_DOWN) {
        break_code = false; 
    }

    KEY key = (KEY)mapped_code; 

    if (break_code) {
        // 处理修饰键释放事件
        if (key == KEY_CTRL_L || key == KEY_ALT_L || key == KEY_SHIFT_L || key == KEY_SHIFT_R) {
            keymap[key][2] = keymap[key][3] = false; 
        }
        return;
    }

    // 处理修饰键按下事件
    switch (key) {
        case KEY_CTRL_L:
        case KEY_ALT_L:
        case KEY_SHIFT_L:
        case KEY_SHIFT_R:
            keymap[key][2] = keymap[key][3] = true; 
            return;
        case KEY_CAPSLOCK:
            capslock_active = !capslock_active; 
            update_leds(); 
            return;
        case KEY_NUMLOCK:
            numlock_active = !numlock_active; 
            update_leds(); 
            return;
        case KEY_SCRLOCK:
            scrlock_active = !scrlock_active; 
            update_leds(); 
            return;
        case KEY_PRINT_SCREEN:
            return;
        default:
            break;
    }
    
    // 处理大写锁定和 SHIFT 键的状态
    bool caps = key >= KEY_A && key <= KEY_Z && capslock_active; 
    bool shift = shift_active != caps;

    char ch = keymap[key][shift]; 
    if (ch == INV) return;

    if (fifo_full(&fifo)) return; 

    fifo_put(&fifo, ch); 

    if (waiter) { 
        task_unblock(waiter, EOK); 
        waiter = NULL; 
    }

    tty_rx_notify(); 
}

u32 keyboard_read(void *dev, char *buf, u32 count) {
    if (!buf || count == 0) return 0; // 添加参数有效性检查

    lock_acquire(&lock);
    int nr = 0;
    int timeout = 5000; // 设置超时时间，单位为毫秒

    while (nr < count) {
        int start = get_current_time(); // 获取当前时间戳
        while (fifo_empty(&fifo)) {
            waiter = running_task();
            task_block(waiter, NULL, TASK_BLOCKED, TIMELESS);

            // 检查是否超时
            if (get_current_time() - start > timeout) {
                lock_release(&lock);
                return nr; // 返回已读取的字符数
            }
        }
        buf[nr++] = fifo_get(&fifo);
    }
    lock_release(&lock);
    return count;
}

void keyboard_init()
{
    // 初始化 FIFO 缓冲区
    fifo_init(&fifo, buf, BUFFER_SIZE);

    // 初始化键盘状态
    capslock_active = false;
    scrlock_active = false;
    numlock_active = false;
    extcode_active = false;

    // 设置 LED 状态
    update_leds(); 

    // 初始化锁
    lock_init(&lock);

    // 初始化等待任务指针
    waiter = NULL;

    // 设置中断处理程序
    set_interrupt_handler(IRQ_KEYBOARD, keyboard_handler);

    // 启用键盘中断
    set_interrupt_mask(IRQ_KEYBOARD, true);

    // 注册设备
    device_install(
        DEV_CHAR, DEV_KEYBOARD,
        NULL, "keyboard", 0,
        NULL, keyboard_read, NULL
    );
}