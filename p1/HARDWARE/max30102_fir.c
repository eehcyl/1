/**
 ******************************************************************************
 * @file           : max30102_fir.c
 * @brief          : ??????(?? ARM DSP ?)
 ******************************************************************************
 */

#include "max30102_fir.h"

#define BLOCK_SIZE  1   /* ???? arm_fir_f32 ???????? */
#define NUM_TAPS    29  /* ??????? */

static arm_fir_instance_f32 S_ir, S_red;
static float firStateF32_ir[BLOCK_SIZE + NUM_TAPS - 1];
static float firStateF32_red[BLOCK_SIZE + NUM_TAPS - 1];

/* ???????(?? MATLAB fdatool ??)*/
const float firCoeffs32LP[NUM_TAPS] = {
    -0.001542701735f, -0.002211477375f, -0.003286228748f, -0.00442651147f, -0.004758632276f,
    -0.003007677384f, 0.002192312852f, 0.01188309677f, 0.02637642808f, 0.04498152807f,
    0.06596207619f, 0.0867607221f, 0.1044560149f, 0.1163498312f, 0.1205424443f,
    0.1163498312f, 0.1044560149f, 0.0867607221f, 0.06596207619f, 0.04498152807f,
    0.02637642808f, 0.01188309677f, 0.002192312852f, -0.003007677384f, -0.004758632276f,
    -0.00442651147f, -0.003286228748f, -0.002211477375f, -0.001542701735f
};

/**
 * @brief FIR ??????
 */
void max30102_fir_init(void)
{
    arm_fir_init_f32(&S_ir, NUM_TAPS, (float32_t *)&firCoeffs32LP[0], &firStateF32_ir[0], BLOCK_SIZE);
    arm_fir_init_f32(&S_red, NUM_TAPS, (float32_t *)&firCoeffs32LP[0], &firStateF32_red[0], BLOCK_SIZE);
}

/**
 * @brief IR ?? FIR ??
 */
void ir_max30102_fir(float *input, float *output)
{
    arm_fir_f32(&S_ir, input, output, BLOCK_SIZE);
}

/**
 * @brief RED ?? FIR ??
 */
void red_max30102_fir(float *input, float *output)
{
    arm_fir_f32(&S_red, input, output, BLOCK_SIZE);
}
