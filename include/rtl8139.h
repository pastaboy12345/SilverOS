#ifndef SILVEROS_RTL8139_H
#define SILVEROS_RTL8139_H

#include "types.h"

void rtl8139_init(void);
void rtl8139_send_packet(void *data, uint32_t len);

extern uint8_t rtl8139_mac[6];

#endif
