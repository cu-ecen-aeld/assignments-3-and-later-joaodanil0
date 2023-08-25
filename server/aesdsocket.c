
#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
 #include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>


#define PORT 9000
#define MESSAGE_FILE "/var/tmp/aesdsocketdata" 

int initializa_socket(){

    int socketfd;

    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if(socketfd < 0){
        perror("socket");
        return -1;
    }
    return socketfd;
}

int initialize_setsockopt(int socketfd, int option){

    int socketopt;

    socketopt = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    if(socketopt < 0){
        perror("socketopt");
        return -1;
    }
    return socketopt;
}

int initialize_socketbind(int socketfd, struct sockaddr_in address){
    
    int socketbind;

    socketbind = bind(socketfd, (struct sockaddr*)&address, sizeof(address));

    if(socketbind < 0){
        perror("bind");
        return -1;
    }
    return socketbind;
}

int initialize_socketlisten(int socketfd){
    
    int socketlisten;

    socketlisten = listen(socketfd, 3);

    if(socketlisten < 0){
        perror("listen");
        return -1;
    }

    return socketlisten;
}

int new_connection(int socketfd, struct sockaddr_in *address, int addrlen){

    int new_connection;

    new_connection = accept(socketfd, (struct sockaddr*) address, &addrlen);

    if(new_connection < 0){
        perror("accept");
        return -1;
    }

    return new_connection;
}

char *get_ip_client(struct sockaddr_in *client_info){

    struct sockaddr_in* pV4Addr = (struct sockaddr_in*) client_info;
    struct in_addr ipAddr = pV4Addr->sin_addr;

    char *str = (char *) malloc(INET_ADDRSTRLEN + 1 * sizeof(char));

    inet_ntop( AF_INET, &ipAddr, str, INET_ADDRSTRLEN );
    return str;
}

static void sigint_handler (int signo){
    // printf ("Caught SIGINT!\n");
    syslog(LOG_DEBUG, "Caught signal, exiting");
    remove(MESSAGE_FILE);
    exit (EXIT_SUCCESS);
}

static void sigterm_handler (int signo){
    // printf ("Caught SIGTERM!\n");
    syslog(LOG_DEBUG, "Caught signal, exiting");
    remove(MESSAGE_FILE);
    exit (EXIT_SUCCESS);
}

void register_signals(){
    if (signal (SIGINT, sigint_handler) == SIG_ERR){
        fprintf (stderr, "Cannot handle SIGINT");
        exit (EXIT_FAILURE);
    }

    if (signal (SIGTERM, sigterm_handler) == SIG_ERR){
        fprintf (stderr, "Cannot handle SIGTERM");
        exit (EXIT_FAILURE);
    }
}

int read_full_content(int fd, char *buf, int len){
    ssize_t ret;
    int total;

    while (len != 0 && (ret = read (fd, buf, len)) != 0) {
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            perror ("read");
            break;
        }
        len -= ret;
        buf += ret;
        total += ret;
    }

    return total;
}




int main(int argc, char *argv[]){

    int socketfd;
    int socketopt;
    int option = 1;
    struct sockaddr_in server_info = {0};
    socklen_t server_info_len = sizeof(server_info);
    struct sockaddr_in client_info = {0};
    socklen_t client_info_len = sizeof(client_info);

    int socketbind;
    int socketlisten;
    int valread;



    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(PORT);

    openlog(NULL, 0, LOG_USER);

    socketfd = initializa_socket(socketfd);    
    if (socketfd == -1) return -1;

    socketopt = initialize_setsockopt(socketfd, option);
    if (socketopt == -1) return -1;

    socketbind = initialize_socketbind(socketfd, server_info);
    if (socketbind == -1) return -1;

    if (argv[1] != NULL && (strcmp(argv[1],"-d") == 0)){
        pid_t pid;
        pid = fork();
        if (pid > 0) 
            exit(0);
        
    }

    socketlisten = initialize_socketlisten(socketfd);
    if (socketlisten == -1) return -1;

    
    register_signals();

    while (1){
        int new_client = new_connection(socketfd, &client_info, client_info_len);
        if (new_client == -1) return -1;
        
        char *client_ip = get_ip_client(&client_info);
        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        int fd;
        fd = open(MESSAGE_FILE, O_RDWR | O_CREAT | O_APPEND ,0664);
        
        int count;
        ioctl(new_client, FIONREAD, &count);
        char buffer[count];
        // printf("%d", count);
        // exit(1);
        valread = read(new_client, buffer, count+1);
        // valread = read_full_content(new_client, buffer, 1024);
        ssize_t nr;
        nr = write (fd, &buffer, valread);

        if(nr == -1){
            perror("write");
            return -1;
        }

        // char end_line = '\n';
        // nr = write (fd, &end_line, sizeof (end_line));
        
       FILE *fp = fopen(MESSAGE_FILE, "r");

        fseek(fp, 0, SEEK_END);
	    int length = ftell(fp);
	    fseek(fp, 0, SEEK_SET);

        char* file_buffer = malloc((length * sizeof(char)) + 2);
        fread(file_buffer, length + 1, 1, fp);
        file_buffer[length -1] = '\n';
        file_buffer[length] = '\0';

        // printf("%s", file_buffer);
        send(new_client, file_buffer, strlen(file_buffer), 0);
        close(fd);
        fclose(fp);
        close(new_client);
        free (file_buffer);
        syslog(LOG_DEBUG, "Closed connection from %s", client_ip);
        free (client_ip);
        
    }
    
    // closing the listening socket
    // shutdown(socketfd, SHUT_RDWR);

    return 0;
}