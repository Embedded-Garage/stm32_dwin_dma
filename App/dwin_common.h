#pragma once

#include <stdint.h>

#define DWIN_COMMAND_READ 0x83u
#define DWIN_COMMAND_WRITE 0x82u

typedef struct{
    uint8_t sof1;
    uint8_t sof2;
    uint8_t length;
    uint8_t command;
    uint16_t address;
    uint16_t value;
} __attribute__((packed)) dwin_frame_write_s;
