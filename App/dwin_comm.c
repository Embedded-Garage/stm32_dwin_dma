#include "dwin_comm.h"

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_uart.h"
#include <stdio.h>

extern UART_HandleTypeDef huart2;

#define DWIN_SOF1 0x5Au
#define DWIN_SOF2 0xA5u

#define DWIN_COMMAND_READ 0x83u
#define DWIN_COMMAND_READ_HEADER_LENGTH 3u

#define DWIN_MAX_DATA_LENGTH 255u

#define DWIN_MAX_CALLBACKS 10u

typedef enum {
  PARSE_STATE_SOF1,
  PARSE_STATE_SOF2,
  PARSE_STATE_LEN,
  PARSE_STATE_DATA,
} dwin_comm_parse_state_e;

typedef struct {
  uint16_t address;
  dwin_comm_callback_t callback;
} dwin_comm_callback_entry_s;

typedef struct {
  dwin_comm_parse_state_e parser_state;
  uint8_t length;
  uint8_t data_index;
  uint8_t data[DWIN_MAX_DATA_LENGTH];

  dwin_comm_callback_entry_s callbacks[DWIN_MAX_CALLBACKS];
  uint8_t callback_count;
} dwin_comm_ctx_s;

static dwin_comm_ctx_s ctx = {.parser_state = PARSE_STATE_SOF1};

static void parse_rx_data(uint8_t *data, uint8_t len);
static void parse_read_command(uint8_t *payload, uint8_t payload_length);
static void handle_read_vp(uint16_t address, uint16_t value);

void dwin_comm_parse(uint8_t character) {
  switch (ctx.parser_state) {
  case PARSE_STATE_SOF1:
    if (character == DWIN_SOF1)
      ctx.parser_state = PARSE_STATE_SOF2;
    break;

  case PARSE_STATE_SOF2:
    if (character == DWIN_SOF2)
      ctx.parser_state = PARSE_STATE_LEN;
    else
      ctx.parser_state = PARSE_STATE_SOF1;
    break;

  case PARSE_STATE_LEN:
    ctx.length = character;
    ctx.parser_state = PARSE_STATE_DATA;
    ctx.data_index = 0u;
    break;

  case PARSE_STATE_DATA:
    ctx.data[ctx.data_index++] = character;
    if (ctx.data_index >= ctx.length) {
      parse_rx_data(ctx.data, ctx.length);
      ctx.parser_state = PARSE_STATE_SOF1;
    }
    break;

  default:
    break;
  }
}

bool dwin_comm_register_callback(uint16_t address,
                                 dwin_comm_callback_t callback) {
  if (ctx.callback_count >= DWIN_MAX_CALLBACKS) {
    return false;
  }
  if (callback == NULL) {
    return false;
  }

  dwin_comm_callback_entry_s *entry = &ctx.callbacks[ctx.callback_count];

  entry->address = address;
  entry->callback = callback;

  ctx.callback_count++;

  return true;
}

static void parse_rx_data(uint8_t *data, uint8_t len) {
  if (len < 1u)
    return;

  uint8_t command = data[0u];
  uint8_t *payload = &data[1u];
  uint8_t payload_length = len - 1u;

  switch (command) {
  case DWIN_COMMAND_READ:
    parse_read_command(payload, payload_length);
    break;

  default:
    /* Unknown command */
    break;
  }
}

static void parse_read_command(uint8_t *payload, uint8_t payload_length) {
  if (payload_length < DWIN_COMMAND_READ_HEADER_LENGTH)
    return;

  uint16_t starting_address = (payload[0] << 8) | payload[1];
  uint8_t vp_cnt = payload[2];

  uint8_t vp_bytes = vp_cnt * 2u;
  uint8_t data_length = payload_length - DWIN_COMMAND_READ_HEADER_LENGTH;

  if (vp_bytes != data_length)
    return;

  uint8_t *data_ptr = &payload[DWIN_COMMAND_READ_HEADER_LENGTH];

  for (uint8_t i = 0; i < vp_cnt; i++) {
    uint16_t address = starting_address + i;

    // uint16_t value = data_ptr[i * 2] << 8 | data_ptr[i * 2 + 1];

    uint16_t value = (data_ptr[0u] << 8) | data_ptr[1u];
    data_ptr += 2u;

    handle_read_vp(address, value);
  }
}

static void handle_read_vp(uint16_t address, uint16_t value) {
  for (uint8_t i = 0u; i < ctx.callback_count; i++) {
    dwin_comm_callback_entry_s *entry = &ctx.callbacks[i];

    if (entry->address == address) {
      if (entry->callback != NULL) {
        entry->callback(address, value);
      }
    }
  }
}
