#ifndef _CAM_H_
#define _CAM_H_

#include "stm32g4xx_hal.h"

/* ?????? */
#define CAM_VACANT          0   /* ????/?? */
#define CAM_OCCUPIED        1   /* ??(??) */
#define CAM_SUPINE          2   /* ?? */
#define CAM_SIDE            3   /* ?? */
#define CAM_PRONE           4   /* ?? */
#define CAM_FACE_COVERED    5   /* ??/?? */

/* ???? — ? main.c ?? */
extern volatile uint8_t  cam_detection;       /* CAM_NONE / CAM_EBIKE / CAM_CAR / CAM_BOTH */
//extern volatile float    cam_car_conf;        /* car ??? */
//extern volatile float    cam_ebike_conf;      /* e-bike ??? */
extern volatile uint8_t  cam_data_ready;      /* ????????1, main????0 */

/* ???? */
void    cam_init(void);
uint8_t cam_Get(void);   /* ?? CAM_NONE / CAM_EBIKE / CAM_CAR / CAM_BOTH */

void cam_inject_string(const char *s);

#endif
