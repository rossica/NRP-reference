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
#include <thread>


#include "server.h"
#include "config.h"
#include "client.h"

using namespace std;
using namespace nrpd;

int main(int argc, char* argv[])
{
    pid_t pid = 0, sid = 0;
    int retCode = 0;
    shared_ptr<NrpdConfig> config;
    shared_ptr<NrpdServer> server;
    shared_ptr<NrpdClient> client;

    config = make_shared<NrpdConfig>();

    if(config == nullptr)
    {
        return 254;
    }

    server = make_shared<NrpdServer>(config);
    client = make_shared<NrpdClient>(config);


    // TODO: make this configurable
    if(false) // daemonize == true
    {
        // Fork in the background to daemonize
        pid = fork();
        if(pid < 0)
        {
            exit(EXIT_FAILURE);
        }
        if(pid > 0)
        {
            exit(EXIT_SUCCESS);
        }
    }

    //TODO: start logging here

    // TODO: make this configurable
    if(false) // daemonize == true
    {
        // Set filemask
        umask(0);



        // create new session for child process
        sid = setsid();
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
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }


    // Specific configuration for nrpd server
    if((retCode = server->InitializeServer()) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    // Configuration and initialization for nrpd client
    if((retCode = client->InitializeClient()) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }


    thread serverThread(NrpdServer::ServerThread, server);
    thread clientThread(NrpdClient::ClientThread, client);
    serverThread.join();
    clientThread.join();
    //retCode = server->ServerLoop();
    //printf("%d", retCode);




    return EXIT_SUCCESS;
}
