#ifndef CL_LIGHTS_H_
#define CL_LIGHTS_H_

#define PI_NAME_LEN 20
#define CONTROL_PI_IP "127.0.0.1"
#define CONTROL_PI_ACC_PORT 15556
#define LED_ARRAY_LEN 5
#define COLOR_GEN_DELAY 300000 /* microseconds */
#define SEND_DELAY 400000 /* microseconds */
#define UINT32_FULL_RED 1
#define UINT32_FULL_GREEN 2
#define UINT32_FULL_BLUE 3

struct reg_msg {
    char name[PI_NAME_LEN];
    uint strip_number;
};

#endif