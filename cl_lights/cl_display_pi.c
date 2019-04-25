#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include "cl_comm_thread.h"

#define PI_NAME "SIDEWAY\'S PI"
/* XXX: move ot header */
#define CHANNEL_A 7
#define MAX_CHECK_COUNT 50

int main() {

    printf("piController running \n");

    uint32_t led_array_buf_1[LED_ARRAY_LEN];
    uint32_t led_array_buf_2[LED_ARRAY_LEN];
    uint32_t led_array_buf_3[LED_ARRAY_LEN];
    uint32_t *receiving_array = led_array_buf_1;
    uint32_t *displaying_array = led_array_buf_2;
    char control_pi_ip[] = CONTROL_PI_IP;
    char pi_name[] = PI_NAME;
    pthread_mutex_t recv_disp_ptr_lock = PTHREAD_MUTEX_INITIALIZER;
    bool new_data = false;
    bool running = false;
   
    struct comm_thread_params params;
    params.control_pi_ip = control_pi_ip;
    params.control_pi_port = CONTROL_PI_ACC_PORT;
    params.pi_name = pi_name;
    params.pi_name_length =  PI_NAME_LEN;
    params.led_array_length = LED_ARRAY_LEN;
    params.strip_number = CHANNEL_A;
    params.led_array_buf_1 = led_array_buf_1;
    params.led_array_buf_2 = led_array_buf_2;
    params.led_array_buf_3 = led_array_buf_3;
    params.receiving_array = &receiving_array;
    params.displaying_array = &displaying_array;
    params.recv_disp_ptr_lock = &recv_disp_ptr_lock;
    params.new_data = &new_data;
    params.running = &running;
    
    run_net_comm(&params);

    int count = 0;
    while(1) {
        printf("display waiting\n");
        sleep(2);
        count++;
        if (count > MAX_CHECK_COUNT){
            break;
        }
    }
    running = false;
    sleep(2);
    printf("end display pi\n");
    return 0;
}