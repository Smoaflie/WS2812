#include "main.h"


void WS2812_INIT();
void WS2812_Set(uint8_t* buf, uint32_t color);
void WS2812_TRANSMIT_TRIGGER(uint32_t SrcAddress, uint32_t DataLength, uint8_t channel);
void WS2812_Turn_Off(uint32_t num, uint8_t channel);
void WS2812_Test_Position(uint32_t num, uint32_t color, uint32_t max_num, uint8_t channel);
void WS2812_Test_FullColor(uint32_t num, uint32_t color, uint8_t channel);
void WS2812_Test_Colorful(uint32_t num, uint32_t color, uint32_t offset, uint8_t channel);

void WS2812_Detect();

void WS2812_DMA_START(uint32_t SrcAddress, uint32_t DataLength);
void WS2812_DMA_TC_CALLBACK(uint8_t channel);

int8_t WS2812_Decoder(uint8_t* buf);