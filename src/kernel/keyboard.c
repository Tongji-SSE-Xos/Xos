#include "hyc.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define KEYBOARD_DATA_PORT 0x60 // 数据端口
#define KEYBOARD_CTRL_PORT 0x64 // 控制端口

#define KEYBOARD_CMD_LED 0xED // 设置 LED 状态
#define KEYBOARD_CMD_ACK 0xFA // ACK

#define INV 0 // 不可见字符

#define CODE_PRINT_SCREEN_DOWN 0xB7

typedef enum
{
    KEY_NONE,
    KEY_ESC,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_Q,
    KEY_W,
    KEY_E,
    KEY_R,
    KEY_T,
    KEY_Y,
    KEY_U,
    KEY_I,
    KEY_O,
    KEY_P,
    KEY_BRACKET_L,
    KEY_BRACKET_R,
    KEY_ENTER,
    KEY_CTRL_L,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_SEMICOLON,
    KEY_QUOTE,
    KEY_BACKQUOTE,
    KEY_SHIFT_L,
    KEY_BACKSLASH,
    KEY_Z,
    KEY_X,
    KEY_C,
    KEY_V,
    KEY_B,
    KEY_N,
    KEY_M,
    KEY_COMMA,
    KEY_POINT,
    KEY_SLASH,
    KEY_SHIFT_R,
    KEY_STAR,
    KEY_ALT_L,
    KEY_SPACE,
    KEY_CAPSLOCK,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_NUMLOCK,
    KEY_SCRLOCK,
    KEY_PAD_7,
    KEY_PAD_8,
    KEY_PAD_9,
    KEY_PAD_MINUS,
    KEY_PAD_4,
    KEY_PAD_5,
    KEY_PAD_6,
    KEY_PAD_PLUS,
    KEY_PAD_1,
    KEY_PAD_2,
    KEY_PAD_3,
    KEY_PAD_0,
    KEY_PAD_POINT,
    KEY_54,
    KEY_55,
    KEY_56,
    KEY_F11,
    KEY_F12,
    KEY_59,
    KEY_WIN_L,
    KEY_WIN_R,
    KEY_CLIPBOARD,
    KEY_5D,
    KEY_5E,
    KEY_PRINT_SCREEN,
} KEY;

static char keymap[][4] = {
    /* 扫描码 未与 shift 组合  与 shift 组合 以及相关状态 */
    /* ---------------------------------- */
    /* 0x00 */ {INV, INV, false, false},   // NULL
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
    /* 0x0E */ {'\b', '\b', false, false}, // BACKSPACE
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
    /* 0x1C */ {'\n', '\n', false, false}, // ENTER
    /* 0x1D */ {INV, INV, false, false},   // CTRL_L
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
    /* 0x2A */ {INV, INV, false, false}, // SHIFT_L
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
    /* 0x36 */ {INV, INV, false, false},  // SHIFT_R
    /* 0x37 */ {'*', '*', false, false},  // PAD *
    /* 0x38 */ {INV, INV, false, false},  // ALT_L
    /* 0x39 */ {' ', ' ', false, false},  // SPACE
    /* 0x3A */ {INV, INV, false, false},  // CAPSLOCK
    /* 0x3B */ {INV, INV, false, false},  // F1
    /* 0x3C */ {INV, INV, false, false},  // F2
    /* 0x3D */ {INV, INV, false, false},  // F3
    /* 0x3E */ {INV, INV, false, false},  // F4
    /* 0x3F */ {INV, INV, false, false},  // F5
    /* 0x40 */ {INV, INV, false, false},  // F6
    /* 0x41 */ {INV, INV, false, false},  // F7
    /* 0x42 */ {INV, INV, false, false},  // F8
    /* 0x43 */ {INV, INV, false, false},  // F9
    /* 0x44 */ {INV, INV, false, false},  // F10
    /* 0x45 */ {INV, INV, false, false},  // NUMLOCK
    /* 0x46 */ {INV, INV, false, false},  // SCRLOCK
    /* 0x47 */ {'7', INV, false, false},  // pad 7 - Home
    /* 0x48 */ {'8', INV, false, false},  // pad 8 - Up
    /* 0x49 */ {'9', INV, false, false},  // pad 9 - PageUp
    /* 0x4A */ {'-', '-', false, false},  // pad -
    /* 0x4B */ {'4', INV, false, false},  // pad 4 - Left
    /* 0x4C */ {'5', INV, false, false},  // pad 5
    /* 0x4D */ {'6', INV, false, false},  // pad 6 - Right
    /* 0x4E */ {'+', '+', false, false},  // pad +
    /* 0x4F */ {'1', INV, false, false},  // pad 1 - End
    /* 0x50 */ {'2', INV, false, false},  // pad 2 - Down
    /* 0x51 */ {'3', INV, false, false},  // pad 3 - PageDown
    /* 0x52 */ {'0', INV, false, false},  // pad 0 - Insert
    /* 0x53 */ {'.', 0x7F, false, false}, // pad . - Delete
    /* 0x54 */ {INV, INV, false, false},  //
    /* 0x55 */ {INV, INV, false, false},  //
    /* 0x56 */ {INV, INV, false, false},  //
    /* 0x57 */ {INV, INV, false, false},  // F11
    /* 0x58 */ {INV, INV, false, false},  // F12
    /* 0x59 */ {INV, INV, false, false},  //
    /* 0x5A */ {INV, INV, false, false},  //
    /* 0x5B */ {INV, INV, false, false},  // Left Windows
    /* 0x5C */ {INV, INV, false, false},  // Right Windows
    /* 0x5D */ {INV, INV, false, false},  // Clipboard
    /* 0x5E */ {INV, INV, false, false},  //

    // Print Screen 是强制定义 本身是 0xB7
    /* 0x5F */ {INV, INV, false, false}, // PrintScreen
};

