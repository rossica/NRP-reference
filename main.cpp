#include <iostream>
#include <memory>

#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>


#include "server.h"
#include "config.h"

using namespace std;
using namespace nrpd;

int main(int argc, char* argv[])
{
    pid_t pid = 0, sid = 0;
    int retCode = 0;
    shared_ptr<NrpdConfig> config(new NrpdConfig);
    shared_ptr<NrpdServer> server;

    if(config  == NULL)
    {
        return 254;
    }

    server = make_shared<NrpdServer>(config);
    
    // Fork in the background to daemonize
    // TODO: make this configurable
    //pid = fork();
    if(pid < 0)
    {
        exit(EXIT_FAILURE);
    }
    if(pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    // Set filemask
    umask(0);

    //TODO: start logging here

    // create new session for child process
    //sid = setsid();
    if(sid < 0)
    {
        exit(EXIT_FAILURE);
    }

    // TODO: chdir here
    // if( chdir("/") < 0)
    // {
    //     exit(EXIT_FAILURE);
    // }

    // TODO: close standard file descriptors here
    // close(STDIN_FILENO);
    // close(STDOUT_FILENO);
    // close(STDERR_FILENO);



    // Specific configuration for nrpd server
    if((retCode = server->InitializeServer()) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    retCode = server->ServerLoop();
    printf("%d", retCode);

    
     

    return EXIT_SUCCESS;
}
