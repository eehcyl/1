/**
 ******************************************************************************
 * @file           : max30102.c
 * @brief          : 血氧传感器驱动(无调试输出)
 ******************************************************************************
 */

#include "max30102.h"
#include "max30102_fir.h"
#include "stdio.h"
#include <math.h>

// 全局变量
uint8_t max30102_int_flag = 0;
float ppg_data_cache_RED[CACHE_NUMS] = {0};
float ppg_data_cache_IR[CACHE_NUMS] = {0};
uint16_t cache_counter = 0;

// 用于保存有效值的静态变量
static uint16_t last_valid_hr = 0;
static float last_valid_spo2 = 0;
static uint8_t stable_count = 0;

/**
 * @brief I2C 写入
 */
void max30102_i2c_write(uint8_t reg_adder, uint8_t data)
{
    uint8_t transmit_data[2];
    transmit_data[0] = reg_adder;
    transmit_data[1] = data;
    i2c_transmit(transmit_data, 2);
}

/**
 * @brief I2C 读取
 */
void max30102_i2c_read(uint8_t reg_adder, uint8_t *pdata, uint8_t data_size)
{
    uint8_t adder = reg_adder;
    i2c_transmit(&adder, 1);
    i2c_receive(pdata, data_size);
}

/**
 * @brief MAX30102 初始化
 */
void max30102_init(void)
{
    uint8_t data;
    
    max30102_i2c_write(REG_MODE_CONFIG, 0x40);
    HAL_Delay(5);
    
    max30102_i2c_write(REG_INT_ENABLE1, 0xE0);
    max30102_i2c_write(REG_INT_ENABLE2, 0x00);
    
    max30102_i2c_write(REG_FIFO_WR_PTR, 0x00);
    max30102_i2c_write(REG_FIFO_OVF_CNT, 0x00);
    max30102_i2c_write(REG_FIFO_RD_PTR, 0x00);
    
    max30102_i2c_write(REG_FIFO_CONFIG, 0x4F);
    max30102_i2c_write(REG_MODE_CONFIG, 0x03);
    max30102_i2c_write(REG_SPO2_CONFIG, 0x27);
    
    /* LED 电流配置 */
    max30102_i2c_write(REG_LED1_PA, 0x32);
    max30102_i2c_write(REG_LED2_PA, 0x32);
    
    max30102_i2c_write(REG_TEMP_CONFIG, 0x01);
    
    max30102_i2c_read(REG_INT_STATUS1, &data, 1);
    max30102_i2c_read(REG_INT_STATUS2, &data, 1);
}

/**
 * @brief FIFO 读取
 */
void max30102_fifo_read(float *output_data)
{
    uint8_t receive_data[6] = {0};
    uint32_t data[2] = {0};
    
    HAL_Delay(1);
    max30102_i2c_read(REG_FIFO_DATA, receive_data, 6);
    HAL_Delay(1);
    
    data[0] = ((receive_data[0] << 16) | (receive_data[1] << 8) | receive_data[2]) & 0x03FFFF;
    data[1] = ((receive_data[3] << 16) | (receive_data[4] << 8) | receive_data[5]) & 0x03FFFF;
    
    *output_data = (float)data[0];
    *(output_data + 1) = (float)data[1];
}

/**
 * @brief 自相关估计心率
 */
uint16_t max30102_getHeartRate_autocorr(const float *input_data, uint16_t cache_nums)
{
    float mean = 0.0f;
    for (uint16_t i = 0; i < cache_nums; i++)
        mean += input_data[i];
    mean /= (float)cache_nums;

    float best = -1e30f;
    uint16_t best_lag = 0;
    const uint16_t lag_lo = 6u;
    uint16_t lag_hi = cache_nums * 3u / 4u;
    if (lag_hi > 180u)
        lag_hi = 180u;
    if (lag_hi <= lag_lo + 2u)
        return 0u;

    for (uint16_t lag = lag_lo; lag < lag_hi; lag++)
    {
        float acc = 0.0f;
        for (uint16_t i = 0; i + lag < cache_nums; i++)
        {
            float a = input_data[i] - mean;
            float b = input_data[i + lag] - mean;
            acc += a * b;
        }
        if (acc > best)
        {
            best = acc;
            best_lag = lag;
        }
    }

    if (best_lag < lag_lo || best <= 0.0f)
        return 0u;

    uint32_t lag = (uint32_t)best_lag;
    uint16_t hr = (uint16_t)(60.0f * MAX30102_FS_HZ / (float)lag + 0.5f);
    uint8_t k;
    for (k = 0; k < 4u && hr > MAX30102_HR_HARMONIC_CAP && lag * 2u < (uint32_t)cache_nums * 3u; k++)
    {
        lag *= 2u;
        hr = (uint16_t)(60.0f * MAX30102_FS_HZ / (float)lag + 0.5f);
    }

    if (hr >= MAX30102_HR_VALID_LO && hr <= MAX30102_HR_VALID_HI)
        return hr;
    return 0u;
}

