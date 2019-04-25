#include "cl_comm_thread.h"
#include "arpa/inet.h"
#include <unistd.h>

void
change_receive_buf(struct comm_thread_params *params)
{
    pthread_mutex_lock(params->recv_disp_ptr_lock);
    if ( *(params->receiving_array) == params->led_array_buf_1) {
        printf("was receiving in 1\n");
        *(params->receiving_array) = ((*params->displaying_array) == params->led_array_buf_2) ? params->led_array_buf_3 : params->led_array_buf_2;
    } else if (*(params->receiving_array) == params->led_array_buf_2) {
        printf("was receiving in 2\n");
        *(params->receiving_array) = ((*params->displaying_array) == params->led_array_buf_3) ? params->led_array_buf_1 : params->led_array_buf_3;
    } else {
        printf("was receiving in 3\n");
        *(params->receiving_array) = ((*params->displaying_array) == params->led_array_buf_1) ? params->led_array_buf_2 : params->led_array_buf_1;
    }
    pthread_mutex_unlock(params->recv_disp_ptr_lock);
}

static
void connect_to_server(int* comSocketIn, struct comm_thread_params *params){
    printf("running connectToServer\n");
    
    /* Create recv_buf the same size as the led_array_bufs and write 0's */
    char recvBuff[params->led_array_length * sizeof(uint32_t)];
    memset(recvBuff, '0', (params->led_array_length * sizeof(uint32_t)));

    if(((*comSocketIn) = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return;
    }
    printf("after socket create\n");

    struct sockaddr_in serverAddress; 
    memset(&serverAddress, '0', sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(params->control_pi_port); 

    if(inet_pton(AF_INET, params->control_pi_ip, &serverAddress.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return;
    }
        printf("after pton \n");

    if( connect((*comSocketIn), (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return;
    } 
    printf("connection established!\n");
    return;
}

static void register_pi(int* comSocketIn, struct comm_thread_params *params)
{
    struct reg_msg reg_msg = {
        "test pi a",
        params->strip_number,
    };
    int sent = send((*comSocketIn), (void*)&reg_msg, sizeof(struct reg_msg), 0);
    printf("sent name to server, sent %dbytes\n", sent);
    //int entityNumber;
    //recv((*comSocketIn), &entityNumber, sizeof(int), 0);
    //printf("entityNumber received was: %d\n", entityNumber);
}

static void
print_rec_buf(struct comm_thread_params *params) {
    int i = 0;
    int len = params->led_array_length;
    printf ("led array: |");
    while (i < len) {
        printf("%u,", (*params->receiving_array)[i]);
        i++;
    }
    printf("|\n");
}

static void
receive_data(int *commSocket, struct comm_thread_params *params)
{
    int len =  sizeof(uint32_t) * params->led_array_length;
    uint32_t *rec_array = *(params->receiving_array);
    printf("%p", commSocket);
    while(params->running) {
        int received = recv(*commSocket, rec_array, len, 0);
                printf("received %d\n", received);
        print_rec_buf(params);
        change_receive_buf(params);
        usleep(RECEIVE_DELAY);
    }
}

void*
run_net_comm_thread(void* args)
{
    
    printf("running runNetCom\n");
    struct comm_thread_params *params = args;
    int commSocket = 0;
    
    connect_to_server(&commSocket, params);
    register_pi(&commSocket, params); /* XXX: does this actually need params? */
    receive_data(&commSocket, params);
    printf("end runNetCom\n");
    return NULL;
}

void run_net_comm(struct comm_thread_params *params)
{
    pthread_t comm_thread;
    params->running = true;
    pthread_create(&comm_thread, NULL, run_net_comm_thread, params);    
    printf("thread started\n");
}
