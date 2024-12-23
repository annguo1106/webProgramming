#include	"unp.h"
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

// locations that holds objects
char *hand[] = {NULL, NULL, NULL};
char *chop[] = {NULL, NULL, NULL};
char *assem[] = {NULL, NULL, NULL};

// message to client buffer
char buffer[2][MAXLINE];
char *iptr[2], *optr[2];

// current order
int order_cnt[2] = {0, 0, 0};
char **orders[2];

// timer settings
timer_t timer_cus[2];
int sec_cus = 30;
timer_t timer_chop[2];
int sec_chop = 5;

struct timerData{
    int player_id;
    char *task;
    int x, y;
};

void sig_chld(int signo){
	int status;
	pid_t pid;

	while((pid = waitpid(-1, &status, WNOHANG)) > 0);
    return;
}

void timer_handler(int signum, siginfo_t *info, void *context) {
    printf("\n- handler\n");
    struct timerData *data = (struct timerData *)info->si_value.sival_ptr;
    int player_id = data->player_id;
    char *task = data->task;
    int x = data->x;
    int y = data->y;

    if(player_id == 0 || player_id == 1){
        if(strcmp(task, "cus") == 0){
            mes12(player_id, 0, 0);
            mes12(player_id, 0, 1);
            order_cnt[player_id]--;
        }
        else if(strcmp(task, "chop") == 0){
            // put chopped lettuce on chopping board
            char *obj;
            if(strcmp(chop[player_id], "l0") == 0)
                obj = strdup("l1");
            else if(strcmp(chop[player_id], "t0") == 0)
                obj = strdup("t1");

            mes10(player_id, x, y, obj, 0, 0);
            mes10(player_id, x, y, obj, 0, 1);
            chop[player_id] = strdup(obj);
        }
        else{
            printf("handler error\n");
        }
    }
    else{
        printf("handler error\n");
    }

    printf("Timer expired for player ID: %d, task: %s, order_cnt: %d\n", player_id, task, order_cnt[player_id]);
}

timer_t create_timer(int seconds, int interval, int player_id, char *task, int x, int y) {
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;

    // Set up signal handler
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_sigaction = timer_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Configure the timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;

    struct timerData *data = malloc(sizeof(struct timerData));
    data->player_id = player_id;
    data->task = strdup(task);  // Duplicate the task string
    data->x = x;
    data->y = y;
    sev.sigev_value.sival_ptr = data;

    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    // Set the timer
    its.it_value.tv_sec = seconds;  // Initial expiration
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = interval; // Interval
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    printf("PID %d: Timer created with initial %d seconds, interval %d seconds, player ID: %d\n", getpid(), seconds, interval, player_id);
    return timerid;
}

void update_timer(timer_t timerid, int new_seconds, int new_interval) {
    struct itimerspec its;

    // Set new expiration times
    its.it_value.tv_sec = new_seconds;   // New initial expiration
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = new_interval; // New interval
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    printf("Timer updated to initial expiration %d seconds and interval %d seconds\n", new_seconds, new_interval);
}

