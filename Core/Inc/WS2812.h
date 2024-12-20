#include "main.h"
#include "tim.h"
 
#define WS_H           60   // 1 码相对计数值
#define WS_L           28   // 0 码相对计数值

typedef struct{
    uint32_t start;
    uint32_t end;
    uint32_t color;
}WS2812_CONTROL_BLOCK;

void WS2812_INIT();
void WS2812_Set(uint8_t* buf, uint32_t color);
void WS2812_DMA_START(uint32_t SrcAddress, uint32_t DataLength);
void WS2812_Turn_Off(uint32_t num);
void WS2812_Test_Position(uint32_t num, uint32_t color);
void WS2812_Test_FullColor(uint32_t num, uint32_t color);
void WS2812_Test_Colorful(uint32_t num, uint32_t color, uint32_t offset);

void WS2812_Detect();

void WS2812_DMA_START(uint32_t SrcAddress, uint32_t DataLength);
void WS2812_DMA_TC_CALLBACK();
