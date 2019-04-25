#ifndef CL_COMM_THREAD_H_
#define CL_COMM_THREAD_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "cl_lights.h"
#include <stdbool.h>

#define RECEIVE_DELAY 300000 /* microseconds */

struct comm_thread_params
{
    const char *control_pi_ip;
    uint32_t    control_pi_port;
    const char *pi_name;
    uint32_t    pi_name_length;
    uint32_t    led_array_length;
    uint32_t    strip_number;
    uint32_t   *led_array_buf_1;
    uint32_t   *led_array_buf_2;
    uint32_t   *led_array_buf_3;
    uint32_t  **receiving_array;
    uint32_t  **displaying_array;
    pthread_mutex_t *recv_disp_ptr_lock;
    bool       new_data;
    bool       running;
};


void run_net_comm(struct comm_thread_params *params);
#endif
