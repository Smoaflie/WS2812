#include "WS2812.h"
#include "string.h"
#include <stdlib.h>

/* 自定义的日志系统 */
#define WS2812_LOG_INIT()   ;
#define WS2812_LOG(fmt, ...)    ;
 

// 获取不同通道的参数
/* 需自行放置于对应位置的函数 */
/* 这里提供的是基于Stm32的HAL库的代码 */
#include "tim.h"
#define WS2812_CHANNEL_PARAMS_GET(channel) ;        \
    extern DMA_HandleTypeDef hdma_tim2_ch1;         \
    extern DMA_HandleTypeDef hdma_tim2_ch3;         \
    extern DMA_HandleTypeDef hdma_tim2_ch2_ch4;     \
    uint32_t htim_ch;                               \
    TIM_HandleTypeDef *htim;                        \
    DMA_HandleTypeDef *hdma_tim_ch;                 \
    switch(channel)                                 \
    {                                               \
        case 0:                                     \
            htim        = &htim2;                   \
            hdma_tim_ch = &hdma_tim2_ch1;           \
            htim_ch     = TIM_CHANNEL_1;            \
            break;                                  \
        case 1:                                     \
            htim        = &htim2;                   \
            hdma_tim_ch = &hdma_tim2_ch3;           \
            htim_ch     = TIM_CHANNEL_3;            \
            break;                                  \
        case 2:                                     \
            htim        = &htim2;                   \
            hdma_tim_ch = &hdma_tim2_ch2_ch4;       \
            htim_ch     = TIM_CHANNEL_2;            \
            break;                                  \
        default:                                    \
            break;                                  \
    }                                               \
    ;                                               \


/*  一个WS2812控制数据为0xRRGGBB(调整后)共24位
 *  通过PWM或SPI模拟发送，需要分别用24个字节表示1/0状态
 *  *本模块编写时使用的是stm32f103c8板子，使用PWM DMA输出会出现数据丢失(问题一)
 *   (表现为发送24位数据，逻辑分析仪上仅能检测到23个脉冲)
 *   此外还有数据异常增多的问题(问题二)(未查明原因)
 *   摸索出来的解决办法是在发送数据段首尾加0，因此定义了buffer_addr_offset,但具体怎么解决的未知
 *   建议换spi输出或手动装填(不使用HAL库直接操作寄存器)
*/
#define buffer_len  28
#define buffer_addr_offset (buffer_len%24/2)
#define transmit_channel_num 1
#define WS_H           60   // 1 码相对计数值
#define WS_L           28   // 0 码相对计数值


/* 不同控制模式枚举名 */
typedef enum {
    FREE = 0,
    BLOCK,
    OFFSET,
} WS2812_DETECT_MODE_;
/* 不同控制命令触发方式枚举名 */
typedef enum{
    COMMAND_TRIGGER = 0,
    COMMAND_DIRECT
}WS2812_COMMAND_TYPE_;
/* 包控制模式单包参数 */
typedef struct{
    uint32_t start;
    uint32_t color;
}WS2812_CONTROL_BLOCK;
/* 包控制命令接收相关结构体 */
typedef struct {
    WS2812_COMMAND_TYPE_ command_type;
    uint32_t led_num;
    uint32_t block_num;
    WS2812_CONTROL_BLOCK *block;
    
    uint32_t block_filled_index;
}WS2812_COMMAND_BLOCK_RECEIVER_;
/* 各通道控制命令接收相关结构体 */
typedef struct {
    WS2812_COMMAND_BLOCK_RECEIVER_ command_block;
}WS2812_COMMAND_RECEIVER_;

/* 控制相关结构体 */
static struct {
    uint8_t buf_1[buffer_len];
    uint8_t buf_2[buffer_len];
    uint32_t buf_addr_to_be_transmit;

    uint32_t index;
    uint32_t total;
    WS2812_CONTROL_BLOCK *block;
    uint32_t block_len;
    uint32_t block_index;
    uint32_t block_color;

    uint32_t offset;
    uint32_t color;

    WS2812_DETECT_MODE_ mode;

    uint8_t transmit_complete_flag;
    WS2812_COMMAND_RECEIVER_ receiver;
} WS2812_DETECT_S_[transmit_channel_num] = {0};





