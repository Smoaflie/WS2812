#include "WS2812.h"
#include "string.h"

/* 自定义的日志系统 */
#include "SEGGER_RTT.h"
#define WS2812_LOG_INIT() SEGGER_RTT_Init()
#define WS2812_LOG(fmt, ...) SEGGER_RTT_printf(0, fmt "\r\n%s", \
                          ##__VA_ARGS__,                          \
                          RTT_CTRL_RESET)


#define htim        htim2
#define hdma_tim_ch hdma_tim2_ch1
#define htim_ch     TIM_CHANNEL_1

#define buffer_len  25

extern DMA_HandleTypeDef hdma_tim2_ch1;

static uint8_t buf_1[buffer_len] = {0};
static uint8_t buf_2[buffer_len] = {0};

typedef enum {
    FREE = 0,
    BLOCK,
    OFFSET,
} WS2812_DETECT_MODE_;

static struct {
    uint32_t index;
    uint32_t total;
    WS2812_CONTROL_BLOCK *block;
    uint32_t block_len;
    uint32_t block_index;

    uint32_t offset;
    uint32_t color;

    WS2812_DETECT_MODE_ mode;
    
    uint8_t transmit_complete_flag;
} WS2812_DETECT_S_;

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

// 初始化-利用HAL库快速配置寄存器
// 移植的时候记得加延时
void WS2812_INIT()
{
    memset(&WS2812_DETECT_S_, 0, sizeof(WS2812_DETECT_S_));

    memset(buf_1, 0, sizeof(buf_1));
    memset(buf_2, 0, sizeof(buf_2));
    HAL_TIM_PWM_Start_DMA(&htim, htim_ch, (const uint32_t *)buf_1, buffer_len + 1);

    WS2812_LOG_INIT();
}

/* 需自行放置于对应位置的函数 */
// 传输相关DMA配置
void WS2812_DMA_START(uint32_t SrcAddress, uint32_t DataLength)
{
    // 失能DMA
    __HAL_DMA_DISABLE(&hdma_tim_ch);
    // 配置寄存器
    /* Configure DMA Channel data length */
    (&hdma_tim2_ch1)->Instance->CNDTR = DataLength + 1;
    /* Configure DMA Channel source address */
    (&hdma_tim2_ch1)->Instance->CMAR = SrcAddress;
    // 开启传输完成中断 & 使能DMA
    __HAL_DMA_ENABLE_IT(&hdma_tim_ch, DMA_IT_TC);
    __HAL_DMA_ENABLE(&hdma_tim_ch);
}
// 传输完成中断回调
void WS2812_DMA_TC_CALLBACK()
{
    WS2812_DETECT_S_.transmit_complete_flag = 1;
    // 清理PWM比较器值
    (&htim)->Instance->CCR1 = 0;

    // 清除传输完成标志位
    //  __HAL_DMA_CLEAR_FLAG(&hdma_tim_ch, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_tim_ch));
}
// 循环执行，用于自动处理发送请求
void WS2812_Detect()
{
    if (WS2812_DETECT_S_.mode == FREE) return;

    if (WS2812_DETECT_S_.transmit_complete_flag) {
        WS2812_DETECT_S_.transmit_complete_flag = 0;

        uint32_t buf = (const uint32_t)(hdma_tim_ch.Instance->CMAR == (const uint32_t)buf_1 ? buf_2 : buf_1);
        WS2812_DMA_START(buf, buffer_len);

        buf            = (const uint32_t)(hdma_tim_ch.Instance->CMAR == (const uint32_t)buf_1 ? buf_2 : buf_1);
        uint32_t index = ++WS2812_DETECT_S_.index;
        if (index > WS2812_DETECT_S_.total) {
            memset(&WS2812_DETECT_S_, 0, sizeof(WS2812_DETECT_S_));
            return;
        }

        switch (WS2812_DETECT_S_.mode) {
            default:
            case FREE:
                return;

            case BLOCK:
                WS2812_CONTROL_BLOCK *block = WS2812_DETECT_S_.block;
                uint32_t block_index        = WS2812_DETECT_S_.block_index;
                uint32_t block_color        = 0x000000;

                uint8_t block_in_range_flag = block_index < WS2812_DETECT_S_.block_len;

                while (block_in_range_flag && index >= block[block_index].start) {
                    block_color = block[block_index].color;
                    WS2812_DETECT_S_.block_index++;
                    block_in_range_flag = WS2812_DETECT_S_.block_index < WS2812_DETECT_S_.block_len;
                }

                WS2812_Set((uint8_t *)buf, block_color);
                
                return;

            case OFFSET:
                static uint8_t reverse = 0;
                WS2812_DETECT_S_.color += reverse ? -WS2812_DETECT_S_.offset : WS2812_DETECT_S_.offset;
                if (WS2812_DETECT_S_.color > 0xFF0000 || WS2812_DETECT_S_.color < 0) reverse = !reverse;

                WS2812_Set((uint8_t *)buf, WS2812_DETECT_S_.color);
                return;
        }
    }
}