char **get_new_order(int count){
    const char *available[] = { "i0", "i1", "i2"}; // "b1111", "b1011", "b0111", "b0011",
    int num_avai = sizeof(available) / sizeof(available[0]);

    // Allocate memory for the array of string pointers
    char **orders = malloc(count * sizeof(char *));
    if (orders == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Seed the random number generator
    srand(time(NULL));

    // Loop to randomly select orders
    for (int i = 0; i < count; i++) {
        int random_index = rand() % num_avai;
        
        // Allocate memory for the order string and copy the selected string into it
        orders[i] = strdup(available[random_index]);
        if (orders[i] == NULL) {
            perror("Failed to duplicate string");
            exit(EXIT_FAILURE);
        }
    }

    return orders;
}

void free_orders(char **orders, int count) {
    for (int i = 0; i < count; i++) {
        free(orders[i]);  // Free each individual string
    }
    free(orders);  // Free the array of string pointers
}

void mes10(int player_id, int x, int y, char *obj, int isNotHand, int sendto){
    char sendline[MAXLINE];
    snprintf(sendline, sizeof(sendline), "10 %d %d %d %s %d\n", player_id, x, y, obj, isNotHand);
    printf("sendline: %s\n", sendline);
    size_t len = strlen(sendline);

    // Check if there is enough space in the buffer
    // printf("iptr: %d\n", iptr[sendto]);
    if (iptr[sendto] + len < &buffer[sendto][MAXLINE]) {
        memcpy(iptr[sendto], sendline, len);
        iptr[sendto] += len;
    }
    else {
        fprintf(stderr, "Error: Not enough space in the buffer to add mes10.\n");
    }

    // printf("iptr after adding: %d\n", iptr[sendto]);
}

void mes11(int player_id, int from_x, int from_y, int to_x, int to_y, int sendto){
    char sendline[MAXLINE];
    snprintf(sendline, sizeof(sendline), "11 %d %d %d %d %d %s\n", player_id, from_x, from_y, to_x, to_y, hand[player_id]);

    size_t len = strlen(sendline);

    // Check if there is enough space in the buffer
    if (iptr[sendto] + len < &buffer[sendto][MAXLINE]) {
        memcpy(iptr[sendto], sendline, len);
        iptr[sendto] += len;
    }
    else {
        fprintf(stderr, "Error: Not enough space in the buffer to add mes11.\n");
    }
}

void mes12(int player_id, int complete, int sendto){
    char sendline[MAXLINE];
    snprintf(sendline, sizeof(sendline), "12 %d %d\n", player_id, complete);

    size_t len = strlen(sendline);

    // Check if there is enough space in the buffer
    if (iptr[sendto] + len < &buffer[sendto][MAXLINE]) {
        memcpy(iptr[sendto], sendline, len);
        // printf("cpy complete\n");
        iptr[sendto] += len;
    }
    else {
        fprintf(stderr, "Error: Not enough space in the buffer to add mes12.\n");
    }
}

void mes13(char **order, int player_id, int sendto){
    char sendline[MAXLINE];
    snprintf(sendline, sizeof(sendline), "13 %d", player_id);
    for(int i=0; i<10; i++){
        strcat(sendline, " ");
        strcat(sendline, order[i]);
    }
    strcat(sendline, "\n");

    size_t len = strlen(sendline);

    // Check if there is enough space in the buffer
    if (iptr[sendto] + len < &buffer[sendto][MAXLINE]) {
        memcpy(iptr[sendto], sendline, len);
        // printf("cpy complete\n");
        iptr[sendto] += len;
    }
    else {
        fprintf(stderr, "Error: Not enough space in the buffer to add mes13.\n");
    }
}

void mes14(int player_id, int sendto){
    char sendline[MAXLINE];
    snprintf(sendline, sizeof(sendline), "14 %d\n", player_id);

    size_t len = strlen(sendline);

    // Check if there is enough space in the buffer
    if (iptr[sendto] + len < &buffer[sendto][MAXLINE]) {
        memcpy(iptr[sendto], sendline, len);
        // printf("cpy complete\n");
        iptr[sendto] += len;
    }
    else {
        fprintf(stderr, "Error: Not enough space in the buffer to add mes14.\n");
    }
}

void mes99(int sendto){
    char sendline[5];
    snprintf(sendline, sizeof(sendline), "99\n");

    size_t len = strlen(sendline);

    // Check if there is enough space in the buffer
    if (iptr[sendto] + len < &buffer[sendto][MAXLINE]) {
        memcpy(iptr[sendto], sendline, len);
        // printf("cpy complete\n");
        iptr[sendto] += len;
    }
    else {
        fprintf(stderr, "Error: Not enough space in the buffer to add mes99.\n");
    }
}

void handle_message(char* recvline, int player_id){
    char obj[20];
    int from_x, from_y, to_x, to_y;
    int to_loc, action;
    printf("\n---- messsage -----\n");
    printf("recvline content: %s\n", recvline);
    
    if(sscanf(recvline, "%s %d %d %d %d %d %d", obj, &from_x, &from_y, &to_x, &to_y, &to_loc, &action) == 7){
        if(strcmp(obj, "l0") == 0){
            if(to_loc == 0){
                // to hand
                if(strcmp(hand[player_id], "") == 0){
                    // empty hand
                    mes10(player_id, to_x, to_y, "l0", 0, 0);
                    mes10(player_id, to_x, to_y, "l0", 0, 1);
                    hand[player_id] = strdup("l0");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("hand occupied\n");
                    mes14(player_id, player_id);
                }
            }
            else if(to_loc == 3){
                // hand to trash
                if(strcmp(hand[player_id], "l0") == 0){
                    // delete lettuce in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("not fit: hand: %s\n", hand[player_id]);
                    mes14(player_id, player_id);
                }
            }
            else if(to_loc == 1){
                // to chopping board
                if(strcmp(hand[player_id], "l0") == 0 && strcmp(chop[player_id], "") == 0){
                    // delete lettuce in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    // put lettuce on chopping board
                    mes10(player_id, to_x, to_y, "l0", 0, 0);
                    mes10(player_id, to_x, to_y, "l0", 0, 1);
                    chop[player_id] = strdup("l0");
                    printf("hand[%d]: %s, chop: %s\n", player_id, hand[player_id], chop[player_id]);
                    // set timer
                    timer_chop[player_id] = create_timer(sec_chop, 0, player_id, "chop", to_x, to_y);
                }
                else{
                    printf("not fit: hand: %s, chop: %s\n", hand[player_id], chop[player_id]);
                    mes14(player_id, player_id);
                }
            }
            else{
                mes14(player_id, player_id);
            }
        }
        else if(strcmp(obj, "l1") == 0){
            if(to_loc == 0){
                // chopping board to hands
                // if(chop[player_id])
            }
        }
        else if(strcmp(obj, "t0") == 0){
            if(to_loc == 0){
                // to hand
                if(strcmp(hand[player_id], "") == 0){
                    // empty hand
                    mes10(player_id, to_x, to_y, "t0", 0, 0);
                    mes10(player_id, to_x, to_y, "t0", 0, 1);
                    hand[player_id] = strdup("t0");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("hand occupied\n");
                    mes14(player_id, player_id);
                }
            }
            else if(to_loc == 3){
                // hand to trash
                if(strcmp(hand[player_id], "t0") == 0){
                    // delete lettuce in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("not fit: hand: %s\n", hand[player_id]);
                    mes14(player_id, player_id);
                }
            }
            else if(to_loc == 1){
                // to chopping board
                if(strcmp(hand[player_id], "t0") == 0 && strcmp(chop[player_id], "") == 0){
                    // delete tomato in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    // put tomato on chopping board
                    mes10(player_id, to_x, to_y, "t0", 0, 0);
                    mes10(player_id, to_x, to_y, "t0", 0, 1);
                    chop[player_id] = strdup("t0");
                    printf("hand[%d]: %s, chop: %s\n", player_id, hand[player_id], chop[player_id]);
                    // set timer
                    timer_chop[player_id] = create_timer(sec_chop, 0, player_id, "chop", to_x, to_y);
                }
                else{
                    printf("not fit: hand: %s, chop: %s\n", hand[player_id], chop[player_id]);
                    mes14(player_id, player_id);
                }
            }
            else{
                mes14(player_id, player_id);
            }
        }
        // else if(strcmp(obj, "t1") == 0){
        // }
        // else if(strcmp(obj, "m") == 0){
        // }
        // else if(strcmp(obj, "b") == 0){
        // }
        else if(strcmp(obj, "f0") == 0){
            if(to_loc == 0){
                // from origin to hand
                if(strcmp(hand[player_id], "c") == 0){
                    // cone in hand = 1st flavor + cone
                    mes10(player_id, to_x, to_y, "i0", 0, 0);
                    mes10(player_id, to_x, to_y, "i0", 0, 1);
                    hand[player_id] = strdup("i0");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else if(strcmp(hand[player_id], "i1") == 0){
                    // all flavor + cone
                    mes10(player_id, to_x, to_y, "i2", 0, 0);
                    mes10(player_id, to_x, to_y, "i2", 0, 1);
                    hand[player_id] = strdup("i2");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("take cone first\n");
                    mes14(player_id, player_id);
                }
            }
            else{
                mes14(player_id, player_id);
            }
        }
        else if(strcmp(obj, "f1") == 0){
            if(to_loc == 0){
                // from origin to hand
                if(strcmp(hand[player_id], "c") == 0){
                    // cone in hand = 2nd flavor + cone
                    mes10(player_id, to_x, to_y, "i1", 0, 0);
                    mes10(player_id, to_x, to_y, "i1", 0, 1);
                    hand[player_id] = strdup("i1");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else if(strcmp(hand[player_id], "i0") == 0){
                    // all flavor + cone
                    mes10(player_id, to_x, to_y, "i2", 0, 0);
                    mes10(player_id, to_x, to_y, "i2", 0, 1);
                    hand[player_id] = strdup("i2");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("take cone first\n");
                    mes14(player_id, player_id);
                }
            }
            else{
                mes14(player_id, player_id);
            }
        }
        else if(strcmp(obj, "c") == 0){
            if(to_loc == 0){
                // origin to hand
                if(strcmp(hand[player_id], "") == 0){
                    // empty hand
                    mes10(player_id, to_x, to_y, "c", 0, 0);
                    mes10(player_id, to_x, to_y, "c", 0, 1);

                    hand[player_id] = strdup("c");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("hand occupied\n");
                    mes14(player_id, player_id);
                }
            }
            else if(to_loc == 3){
                // hand to trash
                if(strcmp(hand[player_id], "c") == 0){
                    // delete cone in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("not fit: hand: %s\n", hand[player_id]);
                    mes14(player_id, player_id);
                }
            }
            else{
                mes14(player_id, player_id);
            }
        }
        // else if(strcmp(obj, "b1111") == 0){
        // }
        // else if(strcmp(obj, "b1011") == 0){
        // }
        // else if(strcmp(obj, "b0111") == 0){
        // }
        // else if(strcmp(obj, "b0011") == 0){
        // }
        else if(strcmp(obj, "i0") == 0){
            if(to_loc == 3){
                // hand to trash
                if(strcmp(hand[player_id], "i0") == 0){
                    // delete ice cream 1 in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("not fit: hand: %s\n", hand[player_id]);
                    mes14(player_id, player_id);
                }
            }
            else if(to_loc == 5){
                // to customer
                int cur = 10 - order_cnt[player_id];
                if(strcmp(hand[player_id], "i0") == 0 && strcmp(orders[player_id][cur], "i0") == 0){
                    // reset timer
                    update_timer(timer_cus[player_id], sec_cus + 1, sec_cus);
                    // delete ice cream 1 in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                    // complete order
                    order_cnt[player_id]--;
                    mes12(player_id, 1, 0);
                    mes12(player_id, 1, 1);
                }
                else{
                    printf("not fit: hand: %s, order: %s\n", hand[player_id], orders[player_id][cur]);
                    mes14(player_id, player_id);
                }
            }
            else{
                mes14(player_id, player_id);
            }
        }
        else if(strcmp(obj, "i1") == 0){
            if(to_loc == 3){
                // hand to trash
                if(strcmp(hand[player_id], "i1") == 0){
                    // delete ice cream 1 in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("not fit: hand: %s\n", hand[player_id]);
                    mes14(player_id, player_id);
                }
            }
            else if(to_loc == 5){
                // to customer
                int cur = 10 - order_cnt[player_id];
                if(strcmp(hand[player_id], "i1") == 0 && strcmp(orders[player_id][cur], "i1") == 0){
                    // reset timer
                    update_timer(timer_cus[player_id], sec_cus + 1, sec_cus);
                    // delete ice cream 1 in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                    // complete order
                    order_cnt[player_id]--;
                    mes12(player_id, 1, 0);
                    mes12(player_id, 1, 1);
                }
                else{
                    printf("not fit: hand: %s, order: %s\n", hand[player_id], orders[player_id][cur]);
                    mes14(player_id, player_id);
                }
            }
            else{
                mes14(player_id, player_id);
            }
        }
        else if(strcmp(obj, "i2") == 0){
            if(to_loc == 3){
                // hand to trash
                if(strcmp(hand[player_id], "i2") == 0){
                    // delete ice cream 1 in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                }
                else{
                    printf("not fit: hand: %s\n", hand[player_id]);
                    mes14(player_id, player_id);
                }
            }
            else if(to_loc == 5){
                // to customer
                int cur = 10 - order_cnt[player_id];
                if(strcmp(hand[player_id], "i2") == 0 && strcmp(orders[player_id][cur], "i2") == 0){
                    // reset timer
                    update_timer(timer_cus[player_id], sec_cus + 1, sec_cus);
                    // delete ice cream 1 in hand
                    mes10(player_id, from_x, from_y, "empty", 0, 0);
                    mes10(player_id, from_x, from_y, "empty", 0, 1);
                    hand[player_id] = strdup("");
                    printf("hand[%d]: %s\n", player_id, hand[player_id]);
                    // complete order
                    order_cnt[player_id]--;
                    mes12(player_id, 1, 0);
                    mes12(player_id, 1, 1);
                }
                else{
                    printf("not fit: hand: %s, order: %s\n", hand[player_id], orders[player_id][cur]);
                    mes14(player_id, player_id);
                }
            }
            else{
                mes14(player_id, player_id);
            }
        }
        else if(strcmp(obj, "0") == 0){
            if(to_loc == 6){
                // floor
                mes11(player_id, from_x, from_y, to_x, to_y, 0);
                mes11(player_id, from_x, from_y, to_x, to_y, 1);
            }
            else{
                printf("invalid player move\n");
                mes14(player_id, player_id);
            }
        }
    }
    else{
        printf("Invalid message type\n");
    }
    return;
}



int
main(int argc, char **argv)
{
	int					listenfd, connfd[2], val;
	pid_t				childpid;
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;
    char sendline[MAXLINE], recvline[MAXLINE];
    void sig_chld(int);

    // TCP server
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(SERV_PORT);
	Bind(listenfd, (SA *) &servaddr, sizeof(servaddr));
	Listen(listenfd, LISTENQ);
    Signal(SIGCHLD, sig_chld);

    fd_set rset, wset;
    int maxfd, n;
    char id[2][MAXLINE];
    bool eof[2] = {false, false, false};
    char *cliaddr1, *cliaddr2;
    printf("server listening...\n");

	for ( ; ; ) {
		clilen = sizeof(cliaddr);
        // user 1
		if((connfd[0] = accept(listenfd, (SA *) &cliaddr, &clilen)) < 0){
            if (errno == EINTR){
                    printf("error: eintr\n");
                    continue;
                }
            else err_sys("accept error");
        }
        cliaddr1 = inet_ntoa(cliaddr.sin_addr);

        if ((n = Read(connfd[0], id[0], MAXLINE)) == 0){
            err_quit("str_cli: server terminated prematurely");
        }
        id[0][n] = '\0';
        printf("player0: %s\n", id[0]);
        fflush(stdout);
        snprintf(sendline, sizeof(sendline), "You are the 1st player. Wait for the second one!\n");
        Writen(connfd[0], sendline, strlen(sendline));

        // user 2
        if((connfd[1] = accept(listenfd, (SA *) &cliaddr, &clilen)) < 0){
            if (errno == EINTR){
                    printf("error: eintr\n");
                    continue;
                }
            else err_sys("accept error");
        }
        cliaddr2 = inet_ntoa(cliaddr.sin_addr);

        if ((n = Read(connfd[1], id[1], MAXLINE)) == 0){
            err_quit("str_cli: server terminated prematurely");
        }
        id[1][n] = '\0';
        printf("player1: %s\n", id[1]);
        fflush(stdout);
        snprintf(sendline, sizeof(sendline), "You are the 2nd player.\n");
        Writen(connfd[1], sendline, strlen(sendline));

        snprintf(sendline, sizeof(sendline), "The second player is %s from %s.\n", id[1], cliaddr2);
        Writen(connfd[0], sendline, strlen(sendline));
        snprintf(sendline, sizeof(sendline), "The first player is %s from %s.\n", id[0], cliaddr1);
        Writen(connfd[1], sendline, strlen(sendline));

        // set fd nonblocking
        val = Fcntl(connfd[0], F_GETFL, 0);
	    Fcntl(connfd[0], F_SETFL, val | O_NONBLOCK);
        val = Fcntl(connfd[1], F_GETFL, 0);
	    Fcntl(connfd[1], F_SETFL, val | O_NONBLOCK);
        maxfd = max(connfd[0], connfd[1]);

		if ( (childpid = Fork()) == 0) {	/* child process */
			Close(listenfd);	/* close listening socket */

            int nready, nwritten;
            int n_user = 2;
            printf(" - %s and %s into gameroom -\n", id[0], id[1]);
            fflush(stdout);

            iptr[0] = optr[0] = buffer[0];	/* initialize buffer pointers */
	        iptr[1] = optr[1] = buffer[1];

            int money_p1 = 0, money_p2 = 0;
            // initialize empty locations
            hand[0] = strdup("");
            hand[1] = strdup("");
            chop[0] = strdup("");
            chop[1] = strdup("");
            assem[0] = strdup("");
            assem[1] = strdup("");

            // send order
            orders[0] = get_new_order(10);
            orders[1] = get_new_order(10);
            printf("mes13 buf0: %d\n", buffer[0]);
            printf("mes13 buf1: %d\n", buffer[1]);
            mes13(orders[0], 0, 0);
            mes13(orders[1], 1, 1);
            mes13(orders[0], 0, 1);
            mes13(orders[1], 1, 0);
            order_cnt[0] = 10;
            order_cnt[1] = 10;

            //set customer timer
            timer_cus[0] = create_timer(sec_cus, sec_cus, 0, "cus", 0, 0);
            timer_cus[1] = create_timer(sec_cus, sec_cus, 1, "cus", 0, 0);
            
            for( ; ; ){

                for(int k=0; k<2; k++){
                    if(order_cnt[k] == 0){
                        free(orders[k]);
                        orders[k] = get_new_order(10);
                        mes13(orders[k], k, 0);
                        mes13(orders[k], k, 1);
                        sec_cus -= 5;
                        update_timer(timer_cus[k], sec_cus + 2, sec_cus);
                        order_cnt[k] = 10;
                    }
                }

                FD_ZERO(&rset);
		        FD_ZERO(&wset);

                if (iptr[0] < &buffer[0][MAXLINE] && !eof[0])
                    FD_SET(connfd[0], &rset);
                if (iptr[1] < &buffer[1][MAXLINE] && !eof[1])
                    FD_SET(connfd[1], &rset);
                if (optr[0] != iptr[0])
			        FD_SET(connfd[0], &wset);
                if (optr[1] != iptr[1])
			        FD_SET(connfd[1], &wset);
                
                nready = select(maxfd+1, &rset, &wset, NULL, NULL);
                if(nready < 0){
                    if (errno == EINTR){
                        printf("error: eintr\n");
                        continue;
                    }
                    else err_sys("select error");
                }

                for(int k=0; k<2; k++){
                    // write to fd
                    if(FD_ISSET(connfd[k], &wset) && ( (n = iptr[k] - optr[k]) > 0)){
                        if ( (nwritten = write(connfd[k], optr[k], n)) < 0) {
                            if (errno != EWOULDBLOCK)
                                err_sys("write error to connfd[0]");
                        }
                        else {
                            fprintf(stderr, "%s: wrote %d bytes to connfd[%d]\n", gf_time(), nwritten, k);
                            optr[k] += nwritten;		/* # just written */
                            if (optr[k] == iptr[k])
                                optr[k] = iptr[k] = buffer[k];	/* back to beginning of buffer */
                        }
                    }
                }

                for(int k=0; k<2; k++){
                    // read from fd
                    memset(recvline, 0, sizeof(recvline));
                    if(FD_ISSET(connfd[k], &rset)){
                        if ( (n = read(connfd[k], recvline, MAXLINE)) == 0) {
                            // user quit
                            n_user--;
                            printf("Player %d quit, n_user = %d\n", k, n_user);
                            if(n_user == 1){
                                mes99(!k);
                                if (optr[k] == iptr[k])
                                    Shutdown(connfd[k], SHUT_WR);
                            }
                            else{
                                Shutdown(connfd[0], SHUT_WR);
                                Shutdown(connfd[1], SHUT_WR);
                                break;
                                // exit
                            }
                            eof[k] = true;
                        }
                        else if(n < 0){
                            if (errno != EWOULDBLOCK)
                                err_sys("read error on connfd");
                        }
                        else{
                            handle_message(recvline, k);
                            // printf("buffer0: %s\n", buffer[0]);
                            // printf("buffer1: %s\n", buffer[1]);
                        }
                    }
                }

            }
            Close(connfd[0]);
            Close(connfd[1]);

            printf("exit\n");
			exit(0);
		}
		Close(connfd[0]);			/* parent closes connected socket */
        Close(connfd[1]);
	}
}