#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "cl_comm_thread.h"

#define PI_NAME "SIDEWAY\'S PI"
/* XXX: move ot header */
#define CHANNEL_A 7

int main() {

    printf("piController running \n");
    
    int socket1;
    socket1 = socket(AF_INET, SOCK_STREAM, 0);
    pthread_t comm_thread;
    uint led_array_buf_1[LED_ARRAY_LEN];
    uint led_array_buf_2[LED_ARRAY_LEN];
    uint led_array_buf_3[LED_ARRAY_LEN];
    uint *receiving_array;
    uint32_t *displaying_array;
    char control_pi_ip[] = CONTROL_PI_IP;
    char pi_name[] = PI_NAME;
    
    struct comm_thread_params comm_thread_params = {
        control_pi_ip,
        CONTROL_PI_ACC_PORT,
        pi_name,
        PI_NAME_LEN,
        LED_ARRAY_LEN,
        CHANNEL_A,
        led_array_buf_1,
        led_array_buf_2,
        led_array_buf_3,
        &receiving_array,
        &displaying_array
    };
    pthread_create(&comm_thread, NULL, run_net_comm_thread, &comm_thread_params);
    
    //runNetCom();
    printf("thread started, joining\n");
    pthread_join(comm_thread, NULL);

    return 0;
}