/* 内部函数*/
// 以分区模式更新LED状态
static void WS2812_START_by_BLOCK(uint32_t WS2812_num, WS2812_CONTROL_BLOCK *block, uint32_t block_len)
{
    WS2812_DETECT_S_.total     = WS2812_num;
    WS2812_DETECT_S_.mode      = BLOCK;
    WS2812_DETECT_S_.block     = block;
    WS2812_DETECT_S_.block_len = block_len;

    memset(buf_1, 0, sizeof(buf_1));
    memset(buf_2, 0, sizeof(buf_2));
}

/* 测试用函数 */
// 关闭所有灯
void WS2812_Turn_Off(uint32_t num)
{
    WS2812_START_by_BLOCK(num, NULL, 0);
}
// 测试灯珠-位置-索引从1开始
void WS2812_Test_Position(uint32_t num, uint32_t color, uint32_t max_num)
{
    static WS2812_CONTROL_BLOCK block[2];
    block[0].start = num;
    block[0].color = color;
    block[1].start = num+1;
    block[1].color = 0x0;
    WS2812_START_by_BLOCK(max_num, block, 2);
}
// 测试灯珠-单色-索引从1开始
void WS2812_Test_FullColor(uint32_t num, uint32_t color)
{
    static WS2812_CONTROL_BLOCK block;
    block.start = 1;
    block.color = color;
    WS2812_START_by_BLOCK(num, &block, 1);
}
// 彩灯
void WS2812_Test_Colorful(uint32_t num, uint32_t color, uint32_t offset)
{
    WS2812_DETECT_S_.total  = num;
    WS2812_DETECT_S_.mode   = OFFSET;
    WS2812_DETECT_S_.color  = color;
    WS2812_DETECT_S_.offset = offset;

    memset(buf_1, 0, sizeof(buf_1));
    memset(buf_2, 0, sizeof(buf_2));
}

/* 命令接收相关 */
#include <stdlib.h>
#include <stdio.h>
typedef enum{
    COMMAND_TRIGGER = 0,
    COMMAND_DIRECT
}WS2812_COMMAND_TYPE;
static struct {
    WS2812_COMMAND_TYPE command_type;
    uint32_t led_num;
    uint32_t block_num;
    WS2812_CONTROL_BLOCK *block;
    
    uint32_t block_filled_index;
}BLOCK_Command_Generator_buf = {0};


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
int8_t WS2812_BLOCK_Command_Generator(uint8_t* data, uint8_t data_len);
int8_t WS2812_BLOCK_Command_Fill(uint8_t* data, uint8_t data_len);
int8_t WS2812_BLOCK_Command_Trigger(uint8_t* data, uint8_t data_len);
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
            case 0xA0:  return WS2812_BLOCK_Command_Generator(data, data_len);
            case 0xA1:  return WS2812_BLOCK_Command_Fill(data, data_len);
            case 0xA2:  return WS2812_BLOCK_Command_Trigger(data, data_len);
            default:    break;
        }
    }
    return 1;
}