/**
 * 函数：WS2812灯数据设置函数
 * 参数：addr:待写入的地址, num:灯的数量，color:按RGB的顺序排列，取低24位
 * 作用：往对应地址写入WS2812的数据
 ***/
void WS2812_Set(uint8_t *buf, uint32_t color)
{
    uint16_t R, G, B;
    R = (color & 0xFF0000) >> 16;
    G = (color & 0x00FF00) >> 8;
    B = (color & 0x0000FF) >> 0;
    for (uint8_t i = 0; i < 8; i++) {
        // 填充数组
        buf[i]      = (G << i) & (0x80) ? WS_H : WS_L;
        buf[i + 8]  = (R << i) & (0x80) ? WS_H : WS_L;
        buf[i + 16] = (B << i) & (0x80) ? WS_H : WS_L;
    }
}

// 初始化
void WS2812_INIT()
{
    memset(WS2812_DETECT_S_, 0, sizeof(WS2812_DETECT_S_));

    /* 需自行放置于对应位置的函数 */
    /* 这里提供的是基于Stm32的HAL库的代码，基于DMA PWM发送 */
    for(int i = 0; i < transmit_channel_num; i++)
    {
        WS2812_CHANNEL_PARAMS_GET(i);
        // 初始化-利用HAL库快速配置寄存器
        HAL_TIM_PWM_Start_DMA(htim, htim_ch, (const uint32_t *)WS2812_DETECT_S_[i].buf_1, buffer_len + 1);
    }

    WS2812_LOG_INIT();
}

// 触发发送一组WS2812灯珠控制时序
void WS2812_TRANSMIT_TRIGGER(uint32_t SrcAddress, uint32_t DataLength, uint8_t channel)
{
    /* 需自行放置于对应位置的函数 */
    /* 这里提供的是基于Stm32f103c8t6的HAL库的代码 */
    WS2812_CHANNEL_PARAMS_GET(channel);
    
    // 失能DMA
    __HAL_DMA_DISABLE(hdma_tim_ch);
    // 配置寄存器
    /* Configure DMA Channel data length */
    hdma_tim_ch->Instance->CNDTR = DataLength + 1;
    /* Configure DMA Channel source address */
    hdma_tim_ch->Instance->CMAR = SrcAddress;
    // 开启传输完成中断 & 使能DMA
    __HAL_DMA_ENABLE_IT(hdma_tim_ch, DMA_IT_TC);
    __HAL_DMA_ENABLE(hdma_tim_ch);
}

// 传输完成中断回调，需放置于对应的中断回调内
void WS2812_DMA_TC_CALLBACK(uint8_t channel)
{

    WS2812_DETECT_S_[channel].transmit_complete_flag = 1;

    /* 需自行放置于对应位置的函数 */
    /* 这里提供的是基于Stm32f103c8t6的HAL库的代码 */
    WS2812_CHANNEL_PARAMS_GET(channel);
    // 清理PWM比较器值
    htim->Instance->CCR1 = 0;
}
// 循环执行，用于自动处理发送请求
void WS2812_Detect()
{
    for(int i = 0; i < transmit_channel_num; i++)
    {
        if (WS2812_DETECT_S_[i].mode == FREE) return;

        if (WS2812_DETECT_S_[i].transmit_complete_flag) {
            WS2812_DETECT_S_[i].transmit_complete_flag = 0;

            uint32_t buf = WS2812_DETECT_S_[i].buf_addr_to_be_transmit;
            WS2812_TRANSMIT_TRIGGER(buf, buffer_len, i);

            buf            = (const uint32_t)(WS2812_DETECT_S_[i].buf_addr_to_be_transmit == (const uint32_t)WS2812_DETECT_S_[i].buf_1 ? WS2812_DETECT_S_[i].buf_2 : WS2812_DETECT_S_[i].buf_1);
            uint32_t index = ++WS2812_DETECT_S_[i].index;
            if (index > WS2812_DETECT_S_[i].total) {
                memset(&WS2812_DETECT_S_, 0, sizeof(WS2812_DETECT_S_));
                return;
            }

            switch (WS2812_DETECT_S_[i].mode) {
                default:
                case FREE:
                    return;

                case BLOCK:
                    WS2812_CONTROL_BLOCK *block = WS2812_DETECT_S_[i].block;
                    uint32_t block_index        = WS2812_DETECT_S_[i].block_index;
                    uint32_t block_color        = WS2812_DETECT_S_[i].block_color;

                    uint8_t block_in_range_flag = block_index < WS2812_DETECT_S_[i].block_len;

                    while (block_in_range_flag && index >= block[block_index].start) {
                        block_color = block[block_index].color;
                        block_in_range_flag = ++block_index < WS2812_DETECT_S_[i].block_len;
                    }

                    WS2812_DETECT_S_[i].block_index = block_index;
                    WS2812_DETECT_S_[i].block_color = block_color;

                    WS2812_Set((uint8_t *)buf+buffer_addr_offset, block_color);
                    
                    return;

                case OFFSET:
                    static uint8_t reverse = 0;
                    WS2812_DETECT_S_[i].color += reverse ? -WS2812_DETECT_S_[i].offset : WS2812_DETECT_S_[i].offset;
                    if (WS2812_DETECT_S_[i].color > 0xFF0000 || WS2812_DETECT_S_[i].color < 0) reverse = !reverse;

                    WS2812_Set((uint8_t *)buf+buffer_addr_offset, WS2812_DETECT_S_[i].color);
                    return;
            }
        }
    }
}

