#include "main.h"
#include "tim.h"
 
#define WS_H           60   // 1 码相对计数值
#define WS_L           28   // 0 码相对计数值
#define WriteAddr (FLASH_BASE+0x6000)
#define EndAddr FLASH_BANK1_END
#define WS2812_MAX_NUM ((EndAddr-WriteAddr-8)>>8)

#define EMPTY_FRAME_ADDR (WriteAddr+4)
#define POS_TEST_ADDR (EMPTY_FRAME_ADDR+(WS2812_MAX_NUM)*48)
#define COLORFUL_ADDR (POS_TEST_ADDR)

uint32_t WS2812_Set(uint32_t addr, uint16_t num, uint32_t color);
void WS2812_Init();
void WS2812_Start(const uint32_t addr, uint32_t len);
void WS2812_Turn_Off();
void WS2812_Test_Position(const uint32_t pos);
void WS2812_RGB();