/**
 * @brief 获取心率
 */
uint16_t max30102_getHeartRate(float *input_data, uint16_t cache_nums)
{
    float mean = 0;
    uint16_t i;
    uint16_t peak_count = 0;
    uint16_t peak_positions[MAX30102_MAX_PEAKS] = {0};
    
    for (i = 0; i < cache_nums; i++)
        mean += input_data[i];
    mean = mean / (float)cache_nums;
    
    const float peak_floor = mean * 1.008f;
    for (i = 1; i < cache_nums - 1u; i++)
    {
        if (input_data[i] > input_data[i - 1] && input_data[i] > input_data[i + 1] &&
            input_data[i] > peak_floor)
        {
            if (peak_count < MAX30102_MAX_PEAKS)
            {
                peak_positions[peak_count] = i;
                peak_count++;
            }
        }
    }

    const uint16_t interval_min = 5u;
    uint16_t interval_max = (cache_nums > 6u) ? (cache_nums - 1u) : 6u;
    if (interval_max < interval_min)
        interval_max = interval_min;

    if (peak_count >= 2)
    {
        uint16_t total = 0;
        uint16_t valid_count = 0;
        for (i = 1; i < peak_count; i++)
        {
            uint16_t interval = peak_positions[i] - peak_positions[i - 1];
            if (interval >= interval_min && interval <= interval_max)
            {
                total += interval;
                valid_count++;
            }
        }
        
        if (valid_count >= 1)
        {
            uint32_t avg = (uint32_t)(total / valid_count);
            uint16_t hr = (uint16_t)(60.0f * MAX30102_FS_HZ / (float)avg + 0.5f);
            uint8_t k;
            for (k = 0; k < 4u && hr > MAX30102_HR_HARMONIC_CAP && avg * 2u <= 300u; k++)
            {
                avg *= 2u;
                hr = (uint16_t)(60.0f * MAX30102_FS_HZ / (float)avg + 0.5f);
            }
            
            if (hr >= MAX30102_HR_VALID_LO && hr <= MAX30102_HR_VALID_HI)
                return hr;
        }
    }

    uint16_t hr_ac = max30102_getHeartRate_autocorr(input_data, cache_nums);
    return hr_ac;
}

/**
 * @brief 获取血氧
 */