/* 内部函数*/
// 以分区模式更新LED状态
static void WS2812_START_by_BLOCK(uint32_t WS2812_num, WS2812_CONTROL_BLOCK *block, uint32_t block_len, uint8_t channel)
{
    WS2812_DETECT_S_[channel].total     = WS2812_num;
    WS2812_DETECT_S_[channel].mode      = BLOCK;
    WS2812_DETECT_S_[channel].block     = block;
    WS2812_DETECT_S_[channel].block_len = block_len;

    memset(WS2812_DETECT_S_[channel].buf_1, 0, sizeof(WS2812_DETECT_S_[channel].buf_1));
    memset(WS2812_DETECT_S_[channel].buf_2, 0, sizeof(WS2812_DETECT_S_[channel].buf_2));
}

/* 测试用函数 */
// 关闭所有灯
void WS2812_Turn_Off(uint32_t num, uint8_t channel)
{
    WS2812_START_by_BLOCK(num, NULL, 0, channel);
}
// 测试灯珠-位置-索引从1开始
void WS2812_Test_Position(uint32_t num, uint32_t color, uint32_t max_num, uint8_t channel)
{
    static WS2812_CONTROL_BLOCK block[2];
    block[0].start = num;
    block[0].color = color;
    block[1].start = num+1;
    block[1].color = 0x0;
    WS2812_START_by_BLOCK(max_num, block, 2, channel);
}
// 测试灯珠-单色-索引从1开始
void WS2812_Test_FullColor(uint32_t num, uint32_t color, uint8_t channel)
{
    static WS2812_CONTROL_BLOCK block;
    block.start = 1;
    block.color = color;
    WS2812_START_by_BLOCK(num, &block, 1, channel);
}
// 彩灯
void WS2812_Test_Colorful(uint32_t num, uint32_t color, uint32_t offset, uint8_t channel)
{
    WS2812_DETECT_S_[channel].total  = num;
    WS2812_DETECT_S_[channel].mode   = OFFSET;
    WS2812_DETECT_S_[channel].color  = color;
    WS2812_DETECT_S_[channel].offset = offset;

    memset(WS2812_DETECT_S_[channel].buf_1, 0, sizeof(WS2812_DETECT_S_[channel].buf_1));
    memset(WS2812_DETECT_S_[channel].buf_2, 0, sizeof(WS2812_DETECT_S_[channel].buf_2));
}


