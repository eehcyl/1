#include "cam.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "LCD_Driver.h"

extern UART_HandleTypeDef huart2;

volatile uint8_t cam_detection  = CAM_VACANT;
volatile uint8_t cam_data_ready = 0;

static uint8_t  rx_buf[512];
static uint16_t rx_idx = 0;
static uint8_t  rx_char;
static float    frame_max_conf = 0.0f;
static char     frame_best_label[32] = {0};
/* Debug counters for visual verification */
static uint32_t dbg_init_count = 0;
static uint32_t dbg_rx_cb_count = 0;
static uint32_t dbg_lines_parsed = 0;
static uint32_t dbg_detect_confirmed = 0;

static int cam_strcasestr(const char *haystack, const char *needle)
{
    size_t nlen;
    size_t i;

    if (haystack == NULL || needle == NULL || needle[0] == '\0')
    {
        return 0;
    }

    nlen = strlen(needle);
    for (i = 0; haystack[i] != '\0'; i++)
    {
        size_t j = 0;
        while (j < nlen &&
               tolower((unsigned char)haystack[i + j]) ==
               tolower((unsigned char)needle[j]))
        {
            j++;
        }
        if (j == nlen)
        {
            return 1;
        }
    }

    return 0;
}

static void cam_trim_line(char *line)
{
    char *start = line;
    char *end;

    while (*start == ' ' || *start == '\t')
    {
        start++;
    }

    if (start != line)
    {
        memmove(line, start, strlen(start) + 1U);
    }

    end = line + strlen(line);
    while (end > line && (end[-1] == ' ' || end[-1] == '\t'))
    {
        end--;
    }
    *end = '\0';
}

static int cam_parse_prediction_line(const char *line, char *out_label, float *out_conf)
{
    const char *colon;
    const char *label_start;
    size_t label_len;

    colon = strrchr(line, ':');
    if (colon == NULL || colon == line)
    {
        return 0;
    }

    label_start = line;
    while (*label_start == ' ' || *label_start == '\t')
    {
        label_start++;
    }

    label_len = (size_t)(colon - label_start);
    if (label_len == 0U || label_len > 31U)
    {
        return 0;
    }

    strncpy(out_label, label_start, label_len);
    out_label[label_len] = '\0';
    cam_trim_line(out_label);

    *out_conf = (float)atof(colon + 1);
    if (*out_conf > 1.0f && *out_conf <= 100.0f)
    {
        *out_conf /= 100.0f;
    }

    if (*out_conf <= 0.0f || *out_conf > 1.0f || out_label[0] == '\0')
    {
        return 0;
    }

    if (cam_strcasestr(out_label, "ms") ||
        cam_strcasestr(out_label, "dsp") ||
        cam_strcasestr(out_label, "classification") ||
        cam_strcasestr(out_label, "anomaly"))
    {
        return 0;
    }

    return 1;
}

static void cam_apply_detection(const char *best_label, float max_conf)
{
    if (max_conf < 0.3f)
    {
        cam_detection = CAM_VACANT;
    }
    else if (cam_strcasestr(best_label, "vacant"))
    {
        cam_detection = CAM_VACANT;
    }
    else if (cam_strcasestr(best_label, "supine"))
    {
        cam_detection = CAM_SUPINE;
    }
    else if (cam_strcasestr(best_label, "left side") ||
             cam_strcasestr(best_label, "right side") ||
             cam_strcasestr(best_label, "side sleeping"))
    {
        cam_detection = CAM_SIDE;
    }
    else if (cam_strcasestr(best_label, "prone"))
    {
        cam_detection = CAM_PRONE;
    }
    else if (cam_strcasestr(best_label, "face-covered") ||
             cam_strcasestr(best_label, "face covered"))
    {
        cam_detection = CAM_FACE_COVERED;
    }
    else
    {
        cam_detection = (max_conf > 0.5f) ? CAM_OCCUPIED : CAM_VACANT;
    }

    cam_data_ready = 1;

    /* ??:?????????????? */
    dbg_detect_confirmed++;
    {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "DET:%u", cam_detection);
        LCD_Show_String(0, 120, "DETECT OK");
        LCD_Show_String(0, 136, tmp);
    }
}

static int cam_is_skippable_line(const char *line)
{
    if (line[0] == '\0')
    {
        return 1;
    }

    if (cam_strcasestr(line, "predictions"))
    {
        return 0;
    }

    return (cam_strcasestr(line, "sampling") ||
            cam_strcasestr(line, "starting inferencing") ||
            cam_strcasestr(line, "edge impulse") ||
            cam_strcasestr(line, "run classifier") ||
            cam_strcasestr(line, "capture") ||
            cam_strcasestr(line, "waiting"));
}

