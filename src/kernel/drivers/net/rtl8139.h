#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include <stdbool.h>

void rtl8139_initialize(void);
void rtl8139_send(uint8_t *data, uint32_t len);
bool rtl8139_isr(void);

#endif