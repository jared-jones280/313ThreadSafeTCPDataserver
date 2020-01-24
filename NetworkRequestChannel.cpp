#include <netdb.h>
#include <thread>
#include "common.h"
#include "NetworkRequestChannel.h"
using namespace std;
void* connection_handler(void*);

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR FOR CLASS   R e q u e s t C h a n n e l  */
/*--------------------------------------------------------------------------*/

NRC::NRC(const string server_name, const string port)//client
{
    struct addrinfo hints, *res;
    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int status;
    //getaddrinfo("www.example.com", "3490", &hints, &res);
    if ((status = getaddrinfo(server_name.c_str(), port.c_str(), &hints, &res)) != 0) {
        cerr << "getaddrinfo: " << gai_strerror(status) << endl;
        exit(0);
    }

    // make a socket:
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0)
    {
        perror ("Cannot create scoket");
        exit(0);
    }

    // connect!
    if (connect(sockfd, res->ai_addr, res->ai_addrlen)<0)
    {
        perror ("Cannot Connect");
        exit(0);
    }
    //cout<<"Successfully connected"<<endl;
}

NRC::NRC(const string port, void (*handle_process_loop)(NRC*)) // server
{
    struct addrinfo hints, *serv;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &serv)) != 0) {
        cerr  << "getaddrinfo: " << gai_strerror(rv) << endl;
        exit(0);
    }
    if ((sockfd = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol)) == -1) {
        perror("server: socket");
        exit(0);
    }
    if (bind(sockfd, serv->ai_addr, serv->ai_addrlen) == -1) {
        close(sockfd);
        perror("server: bind");
        exit(0);
    }
    freeaddrinfo(serv); // all done with this structure

    if (listen(sockfd, 20) == -1) {
        perror("listen");
        exit(1);
    }

    cout << "server: waiting for connections..." << endl;
    char buf [1024];
    while(1)
    {  // main accept() loop
        sin_size = sizeof their_addr;
        int slave_socket = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (slave_socket == -1) {
            perror("accept");
            continue;
        }
        NRC * sc = new NRC(slave_socket);
        thread client_thread(handle_process_loop, sc);
        client_thread.detach();
    }

}
NRC::NRC(int fd){
    sockfd=fd;
}

NRC::~NRC()
{
    close(sockfd);
}

char* NRC::cread(int *len)
{
	char * buf = new char [MAX_MESSAGE];
	int length; 
	length = recv(sockfd, buf, MAX_MESSAGE,0);
	if (len)	// the caller wants to know the length
		*len = length;
	return buf;
}

int NRC::cwrite(void *msg, int len)
{
	if (len > MAX_MESSAGE){
		EXITONERROR("cwrite");
	}
	if (send(sockfd, msg, len,0) < 0){
		EXITONERROR("cwrite");
	}
	return len;
}

void* connection_handler (void* arg){
    int client_socket = *(int *) arg;

    char buf [1024];
    while (true){
        /*string prompt = "Welcome. Give me a number, I will double that for you: ";
        if (send(client_socket, prompt.c_str(), prompt.size() +1, 0) == -1){
            perror("server: Send failure");
            break;
        }*/
        if (recv (client_socket, buf, sizeof (buf), 0) < 0){
            perror ("server: Receive failure");
            exit (0);
        }
        int num = *(int *)buf;
        num *= 2;
        if (num == 0)
            break;
        if (send(client_socket, &num, sizeof (num), 0) == -1){
            perror("send");
            break;
        }
    }
    cout << "Closing client socket" << endl;
    close(client_socket);
}