static void cam_reset_frame(void)
{
    frame_max_conf = 0.0f;
    frame_best_label[0] = '\0';
}

static void cam_process_rx_line(void)
{
    char label[32];
    float conf = 0.0f;

    if (rx_idx == 0U)
    {
        return;
    }

    rx_buf[rx_idx] = '\0';
    cam_trim_line((char *)rx_buf);

    if (cam_is_skippable_line((char *)rx_buf))
    {
        rx_idx = 0;
        return;
    }

    if (cam_strcasestr((char *)rx_buf, "predictions"))
    {
        cam_reset_frame();
        rx_idx = 0;
        return;
    }

    if (cam_parse_prediction_line((char *)rx_buf, label, &conf))
    {
        if (conf > frame_max_conf)
        {
            frame_max_conf = conf;
            strncpy(frame_best_label, label, sizeof(frame_best_label) - 1U);
            frame_best_label[sizeof(frame_best_label) - 1U] = '\0';
        }

        cam_apply_detection(frame_best_label, frame_max_conf);
        /* ??:?????? */
        dbg_lines_parsed++;
        {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%s", (char*)rx_buf);
            LCD_Show_String(0, 152, "LINE PARSED");
            LCD_Show_String(0, 168, tmp);
        }
    }

    rx_idx = 0;
}

static void cam_handle_rx_byte(uint8_t byte)
{
    if (byte == '\n' || byte == '\r')
    {
        cam_process_rx_line();
        return;
    }

    if (rx_idx < (sizeof(rx_buf) - 1U))
    {
        rx_buf[rx_idx++] = byte;
    }
    else
    {
        rx_idx = 0;
    }
}

/* ????:???? '\\n' ????????????????????? */
void cam_inject_string(const char *s)
{
    const uint8_t *p = (const uint8_t *)s;
    while (p && *p)
    {
        /* ????????????(cam_handle_rx_byte ? static,??????????) */
        cam_handle_rx_byte(*p++);
        /* ???,?????(??????)?????;??????????? */
        HAL_Delay(1);
    }
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        /* ????:????? */
        dbg_rx_cb_count++;
        {
            char t[16];
            snprintf(t, sizeof(t), "c:%02X", rx_char);
            LCD_Show_String(0, 88, "RX CB");
            LCD_Show_String(0, 104, t);
        }

        cam_handle_rx_byte(rx_char);

        /* ?????? */
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "I:%lu R:%lu L:%lu D:%lu",
                     (unsigned long)dbg_init_count,
                     (unsigned long)dbg_rx_cb_count,
                     (unsigned long)dbg_lines_parsed,
                     (unsigned long)dbg_detect_confirmed);
            LCD_Show_String(0, 184, buf);
        }

        HAL_UART_Receive_IT(&huart2, &rx_char, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);
        huart->ErrorCode = HAL_UART_ERROR_NONE;

        rx_idx = 0;
        cam_reset_frame();
        HAL_UART_Receive_IT(&huart2, &rx_char, 1);
    }
}

void cam_init(void)
{
    memset(rx_buf, 0, sizeof(rx_buf));
    rx_idx = 0;
    cam_reset_frame();

    cam_detection  = CAM_VACANT;
    cam_data_ready = 0;

    /* ???????(?? cam_init ???) */
    dbg_init_count++;
    LCD_Show_String(0, 40, "CAM INIT");
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "I:%lu R:%lu L:%lu D:%lu",
                 (unsigned long)dbg_init_count,
                 (unsigned long)dbg_rx_cb_count,
                 (unsigned long)dbg_lines_parsed,
                 (unsigned long)dbg_detect_confirmed);
        LCD_Show_String(0, 184, buf);
    }

    __HAL_UART_CLEAR_OREFLAG(&huart2);
    __HAL_UART_CLEAR_FEFLAG(&huart2);
    __HAL_UART_CLEAR_NEFLAG(&huart2);
    __HAL_UART_CLEAR_PEFLAG(&huart2);
    huart2.ErrorCode = HAL_UART_ERROR_NONE;

    {
        uint8_t dummy;
        while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET)
        {
            dummy = (uint8_t)(huart2.Instance->RDR & 0xFF);
            (void)dummy;
        }
    }

    HAL_UART_Receive_IT(&huart2, &rx_char, 1);
}

uint8_t cam_Get(void)
{
    return cam_detection;
}
