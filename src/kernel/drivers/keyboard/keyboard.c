#include "keyboard.h"
#include "../../arch/x86/interrupt_descriptor_table/idt.h"
#include "../../arch/x86/sync/spinlock.h"
#include "../../kernel_services/kernel_services.h"
#include "../../utils/logging/logger.h"
#include "../../arch/x86/task/task.h"
#include "../../utils/console_print/kconsole.h"
#include "../ports/IO.h"
#include <stdbool.h>
#include <stdint.h>

#define MODULE "KEYBOARD"

static volatile key_buffer_t *keyboard_buffer;
static uint8_t modifier_state = 0;
static uint8_t lock_state = 0;
static bool extended_scancode = false;
static spinlock_t keyboard_state_lock = SPINLOCK_INIT;
static volatile uint32_t keyboard_waiting_task = 0;

static const char scancode_to_ascii_normal[] = {
    0,    0,    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0};

static const char scancode_to_ascii_shift[] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,    ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0};

static const uint16_t extended_scancode_map[128] = {
    [0x48] = KEY_UP,
    [0x50] = KEY_DOWN,
    [0x4B] = KEY_LEFT,
    [0x4D] = KEY_RIGHT,
    [0x47] = KEY_HOME,
    [0x4F] = KEY_END,
    [0x49] = KEY_PAGE_UP,
    [0x51] = KEY_PAGE_DOWN,
    [0x52] = KEY_INSERT,
    [0x53] = KEY_DELETE
};

static bool keyboard_is_buffer_empty(void) {
  return keyboard_buffer->head == keyboard_buffer->tail;
}

static bool keyboard_is_buffer_full(void) {
  if (keyboard_buffer == NULL || keyboard_buffer->size == 0)
    return true;
  return (keyboard_buffer->head + 1) % keyboard_buffer->size ==
         keyboard_buffer->tail;
}

static bool keyboard_buffer_write(uint8_t scancode) {
  if (keyboard_is_buffer_full()) {
    return false;
  }
  uint16_t next = (keyboard_buffer->head + 1) % keyboard_buffer->size;
  keyboard_buffer->data[keyboard_buffer->head] = scancode;
  __sync_synchronize();
  keyboard_buffer->head = next;
  return true;
}

static bool keyboard_buffer_read(uint8_t *scancode) {
  if (keyboard_is_buffer_empty()) {
    return false;
  }
  *scancode = keyboard_buffer->data[keyboard_buffer->tail];
  __sync_synchronize();
  keyboard_buffer->tail = (keyboard_buffer->tail + 1) % keyboard_buffer->size;
  return true;
}

static void update_modifier_state(uint8_t scancode, bool is_pressed) {
  switch (scancode) {
  case 0x2A:
  case 0x36:
    if (is_pressed)
      modifier_state |= MODIFIER_SHIFT;
    else
      modifier_state &= ~MODIFIER_SHIFT;
    break;
  case 0x1D:
    if (is_pressed)
      modifier_state |= MODIFIER_CTRL;
    else
      modifier_state &= ~MODIFIER_CTRL;
    break;
  case 0x38:
    if (is_pressed)
      modifier_state |= MODIFIER_ALT;
    else
      modifier_state &= ~MODIFIER_ALT;
    break;
  case 0x3A:
    if (is_pressed)
      lock_state ^= MODIFIER_CAPS_LOCK;
    break;
  case 0x45:
    if (is_pressed)
      lock_state ^= MODIFIER_NUM_LOCK;
    break;
  case 0x46:
    if (is_pressed)
      lock_state ^= MODIFIER_SCROLL_LOCK;
    break;
  }
}

static bool is_break_code(uint8_t scancode) {
  return (scancode & 0x80) != 0;
}

static uint8_t get_make_code(uint8_t scancode) { return scancode & 0x7F; }

bool keyboard_isr(void) {
  uint8_t scancode = inb(KEYBOARD_DATA_PORT);
  outb(0x20, 0x20);

  if (scancode == 0xE0) {
    extended_scancode = true;
    return false;
  }

  uint8_t raw_make = get_make_code(scancode);
  bool is_pressed = !is_break_code(scancode);
  update_modifier_state(raw_make, is_pressed);

  /* Intercept Ctrl+C immediately on keypress */
  if ((modifier_state & MODIFIER_CTRL) && raw_make == 0x2E && is_pressed) {
    kprintln("^C");
    uint32_t fg_pid = task_get_foreground_pid();
    if (fg_pid != 0) {
      log_info(MODULE, "Ctrl+C intercepted: Sending SIGINT to foreground PID %u", fg_pid);
      task_send_signal(fg_pid, SIGINT);
    }
    return false;
  }

  if (extended_scancode) {
    // Push an extended prefix flag or encode into high byte if needed
    // We pass 0xE0 as a marker byte followed by the scancode
    keyboard_buffer_write(0xE0);
    keyboard_buffer_write(scancode);
    extended_scancode = false;
  } else {
    keyboard_buffer_write(scancode);
  }

  if (keyboard_waiting_task != 0) {
    task_unblock(keyboard_waiting_task);
    keyboard_waiting_task = 0;
  }

  return false;
}

