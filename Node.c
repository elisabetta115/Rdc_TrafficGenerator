#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>


#define BUFSIZE 256
#define PORT 9500
#define DESTINATION_ADDRESS "255.255.255.255"

typedef struct node {
    int id;
    time_t time;
    struct node * next;
} node_t;

void print_list(node_t * head) {
    node_t * current = head;
    while (current != NULL) {
        printf("%d:%ld\n", current->id, current->time);
        current = current->next;
    }
}

void push(node_t * head, int time, int id) {
    printf ("SONO nel push\n");
    fflush(stdout);
    node_t * current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    /* now we can add a new variable */
    current->next = (node_t *) malloc(sizeof(node_t));
    current->next->time = time;
    current->next->next = NULL;
}


void show_error(const char *msg){
    perror(msg);
    exit(1);
}

int msleep(long msec){
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

void broadcaster(int pipeG[2], int pipeA[2], int id){

    printf ("Start broadcaster\n");
    fflush(stdout);

    //close all pipe unuseful
    int n=close(pipeG[1]);
    if(n == -1) show_error("Close pipeG[1] in broadcaster");
    n = close(pipeA[0]);
    if(n == -1)show_error("ERROR Close pipeA[0]in broadcaster");

    //initialization of variables for send socket
    int sock_fd, port_num, ret;
    char buffer[BUFSIZE];
    struct sockaddr_in srv_addr;
    struct hostent *srv;

    //initialization of variable for listen socket
    int new_sock_fd, cli_len;
    struct sockaddr_in new_srv_addr;

    //open send socket 
    port_num = PORT;
    sock_fd = socket (AF_INET, SOCK_DGRAM, 0);
    if(sock_fd<0) show_error ("ERROR while opening socket");

    int broadcastPermission = 1;
    ret = setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission));
    if (ret) show_error("setsockopt");

    memset((char *)&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = inet_addr(DESTINATION_ADDRESS);
    srv_addr.sin_port = htons(port_num);

    //open listening socket
    new_sock_fd = socket (AF_INET, SOCK_DGRAM, 0);
    if(new_sock_fd<0) show_error ("ERROR while opening new_socket");

    int reuseAddressPermission = 1;
    ret = setsockopt(new_sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &reuseAddressPermission, sizeof(reuseAddressPermission));
    if (ret) show_error("setsockopt");

    memset((char*)&new_srv_addr, 0, sizeof(new_srv_addr));
    port_num = PORT;
    new_srv_addr.sin_family = AF_INET;
    new_srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    new_srv_addr.sin_port = htons(port_num);

    ret = bind(new_sock_fd, (struct sockaddr *)&new_srv_addr, sizeof(new_srv_addr));
    if (ret<0) show_error("ERROR while binding socket");



    //initialization os sequence number
    int seq_number = 100000000;
    srand(time(NULL));


   
    


    while(1){

        memset ((char *)buffer, 0, BUFSIZE);

        //initialization of fields for message
        int sender_id, message, new_seq_number;

        //read from pipeG
        int n = read(pipeG[0], buffer, BUFSIZE);
        if(n == -1){
            perror("WRITE Reads broadcaster");
            n=close(pipeG[0]);
            if(n == -1) show_error("Close pipeG[1] in broadcaster");
            n = close(pipeA[1]);
            if(n == -1)show_error("ERROR Close pipeA[0]in broadcaster ");
            exit(EXIT_FAILURE);
        } 
        if(n == 0){
            n=close(pipeG[0]);
            if(n == -1) show_error("Close pipeG[1] in broadcaster 2");
            n = close(pipeA[1]);
            if(n == -1)show_error("ERROR Close pipeA[0]in broadcaster 2");
            exit(EXIT_FAILURE);
        }
        else{
            //write on pipeA
            char s_id[BUFSIZE];
            sprintf(s_id, "%d", sender_id);
            int r = write(pipeA[1], s_id, sizeof(s_id));
            if(r == -1){
                n=close(pipeG[0]);
                if(n == -1) show_error("Close pipeG[1] in broadcaster 2");
                n = close(pipeA[1]);
                if(n == -1)show_error("ERROR Close pipeA[0]in broadcaster 2");
                exit(EXIT_FAILURE);
            }
            if(r == 0){
                n=close(pipeG[0]);
                if(n == -1) show_error("Close pipeG[1] in broadcaster 2");
                n = close(pipeA[1]);
                if(n == -1)show_error("ERROR Close pipeA[0]in broadcaster 2");
                exit(EXIT_FAILURE);
            }

            message = atoi(buffer);

            //send message
            printf ("Message read pipe: %d\n", message);
            fflush(stdout);

            sprintf(buffer, "%d:%d:0", id, message);
            n = sendto(sock_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &srv_addr, sizeof(srv_addr));
            if(n<0)show_error("ERROR sendto");

        }

        //read from read socket
        n = read (new_sock_fd, buffer, BUFSIZE);
        if(n<0)show_error("ERROR read");
        

        //message division
        int r = sscanf (buffer, "%d:%d:%d\n",&sender_id, &message,&new_seq_number);
        if(r<3)show_error("Scanf Message");
        

        //write on pipeA
        char s_id[BUFSIZE];
        sprintf(s_id, "%d", sender_id);
        r = write(pipeA[1], s_id, sizeof(s_id));
        if(r == -1)show_error("write broadcaster");
        if(r == 0){

            n = close(pipeA[1]);
            if(n == -1) show_error("Close 8");
            exit(EXIT_FAILURE);
        }

        //send message
        if(sender_id != id && new_seq_number<seq_number){   
            printf ("Message read socket: %d\n", message);
            fflush(stdout);

            seq_number = new_seq_number+1;
            sprintf(buffer, "%d:%d:%d", id, message, seq_number);
            n = sendto(sock_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &srv_addr, sizeof(srv_addr));
            if(n<0)show_error("ERROR sendto");
        } 
    }

    //close remaining pipe
    n=close(pipeG[0]);
    if(n == -1) show_error("Close pipeG[1] in broadcaster");
    n = close(pipeA[1]);
    if(n == -1)show_error("ERROR Close pipeA[0]in broadcaster");

    
    n = close (sock_fd);
    if(n<0)show_error("Error close sock_fd");

    n = close(new_sock_fd);
    if(n<0)show_error("Error close new_sock_fd");

    printf ("Finish broadcaster\n");
    fflush(stdout);

} 

