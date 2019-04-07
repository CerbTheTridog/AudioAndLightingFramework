#ifndef CL_LIGHTS_H_
#define CL_LIGHTS_H_

#define PI_NAME_LEN 20
#define CONTROL_PI_IP "127.0.0.1"
#define CONTROL_PI_ACC_PORT 15556
#define LED_ARRAY_LEN 100

struct com_thread_params
{
    const char *control_pi_ip;
    uint32_t    control_pi_port;
    const char *pi_name;
    uint32_t    pi_name_length;
    uint32_t    led_array_length;
    uint32_t    strip_number;
    uint32_t   *led_array_buf1;
    uint32_t   *led_array_buf2;
    uint32_t   *led_array_buf3;
    uint32_t  **receiving_array;
    uint32_t  **displaying_array;
    pthread_mutex_t *recv_disp_ptr_lock;
};

struct reg_msg {
    char name[PI_NAME_LEN];
    uint strip_number;
};

#endif
