#include "dwin_comm.h"

#include "dwin_common.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_uart.h"
#include <stdio.h>

#define DWIN_SOF1 0x5Au
#define DWIN_SOF2 0xA5u

#define DWIN_COMMAND_READ_HEADER_LENGTH 3u

#define DWIN_MAX_DATA_LENGTH 255u

#define DWIN_MAX_CALLBACKS 10u

typedef enum {
  PARSE_STATE_NOT_INITIALIZED = 0,
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
  UART_HandleTypeDef *huart;
} dwin_comm_ctx_s;

static dwin_comm_ctx_s ctx = {.parser_state = PARSE_STATE_NOT_INITIALIZED};

static void parse_rx_data(uint8_t *data, uint8_t len);
static void parse_read_command(uint8_t *payload, uint8_t payload_length);
static void handle_read_vp(uint16_t address, uint16_t value);

bool dwin_comm_init(UART_HandleTypeDef *huart) {
  if (huart == NULL) {
    return false;
  }

  ctx.huart = huart;
  ctx.parser_state = PARSE_STATE_SOF1;
  ctx.callback_count = 0u;

  return true;
}

void dwin_comm_parse(uint8_t character) {
  switch (ctx.parser_state) {
  case PARSE_STATE_NOT_INITIALIZED:
    // Not initialized, ignore all data
    break;

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

bool dwin_comm_set_vp_value(uint16_t address, uint16_t value) {
  dwin_frame_write_s frame = {.sof1 = DWIN_SOF1,
                              .sof2 = DWIN_SOF2,
                              .length = sizeof(dwin_frame_write_s) -
                                        3u, // Exclude SOF1, SOF2, Length
                              .command = DWIN_COMMAND_WRITE,
                              .address = __builtin_bswap16(address),
                              .value = __builtin_bswap16(value)};

  HAL_StatusTypeDef status = HAL_UART_Transmit(
      ctx.huart, (uint8_t *)&frame, sizeof(dwin_frame_write_s), HAL_MAX_DELAY);

  return (status == HAL_OK) ? true : false;
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