void trafficGenerator (int pipeG[2]){

    int n = close(pipeG[0]);
    if(n == -1) show_error("Close 2");

    while(1){

        int message = rand();
        char buffer[BUFSIZE];
        sprintf(buffer, "%d", message);

        n = write(pipeG[1], buffer, BUFSIZE);
        if(n == -1){
            perror("WRITE TRAFFIC GENERATOR");
            n = close(pipeG[1]);
            if(n == -1) show_error("Close 1");
        }
        if(n==0){
            n = close(pipeG[1]);
            if(n == -1) show_error("Close 9");
            _exit(EXIT_FAILURE);
        }

        n = msleep(10000);
        if(n<0)show_error("ERROR msleep");
    }

    n = close(pipeG[1]);
    if(n == -1) show_error("Close 3");

}

void trafficAnalizer(int pipeA[2]){

    //creation linked list
    node_t * head = malloc(sizeof(node_t));
    int hasHead = 0;
    time_t time2;
    
    int n = close(pipeA[1]);
    if(n == -1)show_error("ERROR close 4");

    char buffer[BUFSIZE];

    while(1){

        n = read(pipeA[0] ,buffer, BUFSIZE);
        if (n<0)show_error("read traffic analizer");
        int sender_id = atoi(buffer);

        if(n == -1){
            perror("READ TRAFFIC ANALIZER");
            n = close(pipeA[0]);
            if(n == -1) show_error("Close 5");
        }
        if(n == 0){
            n = close(pipeA[0]);
            if(n == -1) show_error("Close 8");
            _exit(EXIT_FAILURE);
        }

        
        time2 = time(NULL)-time(&time2);
        if(hasHead == 0){
            head->time = time2;
            head->id = sender_id;
            head->next = NULL;
            hasHead = 1;
        }
        else push(head, time2, sender_id);

        print_list(head);

    }

    n = close(pipeA[0]);
    if(n == -1) show_error("Close 6");

    //printf ("End traffic A\n");
    //fflush(stdout);
}


int main(int argc, char** argv){
    int pipeG[2];
    int ret = pipe(pipeG);
    if (ret == -1) show_error("ERROR pipeBGENERATOR()");

    int pipeA[2];
    ret = pipe(pipeA);
    if (ret == -1) show_error("ERROR pipeA()");

    pid_t child1 = fork();
    if(child1 == -1){
        show_error("fork() child1");
        return 1;
    }
    if(child1>0){
        //printf ("Here\n");
        //fflush(stdout);
        trafficGenerator(pipeG);
        //printf ("Listen\n");
        //fflush(stdout);
        _exit(EXIT_SUCCESS);
    }
    else{

        pid_t child2 = fork();
        if(child2 == -1){
            show_error("fork() child2");
            return 1;
        }
        if(child2>0){
            //printf ("H\n");
            //fflush(stdout);
            trafficAnalizer(pipeA);
            //printf ("E\n");
            //fflush(stdout);
            _exit(EXIT_SUCCESS);
        }

        //printf ("Re\n");
        //fflush(stdout);

        broadcaster (pipeG,pipeA,atoi(argv[1]));
        exit(EXIT_SUCCESS);

        //printf ("Mare\n");
        //fflush(stdout);
    }

    return(0);
    
}




