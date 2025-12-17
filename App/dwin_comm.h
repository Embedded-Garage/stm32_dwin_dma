#ifndef DWIN_COMM_H
#define DWIN_COMM_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*dwin_comm_callback_t)(uint16_t address, uint16_t value);

extern void dwin_comm_parse(uint8_t character);

extern bool dwin_comm_register_callback(uint16_t address, dwin_comm_callback_t callback);

extern bool dwin_comm_set_vp_value(uint16_t address, uint16_t value);

#endif /* DWIN_COMM_H */
