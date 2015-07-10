#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "config.h"

using namespace std;
using namespace nrpd;

int main(int argc, char* argv[])
{
    struct sockaddr_in host_addr, remote_addr;
    int listenfd, socketfd, pid;
    NrpdConfig* config = NULL;

    config = new NrpdConfig();
    if(config  == NULL)
    {
        return 254;
    }
    

 
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0)
    {
        // insert error code here
        cout << "Failed to create socket";
        return -1;
    }


    return 255;
}
