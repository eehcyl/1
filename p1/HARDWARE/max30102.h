#ifndef __MAX30102_H
#define __MAX30102_H

#include "main.h"
#include "i2c.h"
#include <stdint.h>
#include <stdbool.h>

// ==============================
// MAX30102 ????(?? I2C1,?? PA15=SCL, PB7=SDA)
// ??:PA15 ? JTAG ??,??? CubeMX ??? JTAG
// ==============================
#define MAX30102_I2C            hi2c1      // ?? I2C1
#define I2C_WRITE_ADDR          0xAE       // ???
#define I2C_READ_ADDR           0xAF       // ???

// I2C ???(?? HAL ?)
#define i2c_transmit(pdata, data_size)  HAL_I2C_Master_Transmit(&MAX30102_I2C, I2C_WRITE_ADDR, pdata, data_size, 100)
#define i2c_receive(pdata, data_size)   HAL_I2C_Master_Receive(&MAX30102_I2C, I2C_READ_ADDR, pdata, data_size, 100)

// ==============================
// MAX30102 ?????
// ==============================
#define REG_INT_STATUS1         0x00
#define REG_INT_STATUS2         0x01
#define REG_INT_ENABLE1         0x02
#define REG_INT_ENABLE2         0x03
#define REG_FIFO_WR_PTR         0x04
#define REG_FIFO_OVF_CNT        0x05
#define REG_FIFO_RD_PTR         0x06
#define REG_FIFO_DATA           0x07
#define REG_FIFO_CONFIG         0x08
#define REG_MODE_CONFIG         0x09
#define REG_SPO2_CONFIG         0x0A
#define REG_LED1_PA             0x0C
#define REG_LED2_PA             0x0D
#define REG_TEMP_CONFIG         0x21
#define REG_TEMP_INTEGER        0x1F
#define REG_TEMP_FRACTION       0x20

// ==============================
// PPG 每帧长度（整段满后算 HR/SpO2，再清零重采；改此值须重调 MAX30102_FS_HZ）
// ==============================
#define CACHE_NUMS              50
#define PPG_DATA_THRESHOLD      15000   /* 手指检测；噪声大则调高 */
#define MAX30102_DATA_OK        1

/* 峰位置缓存上限，须 ≤ CACHE_NUMS */
#define MAX30102_MAX_PEAKS      24
#if MAX30102_MAX_PEAKS > CACHE_NUMS
#error "MAX30102_MAX_PEAKS must be <= CACHE_NUMS"
#endif

// ==============================
// 采样与心率（需按实际主循环速度微调）
// 估算：每调用一次 MAX30102_Get_DATA 且手指在传感器上，cache 增加 1 点。
// 峰-峰法与自相关都用：HR ≈ 60 * MAX30102_FS_HZ / 间隔(采样点)。
// 若心率整体偏高 → 把 FS 调小；整体偏低 → 把 FS 调大。
//
// 刷新方式说明（与商品设备的差异）：
// - 本工程：整段 CACHE_NUMS 采满 → 算一次 HR/SpO2 → 计数清零再采下一段（非重叠帧）。
// - 手环/手表：多为滑窗 + IIR/中值等平滑，界面 0.5~1s 更新一次“平滑后”的值。
// - 医用指夹：常见 2~5s 一批数据输出，或滑窗；连续重叠在抗抖上更好，但 RAM 与算力更大。
// 若改 CACHE_NUMS，请按 显示心率/真实心率 比例重标定 MAX30102_FS_HZ。
// 血氧：仅整帧采满 CACHE_NUMS 后计算一次（不滑动、不 EMA），测完数值保持稳定
// ==============================
/* 略降显示心率（用户反馈略偏高）；仍偏高再减 0.5~1.0 */
#define MAX30102_FS_HZ          25.5f

#define MAX30102_HR_VALID_LO    28
#define MAX30102_HR_VALID_HI    130

#define MAX30102_HR_HARMONIC_CAP    105

/* 血氧略抬（非医疗标定） */
#define MAX30102_SPO2_UI_GAIN   1.10f
#define MAX30102_SPO2_UI_ADD    4.5f

// ==============================
// ??????
// ==============================
extern uint8_t max30102_int_flag;           // ????
extern float ppg_data_cache_RED[CACHE_NUMS]; // RED ???
extern float ppg_data_cache_IR[CACHE_NUMS];  // IR ???
extern uint16_t cache_counter;               // ?????

// ==============================
// ????
// ==============================
void max30102_init(void);
void max30102_i2c_write(uint8_t reg_adder, uint8_t data);
void max30102_i2c_read(uint8_t reg_adder, uint8_t *pdata, uint8_t data_size);
void max30102_fifo_read(float *data);
uint16_t max30102_getHeartRate(float *input_data, uint16_t cache_nums);
uint16_t max30102_getHeartRate_autocorr(const float *input_data, uint16_t cache_nums);
float max30102_getSpO2(float *ir_input_data, float *red_input_data, uint16_t cache_nums);
uint8_t MAX30102_Get_DATA(uint16_t *HeartRate, float *SpO2, float max30102_data[2], float fir_output[2]);
void MAX30102_LCD_Data(uint16_t HeartRate, float SpO2, char *PHeartRate, char *PSpO2);

#endif /* __MAX30102_H */
