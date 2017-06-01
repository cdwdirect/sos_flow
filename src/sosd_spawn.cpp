#include "sos.h"
#include "mpi.h"
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <iterator>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>


int _commrank;
int _commsize;
int _daemon_rank;
int _daemon_pid;

void do_fork(std::string forkCommand) {
    std::istringstream iss(forkCommand);
    std::vector<std::string> tokens;
    copy(std::istream_iterator<std::string>(iss),
         std::istream_iterator<std::string>(),
         std::back_inserter(tokens));
    const char **args = (const char **)calloc(tokens.size()+1, sizeof(char*));
    for (int i = 0; i < tokens.size() ; i++) {
        args[i] = tokens[i].c_str();
    }
    int rc = execvp(args[0],const_cast<char* const*>(args));
    if (rc < 0) {
        perror("\nError in execvp");
    }
    // exit the daemon spawn!
    std::cout << "Daemon exited!" << std::endl;
    _exit(0);
}

extern "C" void fork_exec_sosd_shutdown(void) {
    // first, figure out who should fork a daemon on this node
    int i;
    int pid;
    if (_commrank == _daemon_rank) {
        usleep(1000);
        pid = vfork();
        if (pid == 0) {
            char* forkCommand;
            forkCommand = getenv ("SOS_FORK_SHUTDOWN");
            if (forkCommand) {
                std::cout << "Rank " << _commrank << " stopping SOS daemon(s): " << forkCommand << std::endl;
                std::string foo(forkCommand);
                do_fork(foo);
            } else {
                std::cout << "Please set the SOS_FORK_SHUTDOWN environment variable to stop SOS in the background." << std::endl;
            }
        }
    }
    //
    // wait until it is running
    //
    //wait(2);
    usleep(2000);
    // cleanup, just in case.
    kill(pid, SIGTERM);
    kill(_daemon_pid, SIGTERM);
}

extern "C" void send_shutdown_message(SOS_runtime *_runtime) {
    int i;
    SOS_buffer     *buffer;
    SOS_msg_header  header;
    int offset;
    // only have one client send the message
    if (_commrank != _daemon_rank) return;
    setenv("SOS_SHUTDOWN", "1", 1);
    // give the asynchronous sends time to flush
    usleep(1000);

    SOS_buffer_init(_runtime, &buffer);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
    header.msg_from = _runtime->my_guid;
    header.pub_guid = 0;

    offset = 0;
    const char * format = "iigg";
    SOS_buffer_pack(buffer, &offset, const_cast<char*>(format),
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);

    header.msg_size = offset;
    offset = 0;
    const char * format2 = "i";
    SOS_buffer_pack(buffer, &offset, const_cast<char*>(format2), header.msg_size);

    std::cout << "Sending SOS_MSG_TYPE_SHUTDOWN ..." << std::endl;

    SOS_send_to_daemon(buffer, buffer);

    SOS_buffer_destroy(buffer);
}

extern "C" void fork_exec_sosd(void) {
    PMPI_Comm_rank(MPI_COMM_WORLD, &_commrank);
    PMPI_Comm_size(MPI_COMM_WORLD, &_commsize);
    // first, figure out who should fork a daemon on this node
    int i;
    // get my hostname
    const int hostlength = 128;
    char hostname[hostlength] = {0};
    gethostname(hostname, sizeof(char)*hostlength);
    std::cout << hostname << std::endl;
    // make array for all hostnames
    char * allhostnames = (char*)calloc((hostlength * _commsize), sizeof(char));
    // copy my name into the big array
    char * host_index = allhostnames + (hostlength * _commrank);
    strncpy(host_index, hostname, hostlength);
    // get all hostnames
    PMPI_Allgather(hostname, hostlength, MPI_CHAR, allhostnames, 
                   hostlength, MPI_CHAR, MPI_COMM_WORLD);
    _daemon_rank = 0;
    // point to the head of the array
    host_index = allhostnames;
    // find the lowest rank with my hostname
    for (i = 0 ; i < _commsize ; i++) {
        //printf("%d:%d comparing '%s' to '%s'\n", _commrank, _commsize, hostname, host_index);
        if (strncmp(hostname, host_index, hostlength) == 0) {
            _daemon_rank = i;
            break;
        }
        host_index = host_index + hostlength;
    }
    std::cout << "Daemon rank: " << _daemon_rank << std::endl;
    // fork the daemon
    if (_commrank == _daemon_rank) {
        _daemon_pid = vfork();
        if (_daemon_pid == 0) {
            char* forkCommand = NULL;
            char* ranks_per_node = NULL;
            char* offset = NULL;
            forkCommand = getenv ("SOS_FORK_COMMAND");
            std::cout << "forkCommand " << forkCommand << std::endl;
            ranks_per_node = getenv ("SOS_APP_RANKS_PER_NODE");
            std::cout << "ranks_per_node " << ranks_per_node << std::endl;
            offset = getenv ("SOS_LISTENER_RANK_OFFSET");
            std::cout << "offset " << offset << std::endl;
            if (forkCommand) {
                std::string custom_command(forkCommand);
                size_t index = 0;
                index = custom_command.find("@LISTENER_RANK@", index);
                if (index != std::string::npos) {
                    if (ranks_per_node) {
                        int rpn = atoi(ranks_per_node);
                        int listener_rank = _commrank / rpn;
                        if(offset) {
                            int off = atoi(offset);
                            listener_rank = listener_rank + off;
                        }
                        std::stringstream ss;
                        ss << listener_rank;
                        custom_command.replace(index,15,ss.str());
                    }
                }
                std::cout << "Rank " << _commrank << " spawning SOS daemon(s): " << custom_command << std::endl;
                do_fork(custom_command);
            } else {
                std::cerr << "Please set the SOS_FORK_COMMAND environment variable to spawn SOS in the background." << std::endl;
            }
        }
    }
    //
    // wait until it is running
    //
    //wait(2);
}

