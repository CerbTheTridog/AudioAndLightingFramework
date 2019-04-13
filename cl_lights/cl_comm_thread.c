#include "cl_comm_thread.h"

static void* connect_to_server(int* comSocketIn, struct comm_thread_params *params){
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
       return ;
    } 
    printf("connection established!\n");
}

static void* register_pi(int* comSocketIn, struct comm_thread_params *params){
    struct reg_msg reg_msg = {
        "test pi a",
        params->strip_number,
    };
    int sent = send((*comSocketIn), (void*)&reg_msg, sizeof(struct reg_msg), 0);
    printf("sent name to server, sent %dbytes\n", sent);
    int entityNumber;
    //recv((*comSocketIn), &entityNumber, sizeof(int), 0);
    //printf("entityNumber received was: %d\n", entityNumber);
}

void* run_net_comm_thread(void* args){
    printf("running runNetCom\n");
    struct comm_thread_params *params = args;
    int commSocket = 0;
    
    connect_to_server(&commSocket, params);
    register_pi(&commSocket, params);
    printf("end runNetCom\n");
}