/* 定义各命令帧解析函数 */
// 0xA0
int8_t WS2812_BLOCK_Command_Generator(uint8_t* data, uint8_t data_len)
{
    BLOCK_Command_Generator_buf.command_type = *data;
    BLOCK_Command_Generator_buf.led_num = *(uint32_t*)(data+1);
    
    
    uint32_t block_num = *(uint32_t*)(data+5);
    BLOCK_Command_Generator_buf.block_num = block_num;
    BLOCK_Command_Generator_buf.block = (WS2812_CONTROL_BLOCK*)malloc(sizeof(WS2812_CONTROL_BLOCK) * block_num);
    if (BLOCK_Command_Generator_buf.block == NULL) {
        return 0; // 块过大
    }
    memset(BLOCK_Command_Generator_buf.block, 0, sizeof(WS2812_CONTROL_BLOCK) * block_num);
    
    BLOCK_Command_Generator_buf.block_filled_index = 0;

    static char output_buffer[] = "Command_block generated:\ntype:0\n"; 
    output_buffer[sizeof(output_buffer)-3] = BLOCK_Command_Generator_buf.command_type + '0';
    extern UART_HandleTypeDef huart3;
    HAL_UART_Transmit_DMA(&huart3, output_buffer, sizeof(output_buffer));
    WS2812_LOG("Command_block enerated:\ncommand_type: %d\nblock_num: %ld\nled_num: %ld\n", 
            (uint8_t)BLOCK_Command_Generator_buf.command_type, BLOCK_Command_Generator_buf.block_num, BLOCK_Command_Generator_buf.led_num);
}
// 0xA1
int8_t WS2812_BLOCK_Command_Fill(uint8_t* data, uint8_t data_len)
{
    uint32_t index = BLOCK_Command_Generator_buf.block_filled_index;
    if(index >= BLOCK_Command_Generator_buf.block_num)  return 0;

    BLOCK_Command_Generator_buf.block[index].start = *(uint32_t*)data;
    BLOCK_Command_Generator_buf.block[index].color = *(data+4) | *(data+5) << 4 | *(data+6) << 8;

    static char output_buffer[] = "\nCommand_block received:0000:00:00\n"; 
    output_buffer[sizeof(output_buffer)-6-3-3] = BLOCK_Command_Generator_buf.block[index].start /1000 % 10 + '0';
    output_buffer[sizeof(output_buffer)-5-3-3] = BLOCK_Command_Generator_buf.block[index].start /100 % 10 + '0';
    output_buffer[sizeof(output_buffer)-4-3-3] = BLOCK_Command_Generator_buf.block[index].start /10 % 10 + '0';
    output_buffer[sizeof(output_buffer)-3-3-3] = BLOCK_Command_Generator_buf.block[index].start % 10 + '0';
    
    output_buffer[sizeof(output_buffer)-3-3] = BLOCK_Command_Generator_buf.block_num % 10 + '0';
    output_buffer[sizeof(output_buffer)-4-3] = BLOCK_Command_Generator_buf.block_num /10 % 10 + '0';
    
    output_buffer[sizeof(output_buffer)-3] = BLOCK_Command_Generator_buf.block_filled_index % 10 + '0';
    output_buffer[sizeof(output_buffer)-4] = BLOCK_Command_Generator_buf.block_filled_index /10 % 10 + '0';
    extern UART_HandleTypeDef huart3;
    HAL_UART_Transmit_DMA(&huart3, output_buffer, sizeof(output_buffer));
    WS2812_LOG("Command_block received:\nstart_num: %d\ncolor: 0x%06X", 
            (uint8_t)BLOCK_Command_Generator_buf.block[index].start, BLOCK_Command_Generator_buf.block[index].color);

    BLOCK_Command_Generator_buf.block_filled_index++;
    if(BLOCK_Command_Generator_buf.block_filled_index == BLOCK_Command_Generator_buf.block_num &&
        BLOCK_Command_Generator_buf.command_type == COMMAND_DIRECT)
        {
            const static char output_buffer2[] = "\nCommand_block apply!\n"; 
            HAL_UART_Transmit_DMA(&huart3, output_buffer2, sizeof(output_buffer2));
            WS2812_LOG("Command_block apply!");

            WS2812_START_by_BLOCK(
                BLOCK_Command_Generator_buf.led_num,
                BLOCK_Command_Generator_buf.block,
                BLOCK_Command_Generator_buf.block_num
            );

            
            return 1;
        }
    return 0;
}
// 0xA2
int8_t WS2812_BLOCK_Command_Trigger(uint8_t* data, uint8_t data_len)
{
    if(BLOCK_Command_Generator_buf.block_filled_index == BLOCK_Command_Generator_buf.block_num &&
        BLOCK_Command_Generator_buf.command_type == COMMAND_TRIGGER)
        {
            WS2812_START_by_BLOCK(
                BLOCK_Command_Generator_buf.led_num,
                BLOCK_Command_Generator_buf.block,
                BLOCK_Command_Generator_buf.block_num
            );

            WS2812_LOG("Command_block apply!.");
            return 1;
        }
    return 0;
}