// CRC-16 标准多项式
#define CRC_POLYNOMIAL 0x8005
// CRC-16 计算函数
uint16_t calculate_crc16(const uint8_t *data, size_t length) 
{
    uint16_t crc = 0xFFFF; // 初始化值
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i]; // 将当前字节异或到 CRC 的低字节
        for (int j = 0; j < 8; j++) { // 逐位处理
            if (crc & 0x0001) {       // 检查最低位
                crc = (crc >> 1) ^ CRC_POLYNOMIAL;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc; // 返回 CRC 校验值
}

/* 声明各命令帧解析函数 */
int8_t WS2812_Command_Trigger(uint8_t channel);
int8_t WS2812_BLOCK_Command_Generator(uint8_t* data, uint8_t data_len);
int8_t WS2812_BLOCK_Command_Fill(uint8_t* data, uint8_t data_len);
/* 放置于接收中断内，用于解析控制帧 */
int8_t WS2812_Decoder(uint8_t* buf)
{
    uint8_t head = *buf;
    uint8_t command = *(buf+1);
    uint8_t data_len = *(buf+2);
    uint8_t* data = buf+3;
    uint16_t crc = *(buf+3+data_len) | *(buf+3+data_len+1) << 8;

    // 校验包头
    if(head != 0xAA)    return -1;
    
    // 验证CRC
    if(crc != 0xABCD && crc != calculate_crc16(buf+1, 2+data_len))   return -2;
    // 处理数据
    for(int i = 0; i < data_len; i++)
    {   
        switch(command)
        {
            case 0x00:  return WS2812_Command_Trigger(*data);   // 该命令数据段只有一个字节 channel
            case 0xA0:  return WS2812_BLOCK_Command_Generator(data, data_len);
            case 0xA1:  return WS2812_BLOCK_Command_Fill(data, data_len);
            default:    break;
        }
    }
    return 1;
}

/* 定义各命令帧解析函数 */
// 0x00 亦可被其他控制命令调用
int8_t WS2812_Command_Trigger(uint8_t channel)
{
    WS2812_COMMAND_BLOCK_RECEIVER_ *block_receiver = &WS2812_DETECT_S_[channel].receiver.command_block;

    if(block_receiver->block_filled_index == block_receiver->block_num &&
        block_receiver->command_type == COMMAND_TRIGGER)
        {
            WS2812_START_by_BLOCK(
                block_receiver->led_num,
                block_receiver->block,
                block_receiver->block_num,
                channel
            );

            WS2812_LOG("Command_block apply!.");
            return 1;
        }
    return 0;
}
// 0xA0
int8_t WS2812_BLOCK_Command_Generator(uint8_t* data, uint8_t data_len)
{
    uint8_t channel = *(data + 9);
    WS2812_COMMAND_BLOCK_RECEIVER_ *block_receiver = &WS2812_DETECT_S_[channel].receiver.command_block;

    block_receiver->command_type = *data;
    block_receiver->led_num = *(uint32_t*)(data+1);
    
    
    uint32_t block_num = *(uint32_t*)(data+5);
    block_receiver->block_num = block_num;

    if (block_receiver->block != NULL) {
        free(block_receiver->block);
        block_receiver->block = NULL;
    }
    block_receiver->block = (WS2812_CONTROL_BLOCK*)malloc(sizeof(WS2812_CONTROL_BLOCK) * block_num);
    if (block_receiver->block == NULL) {
        return 0; // 块过大，内存申请失败
    }
    memset(block_receiver->block, 0, sizeof(WS2812_CONTROL_BLOCK) * block_num);
    
    block_receiver->block_filled_index = 0;

    WS2812_LOG("Command_block enerated:\ncommand_type: %d\nblock_num: %ld\nled_num: %ld\n", 
            (uint8_t)block_receiver->command_type, block_receiver->block_num, block_receiver->led_num);
}
// 0xA1
int8_t WS2812_BLOCK_Command_Fill(uint8_t* data, uint8_t data_len)
{
    uint8_t channel = *(data + 8);
    WS2812_COMMAND_BLOCK_RECEIVER_ *block_receiver = &WS2812_DETECT_S_[channel].receiver.command_block;

    uint32_t index = block_receiver->block_filled_index;
    if(index >= block_receiver->block_num)  return 0;

    block_receiver->block[index].start = *(uint32_t*)data;
    block_receiver->block[index].color = *(data+4) | *(data+5) << 8 | *(data+6) << 16;

    WS2812_LOG("Command_block received:\nstart_num: %d\ncolor: 0x%06X", 
            (uint8_t)block_receiver->block[index].start, block_receiver->block[index].color);

    block_receiver->block_filled_index++;
    if(block_receiver->block_filled_index == block_receiver->block_num &&
        block_receiver->command_type == COMMAND_DIRECT)
        {
            return WS2812_Command_Trigger(channel);
        }
    return 0;
}
