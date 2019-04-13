#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "cl_lights.h"

//#define SERVER_IP_S "10.0.0.99"
#define PI_NAME "SIDEWAY\'S PI"
//#define LOCAL_LISTEN_PORT 15557
//#define RECV_BUFF_SIZE 1024
#define MAX_CONNECTIONS 10
#define PI_NAME_LEN 20
#define LED_ARRAY_LEN 100

/* XXX: move to a .h to share with display pi's */
struct display_pi {
	struct reg_msg reg_msg;
	int socket;
};

uint led_array_buf_1[LED_ARRAY_LEN];
uint led_array_buf_2[LED_ARRAY_LEN];
uint led_array_buf_3[LED_ARRAY_LEN];

struct display_pi pi_list[MAX_CONNECTIONS];

uint *recording_array;
uint *sending_array;

int pi_count = 0;

/* XXX: remove */
uint recording_count;


/* XXX: Currently accepts just one connection and checks its reg msg. Needs to
 * also populate directory and then loop to accept more connections */
void* accept_connections(int *com_sockets){
	printf("accept_connection running\n");
	int accept_socket = 0;
	struct sockaddr_in accept_address;
	memset(&accept_address, '0', sizeof(accept_address));
	accept_socket = socket(AF_INET, SOCK_STREAM, 0);
	accept_address.sin_family = AF_INET;
	accept_address.sin_addr.s_addr = htonl(INADDR_ANY);
	accept_address.sin_port = htons(CONTROL_PI_ACC_PORT);

	int true = 1;

	/* Immediately make port available again */
	setsockopt(accept_socket, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));

	int toCheck = bind(accept_socket, (struct sockaddr*)&accept_address, sizeof(accept_address));
	if (toCheck) {
		printf("to check return was %d, errno was %s\n", toCheck, strerror(errno));
		return 0;
	}

	while (pi_count < MAX_CONNECTIONS) {
		int conn_socket = 0;
		//char sendBuff[1025];
		//memset(sendBuff, '0', sizeof(sendBuff));

		listen(accept_socket, 10);
		conn_socket = accept(accept_socket, (struct sockaddr*) NULL, NULL);
		if (conn_socket < 0) {
			printf("error accepting connection\n");
		}

		else {
			printf("connection accepted!\n");
			struct display_pi* curr_pi = calloc(1, sizeof(struct display_pi));
			(*curr_pi).socket = conn_socket;
			pi_list[pi_count] = *curr_pi;
			pi_count++;
			struct reg_msg recvd_msg;
			int recvd_bytes;

			printf("about to recv\n");
			recvd_bytes = recv(conn_socket, &recvd_msg, sizeof(struct reg_msg), 0);
			if (recvd_bytes != sizeof(struct reg_msg)) {
				printf("did not receive sizeof(reg_msg) chars\n");
				exit(1);
			}
			printf("\n");
			printf("reg details; %s, %d\n", recvd_msg.name, recvd_msg.strip_number);
			/* XXX: needs to send confirmation messag */
			//int sent = send((*curr_p).socket, recvd_msg, sizeof(struct reg_msg), 0);
			//	printf("recvd %d chars \n", recvd);
			//const char* constMessage1 = message1;
			//printf("- %s", recvd_msg);
			//printf(" -\n");
		}
	}
	/* XXX: Should instead periodically check number of open connections and start loop again if needed */
	printf("[ConnectionAccepter]: Reached MAX_ENTITIES, exiting accepter\n");
}

//main thread starts up accept, comm, and calc threads
//joins all threads

/* XXX: For now, loop through populating each LED buffer with only 2 uints;
 * 1: whih buff number it is, 1 2 or 3
 * 2: a global count of how many interations of populate have beend one
 */
void
run_color_calc()
{
	printf("color calc called, done\n");
	uint array_num;
	while (1) {
		if (recording_array == led_array_buf_1) {
			array_num = 1;
		} else if (recording_array == led_array_buf_2) {
			array_num = 2;
		} else {
			array_num = 3;
		}

		recording_array[0] = array_num;
		recording_array[1] = recording_count;
		recording_count += 1;
	}
	change_buf();
}

void* run_conn_accpt(){
	printf("running connection acceptor\n");
	int com_sockets[MAX_CONNECTIONS];
	uint loop_count = 0;
	while(1) {
		loop_count +=1;
		if (loop_count >5) {
			break;
		}
		printf("test\n");
		sleep(1);
	}
	while(1) {
		accept_connections(com_sockets);
	}

}

void
init() {
	recording_array = led_array_buf_1;
	sending_array = led_array_buf_2;
	recording_count = 0;
	uint i = 0;
	/* XXX  change to memset */
	while (i < LED_ARRAY_LEN) {
		led_array_buf_1[i] = 0;
		i++;
	}
	i = 0;
	while (i < LED_ARRAY_LEN) {
		led_array_buf_2[i] = 0;
		i++;
	}
	i = 0;
	while (i < LED_ARRAY_LEN) {
		led_array_buf_3[i] = 0;
		i++;
	}
}

//main thread starts up accept, comm, and calc threads
//joins all threads
int
main()
{
		printf("main running");
	init();
	pthread_t conn_accpt_th;
	pthread_create(&conn_accpt_th, NULL, run_conn_accpt, NULL);
	/* XXX: create calc thread instead of running it in this thread */
	run_color_calc();

	/* Does not yet do anyting with other thead */
	printf("main thread done, joining");
	pthread_join(conn_accpt_th, NULL);

	return 0;
	printf("CL_control_pi running \n");
}



