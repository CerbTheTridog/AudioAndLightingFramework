#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "cl_lights.h"
#include <stdbool.h>

//#define SERVER_IP_S "10.0.0.99"
#define PI_NAME "SIDEWAY\'S PI"
//#define LOCAL_LISTEN_PORT 15557
//#define RECV_BUFF_SIZE 1024
#define MAX_CONNECTIONS 10

/* XXX: move to a .h to share with display pi's */
struct display_pi {
	struct reg_msg reg_msg;
	int socket;
	pthread_t thread;
	uint index;
};

uint led_array_buf_1[LED_ARRAY_LEN];
uint led_array_buf_2[LED_ARRAY_LEN];
uint led_array_buf_3[LED_ARRAY_LEN];

/* Pointers and lock for led buffers */
pthread_mutex_t rec_send_ptr_lock = PTHREAD_MUTEX_INITIALIZER;
bool new_data; /* investigate semiphores instead of looping and checking this bool */
uint *recording_array;
uint *sending_array;

/* Data structure and lock for tracking connections */
pthread_mutex_t pi_list_lock = PTHREAD_MUTEX_INITIALIZER;
struct display_pi pi_list[MAX_CONNECTIONS];
uint pi_count = 0;

/* XXX: remove */
uint recording_count;


/* XXX: move these to .h as "change_buf_ptr" and take two int**'s as parameters */

/*
 * Changes which of the two non-sending buffers is now recording and sets new
 * data to true
 */
void
change_record_buf()
{
    pthread_mutex_lock(&rec_send_ptr_lock);
	new_data = true;
	if (recording_array == led_array_buf_1) {
		printf("was recording in 1\n");
		recording_array = (sending_array == led_array_buf_2) ?
				led_array_buf_3 : led_array_buf_2;		
	} else if (recording_array == led_array_buf_2) {
		printf("was recording in 2\n");
		recording_array = (sending_array == led_array_buf_3) ?
				led_array_buf_1 : led_array_buf_3;		
	} else {
		printf("was recording in 3\n");
		recording_array = (sending_array == led_array_buf_1) ?
				led_array_buf_2 : led_array_buf_1;		
	}
	pthread_mutex_unlock(&rec_send_ptr_lock);
}

static void
print_rec_buf() {
    int i = 0;
    int len = LED_ARRAY_LEN;
    printf ("led array: |");
    while (i < len) {
        printf("%u,", (recording_array)[i]);
        i++;
    }
    printf("|\n");
}

static void
print_send_buf() {
    int i = 0;
    int len = LED_ARRAY_LEN;
    printf ("led array: |");
    while (i < len) {
        printf("%u,", (sending_array)[i]);
        i++;
    }
    printf("|\n");
}

void
change_send_buf()
{
    pthread_mutex_lock(&rec_send_ptr_lock);
    if (sending_array == led_array_buf_1) {
        sending_array = (recording_array == led_array_buf_2) ?
        		led_array_buf_3 : led_array_buf_2;
    } else if (sending_array == led_array_buf_2) {
        sending_array = (recording_array == led_array_buf_3) ?
        		led_array_buf_1 : led_array_buf_3;
    } else {
        sending_array = (recording_array == led_array_buf_1) ?
        		led_array_buf_2 : led_array_buf_1;
    }
    pthread_mutex_unlock(&rec_send_ptr_lock);
}


void*
run_conn_th(void* pi_ptr)
{
	struct display_pi *pi = pi_ptr;
	printf("run_conn_th: new thread running, name:%s,  index: %d,  id:%d\n",
			pi->reg_msg.name, pi->index, pi->thread);
	int send_arr_num = 0;
	int sent = 0;
	while(1) {
		sent = 0;

		if(new_data) {
			change_send_buf();
		}
		if (sending_array == led_array_buf_1) {
			send_arr_num = 1;
		} else if (sending_array == led_array_buf_2) {
			send_arr_num = 2;
		} else {
			send_arr_num = 3;
		}
		//printf("run_conn_th: running\n");


		sent = send((pi->socket), (void*)sending_array, (sizeof(uint32_t) * LED_ARRAY_LEN), 0);
    	printf("sent array%d, sent %dbytes:\n", send_arr_num, sent);
    	print_send_buf();
		/* XXX: write send buf *********************************************************************************************/

		pthread_mutex_lock(&rec_send_ptr_lock);
		new_data = false;
		pthread_mutex_unlock(&rec_send_ptr_lock);

		usleep(SEND_DELAY);
	}
	//pi_list[pi_count].
}

/* XXX: Currently accepts just one connection and checks its reg msg. Needs to
 * also populate directory and then loop to accept more connections */