static lock_t lock;    // 锁
static task_t *waiter; // 等待输入的任务

#define BUFFER_SIZE 64        // 输入缓冲区大小
static char buf[BUFFER_SIZE]; // 输入缓冲区
static fifo_t fifo;           // 循环队列

static bool capslock_state; // 大写锁定
static bool scrlock_state;  // 滚动锁定
static bool numlock_state;  // 数字锁定
static bool extcode_state;  // 扩展码状态

// 键盘状态宏
#define ctrl_state (keymap[KEY_CTRL_L][2] || keymap[KEY_CTRL_L][3]) // 用于检查 CTRL 键状态
#define alt_state (keymap[KEY_ALT_L][2] || keymap[KEY_ALT_L][3])     // 用于检查 ALT 键状态
#define shift_state (keymap[KEY_SHIFT_L][2] || keymap[KEY_SHIFT_R][2]) // 用于检查 SHIFT 键状态


static void keyboard_wait() {
    while (inb(KEYBOARD_CTRL_PORT) & 0x02); // 读取键盘缓冲区，直到为空
}

static void keyboard_ack() {
    while (inb(KEYBOARD_DATA_PORT) != KEYBOARD_CMD_ACK); // 等待键盘发送 ACK，表示收到了命令
}

static void set_leds()
{
    // 滚动锁定：第 0 位
    // 数字锁定：第 1 位
    // 大写锁定：第 2 位
    u8 leds = (capslock_state << 2) | (numlock_state << 1) | scrlock_state; // 根据键盘状态设置 LED 灯
    keyboard_wait(); // 等待缓冲区为空
    // 设置 LED 命令
    outb(KEYBOARD_DATA_PORT, KEYBOARD_CMD_LED);
    keyboard_ack(); //等待键盘给 ACK
    keyboard_wait(); // 等待缓冲区为空
    outb(KEYBOARD_DATA_PORT, leds); // 设置 LED 灯状态
    keyboard_ack(); //等待键盘给 ACK
}

extern int tty_rx_notify();

// 用于判断并相应各种按键操作，包括了部分按键的LED灯
static void keyboard_handler(int vector) {
    assert(vector == 0x21); // 确认中断向量正确
    outb(0x20, 0x20);       // 发送中断结束信号

    u8 scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode == 0xE0 || scancode == 0xE1) {
        extcode_state = true; // 处理扩展按键码
        return;
    }

    bool break_code = scancode & 0x80; 
    u16 mapped_code = (extcode_state << 8) | (scancode & 0x7F); 
    extcode_state = false; 

    if (mapped_code == CODE_PRINT_SCREEN_DOWN) {
        break_code = false; 
    }

    KEY key = (KEY)mapped_code; 

    if (break_code) {
        // 处理特殊按键释放状态
        if (key == KEY_CTRL_L || key == KEY_ALT_L || key == KEY_SHIFT_L || key == KEY_SHIFT_R) {
            keymap[key][2] = keymap[key][3] = false; 
        }
        return;
    }

    // 处理特殊按键按下状态
    switch (key) {
        case KEY_CTRL_L:
        case KEY_ALT_L:
        case KEY_SHIFT_L:
        case KEY_SHIFT_R:
            keymap[key][2] = keymap[key][3] = true; 
            return;
        case KEY_CAPSLOCK:
            capslock_state = !capslock_state; 
            set_leds(); 
            return;
        case KEY_NUMLOCK:
            numlock_state = !numlock_state; 
            set_leds(); 
            return;
        case KEY_SCRLOCK:
            scrlock_state = !scrlock_state; 
            set_leds(); 
            return;
        case KEY_PRINT_SCREEN:
            return;
        default:
            break;
    }
    
    // 大写字母和 SHIFT 键处理
    bool caps = key >= KEY_A && key <= KEY_Z && capslock_state; 
    bool shift = shift_state != caps;

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
    int timeout = 5000; // 超时时间，假设单位是毫秒

    while (nr < count) {
        int start = get_current_time(); // 获取当前时间
        while (fifo_empty(&fifo)) {
            waiter = running_task();
            task_block(waiter, NULL, TASK_BLOCKED, TIMELESS);

            // 检查超时
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
    capslock_state = false;
    scrlock_state = false;
    numlock_state = false;
    extcode_state = false;

    // 设置 LED 状态
    set_leds(); 

    // 初始化锁
    lock_init(&lock);

    // 初始化等待任务指针
    waiter = NULL;

    // 设置中断处理程序
    set_interrupt_handler(IRQ_KEYBOARD, keyboard_handler);

    // 启用键盘中断
    set_interrupt_mask(IRQ_KEYBOARD, true);

    // 安装设备
    device_install(
        DEV_CHAR, DEV_KEYBOARD,
        NULL, "keyboard", 0,
        NULL, keyboard_read, NULL
    );
}
