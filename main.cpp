#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;


int main(int argc, char* argv[])
{
    struct sockaddr_in host_addr, remote_addr;
    int listenfd, socketfd, port, pid;

    port = 8080;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0)
    {
        // insert error code here
        cout << "Failed to create socket";
        return -1;
    }


    return -1;
}