float max30102_getSpO2(float *ir_input_data, float *red_input_data, uint16_t cache_nums)
{
    float ir_min = ir_input_data[0], ir_max = ir_input_data[0];
    float red_min = red_input_data[0], red_max = red_input_data[0];
    uint16_t i;
    
    for (i = 1; i < cache_nums; i++)
    {
        if (ir_input_data[i] < ir_min) ir_min = ir_input_data[i];
        if (ir_input_data[i] > ir_max) ir_max = ir_input_data[i];
        if (red_input_data[i] < red_min) red_min = red_input_data[i];
        if (red_input_data[i] > red_max) red_max = red_input_data[i];
    }
    
    float spo2_mm = 0.0f;
    if ((ir_max - ir_min) > 0.0f && (red_max + red_min) > 0.0f)
    {
        float Rmm = ((ir_max + ir_min) * (red_max - red_min)) /
                    ((red_max + red_min) * (ir_max - ir_min));
        spo2_mm = -45.060f * Rmm * Rmm + 30.354f * Rmm + 97.000f;
    }

    float ir_m = 0.0f, red_m = 0.0f;
    for (i = 0; i < cache_nums; i++)
    {
        ir_m += ir_input_data[i];
        red_m += red_input_data[i];
    }
    ir_m /= (float)cache_nums;
    red_m /= (float)cache_nums;

    float ir_ac = 0.0f, red_ac = 0.0f;
    if (ir_m > 10.0f && red_m > 10.0f)
    {
        float ir_v = 0.0f, red_v = 0.0f;
        for (i = 0; i < cache_nums; i++)
        {
            float di = ir_input_data[i] - ir_m;
            float dr = red_input_data[i] - red_m;
            ir_v += di * di;
            red_v += dr * dr;
        }
        ir_ac = sqrtf(ir_v / (float)cache_nums);
        red_ac = sqrtf(red_v / (float)cache_nums);
    }

    float spo2_ac = 0.0f;
    if (ir_ac > 1.0f && red_ac > 1.0f && ir_m > 1.0f && red_m > 1.0f)
    {
        float Rrat = (red_ac / red_m) / (ir_ac / ir_m);
        spo2_ac = 118.0f - 32.0f * Rrat;
    }

    float spo2;
    if (spo2_mm > 1.0f && spo2_ac > 1.0f)
        spo2 = 0.42f * spo2_mm + 0.58f * spo2_ac;
    else if (spo2_ac > 1.0f)
        spo2 = spo2_ac;
    else if (spo2_mm > 1.0f)
        spo2 = spo2_mm;
    else
        return 0.0f;

    spo2 = spo2 * MAX30102_SPO2_UI_GAIN + MAX30102_SPO2_UI_ADD;
    if (spo2 > 100.0f)
        spo2 = 100.0f;

    return spo2;
}

static uint16_t max30102_median3_u16(uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t t;
    if (a > b) { t = a; a = b; b = t; }
    if (b > c) { t = b; b = c; c = t; }
    if (a > b) { t = a; a = b; b = t; }
    return b;
}

/**
 * @brief MAX30102 数据获取函数
 */
uint8_t MAX30102_Get_DATA(uint16_t *HeartRate, float *SpO2, float max30102_data[2], float fir_output[2])
{
    static float last_ir = 0;
    static uint8_t no_finger_cnt = 0;
    static uint16_t hr_hist[3] = {0};
    static uint8_t hr_hist_n = 0;
    
    max30102_fifo_read(max30102_data);

    float ir_s = max30102_data[0];
    float red_s = max30102_data[1];
    ir_max30102_fir(&ir_s, &fir_output[0]);
    red_max30102_fir(&red_s, &fir_output[1]);
    
    // 手指检测
    if ((max30102_data[0] > PPG_DATA_THRESHOLD) && (max30102_data[1] > PPG_DATA_THRESHOLD))
    {
        no_finger_cnt = 0;

        if (cache_counter < CACHE_NUMS)
        {
            ppg_data_cache_IR[cache_counter] = fir_output[0];
            ppg_data_cache_RED[cache_counter] = fir_output[1];
            cache_counter++;
        }
    }
    else
    {
        no_finger_cnt++;
        if (no_finger_cnt >= 3u)
        {
            *HeartRate = 0u;
            *SpO2 = 0.0f;
        }
        if (no_finger_cnt >= 10u)
        {
            cache_counter = 0u;
            no_finger_cnt = 0u;
            hr_hist_n = 0u;
            hr_hist[0] = hr_hist[1] = hr_hist[2] = 0u;
        }
    }

    if (cache_counter >= CACHE_NUMS)
    {
        uint16_t raw_hr = max30102_getHeartRate(ppg_data_cache_IR, CACHE_NUMS);
        if (raw_hr >= MAX30102_HR_VALID_LO && raw_hr <= MAX30102_HR_VALID_HI)
        {
            hr_hist[hr_hist_n % 3u] = raw_hr;
            hr_hist_n++;
            if (hr_hist_n >= 3u)
                *HeartRate = max30102_median3_u16(hr_hist[0], hr_hist[1], hr_hist[2]);
            else
                *HeartRate = raw_hr;
        }
        else
        {
            *HeartRate = raw_hr;
        }

        *SpO2 = max30102_getSpO2(ppg_data_cache_IR, ppg_data_cache_RED, CACHE_NUMS);

        cache_counter = 0u;

        if ((*HeartRate >= MAX30102_HR_VALID_LO && *HeartRate <= MAX30102_HR_VALID_HI) ||
            (*SpO2 >= 65.0f && *SpO2 <= 100.0f))
        {
            return MAX30102_DATA_OK;
        }
    }

    return !MAX30102_DATA_OK;
}
