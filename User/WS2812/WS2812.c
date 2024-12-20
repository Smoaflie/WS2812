#include "WS2812.h"
#include "string.h"

#define htim htim2
#define hdma_tim_ch hdma_tim2_ch1
#define htim_ch TIM_CHANNEL_1

#define buffer_len  25

extern DMA_HandleTypeDef hdma_tim2_ch1;


static uint8_t  buf_1[buffer_len] = {0};
static uint8_t  buf_2[buffer_len] = {0};

uint8_t WS2812_TC_flag = 0;

typedef enum{
  FREE = 0,
  BLOCK,
  OFFSET,
}WS2812_DETECT_MODE_;
struct{
  uint32_t index;
  uint32_t total;
  WS2812_CONTROL_BLOCK* block;
  uint32_t block_len;
  uint32_t block_index;

  uint32_t offset;
  uint32_t color;
  
  WS2812_DETECT_MODE_ mode;
}WS2812_DETECT_S_;
/**
 * 函数：WS2812灯数据设置函数
 * 参数：addr:待写入的地址, num:灯的数量，color:按RGB的顺序排列，取低24位
 * 作用：往对应地址写入WS2812的数据
***/
void WS2812_Set(uint8_t* buf, uint32_t color)
{
  uint16_t R,G,B;
  R = (color & 0xFF0000) >> 16;
  G = (color & 0x00FF00) >> 8;
  B = (color & 0x0000FF) >> 0;
  for (uint8_t i = 0;i < 8;i++)
  {
    //填充数组
    buf[i]  =          (G << i) & (0x80)?WS_H:WS_L;
    buf[i + 8]=    (R << i) & (0x80)?WS_H:WS_L;
    buf[i + 16]=   (B << i) & (0x80)?WS_H:WS_L;
  }
}

//初始化-利用HAL库快速配置寄存器
//移植的时候记得加延时
void WS2812_INIT()
{
  memset(&WS2812_DETECT_S_, 0, sizeof(WS2812_DETECT_S_));

  memset(buf_1, 0, sizeof(buf_1));
  memset(buf_2, 0, sizeof(buf_2));
  HAL_TIM_PWM_Start_DMA(&htim, htim_ch, (const uint32_t *)buf_1,  buffer_len+1);
}



//配置DMA
void WS2812_DMA_START(uint32_t SrcAddress, uint32_t DataLength)
{
  //失能DMA
  __HAL_DMA_DISABLE(&hdma_tim_ch);
  //配置寄存器
  /* Configure DMA Channel data length */
  (&hdma_tim2_ch1)->Instance->CNDTR = DataLength+1;
  /* Configure DMA Channel source address */
  (&hdma_tim2_ch1)->Instance->CMAR = SrcAddress;
  //开启传输完成中断 & 使能DMA
  __HAL_DMA_ENABLE_IT(&hdma_tim_ch, DMA_IT_TC);
  __HAL_DMA_ENABLE(&hdma_tim_ch);
}

// DMA传输完成中断回调
void WS2812_DMA_TC_CALLBACK()
{
  WS2812_TC_flag = 1;
  //清理PWM比较器值
  (&htim)->Instance->CCR1 = 0;

  //清除传输完成标志位
  // __HAL_DMA_CLEAR_FLAG(&hdma_tim_ch, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_tim_ch));
}

//开始传输
void WS2812_START_by_BLOCK(uint32_t WS2812_num, WS2812_CONTROL_BLOCK* block, uint32_t block_len)
{
  WS2812_DETECT_S_.total = WS2812_num;
  WS2812_DETECT_S_.mode = BLOCK;
  WS2812_DETECT_S_.block = block;
  WS2812_DETECT_S_.block_len = block_len;

  memset(buf_1, 0, sizeof(buf_1));
  memset(buf_2, 0, sizeof(buf_2));
}

//关闭所有灯
void WS2812_Turn_Off(uint32_t num)
{
  WS2812_START_by_BLOCK(num, NULL, 0);
}
//测试灯珠-位置-索引从1开始
void WS2812_Test_Position(uint32_t num, uint32_t color)
{
  static WS2812_CONTROL_BLOCK block;
    block.start = num;
    block.end = num;
    block.color = color;
  WS2812_START_by_BLOCK(num, &block, 1);
}
//测试灯珠-单色-索引从1开始
void WS2812_Test_FullColor(uint32_t num, uint32_t color)
{
  static WS2812_CONTROL_BLOCK block;
    block.start = 1;
    block.end = num;
    block.color = color;
  WS2812_START_by_BLOCK(num, &block, 1);
}
//彩灯
void WS2812_Test_Colorful(uint32_t num, uint32_t color, uint32_t offset)
{
  WS2812_DETECT_S_.total = num;
  WS2812_DETECT_S_.mode = OFFSET;
  WS2812_DETECT_S_.color = color;
  WS2812_DETECT_S_.offset = offset;

  memset(buf_1, 0, sizeof(buf_1));
  memset(buf_2, 0, sizeof(buf_2));
}

//循环执行，自动完成WS2812控制工作
void WS2812_Detect()
{
  if(WS2812_DETECT_S_.mode == FREE) return;

  if(WS2812_TC_flag)
  {
    WS2812_TC_flag = 0;

    uint32_t buf = (const uint32_t)(hdma_tim_ch.Instance->CMAR == (const uint32_t)buf_1 ? buf_2 : buf_1);
    WS2812_DMA_START(buf, buffer_len);

    buf = (const uint32_t)(hdma_tim_ch.Instance->CMAR == (const uint32_t)buf_1 ? buf_2 : buf_1);
    uint32_t index = ++WS2812_DETECT_S_.index;
    if(index > WS2812_DETECT_S_.total)
    {
      memset(&WS2812_DETECT_S_, 0, sizeof(WS2812_DETECT_S_));
      return;
    }
    
    switch(WS2812_DETECT_S_.mode)
    {
      default:
      case FREE:
        return;

      case BLOCK:
        WS2812_CONTROL_BLOCK* block = WS2812_DETECT_S_.block;
        uint32_t block_index = WS2812_DETECT_S_.block_index;

        uint8_t struct_in_range_flag = block_index < WS2812_DETECT_S_.block_len;

        if(struct_in_range_flag &&  index >= block[block_index].start && index <= block[block_index].end)
          WS2812_Set((uint8_t * )buf, block[block_index].color);
        else
          WS2812_Set((uint8_t * )buf, 0x00);

        while (struct_in_range_flag && index>block[block_index].end)  {
          WS2812_DETECT_S_.block_index++;
        }
        return;

      case OFFSET:
        static uint8_t reverse = 0;
        WS2812_DETECT_S_.color += reverse ? -WS2812_DETECT_S_.offset : WS2812_DETECT_S_.offset;
        if(WS2812_DETECT_S_.color > 0xFF0000 || WS2812_DETECT_S_.color < 0) reverse=!reverse;
        
        WS2812_Set((uint8_t * )buf, WS2812_DETECT_S_.color);
        return;
    }
  }
}