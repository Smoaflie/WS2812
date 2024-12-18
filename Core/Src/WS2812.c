#include "WS2812.h"
#include "string.h"

// 往flash内写入数据
void WS2812_Write(uint32_t addr, uint16_t val){
  while(FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE) != HAL_OK);
  SET_BIT(FLASH->CR, FLASH_CR_PG);
  *(__IO uint16_t*)addr      = val;
}
/**
 * 函数：WS2812灯数据设置函数
 * 参数：addr:待写入的地址, num:灯的数量，color:按RGB的顺序排列，取低24位
 * 作用：往对应地址写入WS2812的数据
***/
uint32_t WS2812_Set(uint32_t addr, uint16_t num, uint32_t color)
{
  uint16_t R,G,B;
  R = (color & 0xFF0000) >> 16;
  G = (color & 0x00FF00) >> 8;
  B = (color & 0x0000FF) >> 0;
  while(num > 0){
    for (uint8_t i = 0;i < 8;i++)
    {
      //填充数组
      WS2812_Write(addr + 2*i,          (G << i) & (0x80)?WS_H:WS_L);
      WS2812_Write(addr + 2*(i + 8),    (R << i) & (0x80)?WS_H:WS_L);
      WS2812_Write(addr + 2*(i + 16),   (B << i) & (0x80)?WS_H:WS_L);
    }
    addr += 2*24;
    num--;
  }
  return addr;
}
//数据初始化
void WS2812_Init()
{
  if((*((uint16_t*)(WriteAddr)) & 0xFFFF) == 0xABCD)  {
    /* 通过标志位判断是否完成初始化 */
    return; 
  }
  uint32_t flash_erase_error;
  FLASH_EraseInitTypeDef flash_erase_init = {
    .TypeErase = FLASH_TYPEERASE_PAGES,
    .Banks = FLASH_BANK_1,
    .PageAddress = WriteAddr,
    .NbPages = (EndAddr - WriteAddr)/0x400 + 1
  };
  HAL_FLASHEx_Erase(&flash_erase_init, &flash_erase_error);
  while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
      /* 等待 BSY 标志位清除 */
  }
  // 空帧 & 侧位单帧
  WS2812_Set(EMPTY_FRAME_ADDR, WS2812_MAX_NUM,0x00);
  WS2812_Set(POS_TEST_ADDR, 1,0xFF0000);
  // 炫彩RGB
  uint32_t offset = (0xFFFFFF/WS2812_MAX_NUM);
  for(uint8_t j = 0; j < 2; j++){
    uint32_t color = 0;
    for(uint16_t i = 0; i < WS2812_MAX_NUM; i++){
      color+=offset;
      WS2812_Set(COLORFUL_ADDR+(j*WS2812_MAX_NUM+i)*48, 1, color);
    }
  }

  WS2812_Write(WriteAddr,0xABCD);
}
//开启传输
void WS2812_Start(const uint32_t addr, uint32_t len)
{
  HAL_TIM_PWM_Stop_DMA(&htim2, TIM_CHANNEL_1);  // 停止定时器 PWM 和 DMA
  //作用：调用DMA将显存中的内容实时搬运至定时器的比较寄存器
  HAL_TIM_PWM_Start_DMA(&htim2,TIM_CHANNEL_1,(const uint32_t *)addr,len*24); 
}
//关闭所有灯
void WS2812_Turn_Off()
{
  WS2812_Start(EMPTY_FRAME_ADDR,WS2812_MAX_NUM+1);
}
//测试灯珠位置-索引从1开始
void WS2812_Test_Position(const uint32_t pos)
{
  static uint32_t current_pos = 0;
  if(current_pos != pos){
    WS2812_Turn_Off();
    HAL_Delay(500);
    current_pos = pos;
  }
  WS2812_Start(POS_TEST_ADDR-(pos-1)*48, pos);
}
//RGB！
void WS2812_RGB()
{
  static int32_t count = 0;
  static uint8_t flag = 1;
  count += flag ? 1 : -1;
  if(count > WS2812_MAX_NUM || count < 0) flag = !flag;

  WS2812_Start(COLORFUL_ADDR+count, WS2812_MAX_NUM);
}