void keyboard_initialize(void) {
  extended_scancode = false;
  modifier_state = 0;
  lock_state = 0;
  keyboard_buffer = (key_buffer_t *)kmem_zalloc(sizeof(key_buffer_t));
  keyboard_buffer->size = 256;
  keyboard_buffer->data = (uint8_t *)kmem_zalloc(keyboard_buffer->size);
  keyboard_buffer->head = 0;
  keyboard_buffer->tail = 0;
  register_interrupt_handler(KEYBOARD_INTERRUPT_VECTOR, keyboard_isr);
  keyboard_install_handler();
  log_info(MODULE, "Initialized");
}

void keyboard_install_handler(void) {
  outb(0x21, inb(0x21) & 0xFD);
}

bool keyboard_has_key(void) { return !keyboard_is_buffer_empty(); }

void keyboard_flush_buffer(void) {
  uint32_t flags = spinlock_acquire_irq_save(&keyboard_state_lock);
  keyboard_buffer->head = 0;
  keyboard_buffer->tail = 0;
  spinlock_release_irq_restore(&keyboard_state_lock, flags);
}

bool keyboard_read_scancode(uint8_t *scancode) {
  if (scancode == NULL) return false;
  return keyboard_buffer_read(scancode);
}

char keyboard_scancode_to_char(uint8_t scancode) {
  uint8_t make_code = scancode & 0x7F;
  if (is_break_code(scancode))
    return 0;

  bool shift_active = (modifier_state & MODIFIER_SHIFT) != 0;
  bool caps_active = (lock_state & MODIFIER_CAPS_LOCK) != 0;

  if (make_code >= 0x10 && make_code <= 0x2F) {
    if (caps_active) {
      shift_active = !shift_active;
    }
  }

  char result;
  if (shift_active && make_code < sizeof(scancode_to_ascii_shift)) {
    result = scancode_to_ascii_shift[make_code];
  } else if (make_code < sizeof(scancode_to_ascii_normal)) {
    result = scancode_to_ascii_normal[make_code];
  } else {
    result = 0;
  }
  return result;
}

uint16_t keyboard_read_keycode(void) {
  uint8_t scancode;
  while (!keyboard_buffer_read(&scancode)) {
    task_t *current = task_get_current();
    if (current != NULL) {
      keyboard_waiting_task = current->pid;
      task_block();
    } else {
      __asm__ volatile("hlt");
    }
  }

  if (scancode == 0xE0) {
    uint8_t ext_scancode;
    while (!keyboard_buffer_read(&ext_scancode)) {
      task_sleep(1);
    }
    if (!is_break_code(ext_scancode)) {
      uint8_t make = get_make_code(ext_scancode);
      if (make < sizeof(extended_scancode_map) / sizeof(extended_scancode_map[0])) {
        return extended_scancode_map[make];
      }
    }
    return 0;
  }

  if (is_break_code(scancode)) {
    update_modifier_state(get_make_code(scancode), false);
    return 0;
  }

  update_modifier_state(scancode, true);

  if (scancode == 0x0F) { 
    return KEY_TAB;
  }

  char ascii = keyboard_scancode_to_char(scancode);
  if (ascii != 0) {
    return (uint16_t)ascii;
  }

  return 0;
}

char keyboard_read_char(void) {
  uint16_t code = keyboard_read_keycode();
  if ((code & 0xFF00) == 0) {
    return (char)code;
  }
  return 0;
}

char keyboard_read_char_nonblocking(void) {
  uint8_t scancode;
  if (!keyboard_buffer_read(&scancode)) {
    return 0;
  }
  if (scancode == 0xE0) {
    return 0; 
  }
  return keyboard_scancode_to_char(scancode);
}

uint8_t keyboard_get_modifiers(void) { return modifier_state; }

bool keyboard_is_modifier_active(uint8_t modifier) {
  return (modifier_state & modifier) != 0;
}

uint8_t keyboard_get_lock_state(void) { return lock_state; }

size_t keyboard_read_line(char *buffer, size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) return 0;
  size_t i = 0;
  while (i < buffer_size - 1) {
    char c = keyboard_read_char();
    if (c == '\n') {
      buffer[i++] = '\n';
      buffer[i] = '\0';
      return i;
    } else if (c == '\b') {
      if (i > 0) i--;
    } else if (c != 0) {
      buffer[i++] = c;
    }
  }
  buffer[i] = '\0';
  return i;
}