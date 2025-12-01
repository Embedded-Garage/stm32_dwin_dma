#include "dwin_comm.h"

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_uart.h"

extern UART_HandleTypeDef huart2;

#define DWIN_SOF1 0x5Au
#define DWIN_SOF2 0xA5u

#define DWIN_MAX_DATA_LENGTH 255u

typedef enum {
  PARSE_STATE_SOF1,
  PARSE_STATE_SOF2,
  PARSE_STATE_LEN,
  PARSE_STATE_DATA,
} dwin_comm_parse_state_e;

typedef struct {
  dwin_comm_parse_state_e parser_state;
  uint8_t length;
  uint8_t data_index;
  uint8_t data[DWIN_MAX_DATA_LENGTH];
} dwin_comm_ctx_s;

static dwin_comm_ctx_s ctx = {.parser_state = PARSE_STATE_SOF1};

static void parse_rx_data(uint8_t *data, uint8_t len);

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
    if (ctx.data_index >= ctx.length)
    {
        parse_rx_data(ctx.data, ctx.length);
        ctx.parser_state = PARSE_STATE_SOF1;
    }
    break;

  default:
    break;
  }
}

static void parse_rx_data(uint8_t *data, uint8_t len)
{
    HAL_UART_Transmit(&huart2, (const uint8_t *)"dwin data received", 18, HAL_MAX_DELAY);
}