void* accept_connections(int *com_sockets)
{
	printf("accept_connection running\n");
	int accept_socket = 0;
	struct sockaddr_in accept_address;
	memset(&accept_address, '0', sizeof(accept_address));
	accept_socket = socket(AF_INET, SOCK_STREAM, 0);
	accept_address.sin_family = AF_INET;
	accept_address.sin_addr.s_addr = htonl(INADDR_ANY);
	accept_address.sin_port = htons(CONTROL_PI_ACC_PORT);
	int true_val = 1;

	/* Immediately make port available again */
	setsockopt(accept_socket, SOL_SOCKET, SO_REUSEADDR, &true_val, sizeof(int));

	/* Bind socket */
	int toCheck = bind(accept_socket, (struct sockaddr*)&accept_address, sizeof(accept_address));
	if (toCheck) {
		printf("[ConnectionAccepter]: to check return was %d, errno was %s\n", toCheck, strerror(errno));
		return 0;
	}

	/* Loop through accepting a new connection and starting a thread for it */
	while (pi_count < MAX_CONNECTIONS) {
		printf("[ConnectionAccepter]: accepting a new connection\n");
		int conn_socket = 0;
		//char sendBuff[1025];
		//memset(sendBuff, '0', sizeof(sendBuff));

		/* Listen on set up socket */
		listen(accept_socket, 10);
		conn_socket = accept(accept_socket, (struct sockaddr*) NULL, NULL);
		if (conn_socket < 0) {
			printf("[ConnectionAccepter]: error accepting connection\n");
			goto err_out;
		}

		/* Set up data structures to receive reg_msg */
		printf("[ConnectionAccepter]: connection accepted!\n");
		int recvd_bytes;
		struct display_pi *new_pi = &pi_list[pi_count];

		/* Attempt to receive reg_msg */
		printf("[ConnectionAccepter]: about to recv\n");
		recvd_bytes = recv(conn_socket, &new_pi->reg_msg, sizeof(struct reg_msg), 0);
		if (recvd_bytes != sizeof(struct reg_msg)) {
			printf("[ConnectionAccepter]: did not receive sizeof(reg_msg) chars\n");
			goto err_out;
		}

		/* If reg_msg checks out, save pi details to pi list */
		printf("[ConnectionAccepter]: reg details; %s, %d\n", new_pi->reg_msg.name, new_pi->reg_msg.strip_number);
		new_pi->socket = conn_socket;
		new_pi->index = pi_count;

		/* Start up new thread to transmit to this pi */
		if (pthread_create(&new_pi->thread, NULL, run_conn_th, new_pi) ) {
			printf("[ConnectionAccepter]: thread creation failed");
			goto err_out;
		}
		pi_count++;

		/* XXX: needs to send confirmation messag */
		//int sent = send((*curr_p).socket, recvd_msg, sizeof(struct reg_msg), 0);
		//	printf("recvd %d chars \n", recvd);
		//const char* constMessage1 = message1;
		//printf("- %s", recvd_msg);
		//printf(" -\n");
	}
	/* XXX: Should instead periodically check number of open connections and start loop again if needed */
	printf("[ConnectionAccepter]: Reached MAX_ENTITIES, exiting accepter\n");
	return;

err_out:
	printf("Connection accepter erroring out\n");
	return;
}



//main thread starts up accept, comm, and calc threads
//joins all threads

/* XXX: For now, loop through populating each LED buffer with only 2 uints;
 * 1: which buff number it is, 1 2 or 3
 * 2: a global count of how many interations of populate have beend one
 */
void
run_color_calc()
{
	printf("color calc called\n");
	uint32_t color = 0;
	uint32_t red = UINT32_FULL_RED;
	uint32_t green = UINT32_FULL_GREEN;
	uint32_t blue = UINT32_FULL_BLUE;
	int cur_led = 0;
	uint array_num;
	while (1) {
		/*if (recording_array == led_array_buf_1) {
			array_num = 1;
		} else if (recording_array == led_array_buf_2) {
			array_num = 2;
		} else {
			array_num = 3;
		}

		recording_array[0] = array_num;
		recording_array[1] = recording_count;
		recording_count += 1; */

		/* At the end of the strip, change to the next color and go back to start */
		if (cur_led >= LED_ARRAY_LEN) {
			cur_led = 0;
			if (color == red) {
				color = green;
			} else if (color == green) {
				color = blue;
			} else {
				color = red;
			}
		}
		/* Clear the entire strip except for the one LED we want on */
		memset(recording_array, 0, (sizeof(uint32_t) * LED_ARRAY_LEN));
		recording_array[(cur_led)] = color;
		cur_led ++;
		print_rec_buf();
		change_record_buf();
		usleep(COLOR_GEN_DELAY);		
	}


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

	/* Write 0's to all arrays */
	memset(led_array_buf_1, '0', (LED_ARRAY_LEN * sizeof(uint32_t)));
	memset(led_array_buf_2, '0', (LED_ARRAY_LEN * sizeof(uint32_t)));
	memset(led_array_buf_3, '0', (LED_ARRAY_LEN * sizeof(uint32_t)));
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



