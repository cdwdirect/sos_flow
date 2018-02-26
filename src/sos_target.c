
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "sos.h"
#include "sos_debug.h"
#include "sos_target.h"

int
SOS_target_accept_connection(SOS_socket *target)
{
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_accept_connection");
    int i = 0;

    dlog(5, "Listening for a message...\n");
    target->peer_addr_len = sizeof(target->peer_addr);
    dlog(6, "  ... accepting\n");
    target->remote_socket_fd = accept(target->local_socket_fd,
            (struct sockaddr *) &target->peer_addr,
            &target->peer_addr_len);
    dlog(6, "  ... getting name info\n");
    i = getnameinfo((struct sockaddr *) &target->peer_addr,
            target->peer_addr_len, target->remote_host,
            NI_MAXHOST, target->remote_port, NI_MAXSERV,
            NI_NUMERICSERV);
    if (i != 0) {
        dlog(0, "Error calling getnameinfo() on client connection."
                "  (%s)\n", strerror(errno));
    }
    
    return i;
}


int
SOS_target_recv_msg(
        SOS_socket *target,
        SOS_buffer *reply)
{
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_recv_msg");
    SOS_msg_header header;

    if (SOS->status == SOS_STATUS_SHUTDOWN) {
        dlog(0, "Ignoring receive call because SOS is shutting down.\n");
        return -1;
    }

    int server_socket_fd = target->remote_socket_fd;

    if (reply == NULL) {
        dlog(0, "WARNING: Attempting to receive message into uninitialzied"
                " buffer.  Attempting to init/proceed...\n");
        SOS_buffer_init_sized_locking(SOS, &reply,
                SOS_DEFAULT_BUFFER_MAX, false);
    }

    int offset = 0;
    reply->len = recv(target->remote_socket_fd, reply->data,
            reply->max, 0);
    if (reply->len < 0) {
        //fprintf(stderr, "SOS: recv() call returned an error:\n\t\"%s\"\n",
                //strerror(errno));
        return reply->len;
    }

    memset(&header, '\0', sizeof(SOS_msg_header));
    if (reply->len >= sizeof(SOS_msg_header)) {
        int offset = 0;
        SOS_msg_unzip(reply, &header, 0, &offset);
    } else {
        fprintf(stderr, "SOS: Received malformed message:"
                " (bytes: %d)\n", reply->len);
        return -1;
    }

    // Check the size of the message. We may not have gotten it all.
    while (header.msg_size > reply->len) {
        int old = reply->len;
        while (header.msg_size > reply->max) {
            SOS_buffer_grow(reply, 1 + (header.msg_size - reply->max),
                    SOS_WHOAMI);
        }
        int rest = recv(target->remote_socket_fd, (void *) (reply->data + old),
                header.msg_size - old, 0);
        if (rest < 0) {
            fprintf(stderr, "SOS: recv() call for reply from"
                    " daemon returned an error:\n\t\"(%s)\"\n",
                    strerror(errno));
            return -1;
        } else {
            dlog(6, "  ... recv() returned %d more bytes.\n", rest);
        }
        reply->len += rest;
    }

    dlog(6, "Reply fully received.  reply->len == %d\n", reply->len);
    return reply->len;

}


int
SOS_target_init(
        SOS_runtime       *sos_context,
        SOS_socket       **target,
        char              *target_host,
        int                target_port)
{
    SOS_SET_CONTEXT(sos_context, "SOS_target_init");

    *target = calloc(1, sizeof(SOS_socket));
    SOS_socket *tgt = *target;
    tgt->sos_context = sos_context;

    tgt->send_lock = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_lock(tgt->send_lock);

    if (target_host != NULL) {
        strncpy(tgt->remote_host, target_host, NI_MAXHOST);
    } else {
        dlog(0, "WARNING: No host specified during a SOS_target_init."
                "  Defaulting to 'localhost'.\n");
        strncpy(tgt->remote_host, SOS_DEFAULT_SERVER_HOST, NI_MAXHOST);
    }

    snprintf(tgt->remote_port, NI_MAXSERV, "%d", target_port);

    tgt->buffer_len                = SOS_DEFAULT_BUFFER_MAX;
    tgt->timeout                   = SOS_DEFAULT_MSG_TIMEOUT;
    tgt->local_hint.ai_family     = AF_UNSPEC;     // Allow IPv4 or IPv6
    tgt->local_hint.ai_socktype   = SOCK_STREAM;   // _STREAM/_DGRAM/_RAW
    tgt->local_hint.ai_flags      = AI_NUMERICSERV;// Don't invoke namserv.
    tgt->local_hint.ai_protocol   = 0;             // Any protocol

    char local_hostname[NI_MAXHOST];
    gethostname(local_hostname, NI_MAXHOST);

    strncpy(tgt->local_host, local_hostname, NI_MAXHOST);
    strncpy(tgt->local_port, "0", NI_MAXSERV);
    tgt->port_number = 0;


    pthread_mutex_unlock(tgt->send_lock);

    return 0;
}

int
SOS_target_destroy(SOS_socket *target) {
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_destroy");

    pthread_mutex_lock(target->send_lock);
    pthread_mutex_destroy(target->send_lock);

    free(target->send_lock);
    free(target);

    return 0;
}

int
SOS_target_connect(SOS_socket *target) {
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_connect");

    int retval = 0;
    int new_fd = 0;

    dlog(8, "Obtaining target send_lock...\n");
    pthread_mutex_lock(target->send_lock);

    dlog(8, "Attempting to open server socket...\n");
    dlog(8, "   ...gathering address info.\n");
    target->remote_socket_fd = -1;
    retval = getaddrinfo(target->remote_host, target->remote_port,
        &target->remote_hint, &target->result_list);
    if (retval < 0) {
        dlog(0, "ERROR: Could not get info on target.  (%s:%s)\n",
            target->remote_host, target->remote_port );
        pthread_mutex_unlock(target->send_lock);
        return -1;
    }

    dlog(8, "   ...iterating possible connection techniques.\n");
    // Iterate the possible connections and register with the SOS daemon:
    for (target->remote_addr = target->result_list ;
        target->remote_addr != NULL ;
        target->remote_addr = target->remote_addr->ai_next)
    {
        new_fd = socket(target->remote_addr->ai_family,
            target->remote_addr->ai_socktype,
            target->remote_addr->ai_protocol);
        if (new_fd == -1) { continue; }

        retval = connect(new_fd, target->remote_addr->ai_addr,
            target->remote_addr->ai_addrlen);
        if (retval != -1) break;

        close(new_fd);
        new_fd = -1;
    }


    dlog(8, "   ...freeing unused results.\n");
    freeaddrinfo( target->result_list );

    if (new_fd <= 0) {
        dlog(0, "Error attempting to connect to the server.  (%s:%s)\n",
            target->remote_host, target->remote_port);
        pthread_mutex_unlock(target->send_lock);
        return -1;
    }

    target->remote_socket_fd = new_fd;
    dlog(8, "   ...successfully connected!"
            "  target->remote_socket_fd == %d\n", target->remote_socket_fd);

    return 0;
}


int SOS_target_disconnect(SOS_socket *target) {
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_disconnect");

    dlog(8, "Closing target file descriptor... (%d)\n",
            target->remote_socket_fd);

    close(target->remote_socket_fd);
    target->remote_socket_fd = -1;
    dlog(8, "Releasing target send_lock...\n");
    pthread_mutex_unlock(target->send_lock);

    dlog(8, "Done.\n");
    return 0;
}

int
SOS_target_send_msg(
        SOS_socket *target,
        SOS_buffer *msg)
{
    SOS_SET_CONTEXT(msg->sos_context, "SOS_target_send_msg");

    SOS_msg_header header;
    int            offset      = 0;
    int            inset       = 0;
    int            retval      = 0;
    double         time_start  = 0.0;
    double         time_out    = 0.0;

    if (SOS->status == SOS_STATUS_SHUTDOWN) {
        dlog(1, "Suppressing a send.  (SOS_STATUS_SHUTDOWN)\n");
        return -1;
    }

    if (SOS->config.offline_test_mode == true) {
        dlog(1, "Suppressing a send.  (OFFLINE_TEST_MODE)\n");
        return -1;
    }

    dlog(6, "Processing a send.\n");

    int more_to_send      = 1;
    int failed_send_count = 0;
    int total_bytes_sent  = 0;
    retval = 0;

    SOS_TIME(time_start);
    while (more_to_send) {
        if (failed_send_count >= 8) {
            dlog(0, "ERROR: Unable to contact target after 8 attempts.\n");
            more_to_send = 0;
            pthread_mutex_unlock(target->send_lock);
            return -1;
        }
        retval = send(target->remote_socket_fd, (msg->data + total_bytes_sent),
                msg->len, 0);
        if (retval < 0) {
            failed_send_count++;
            dlog(0, "ERROR: Could not send message to target."
                    " (%s)\n", strerror(errno));
            dlog(0, "ERROR:    ...retrying %d more times after"
                    " a brief delay.\n", (8 - failed_send_count));
            usleep(10000);
            continue;
        } else {
            total_bytes_sent += retval;
        }
        if (total_bytes_sent >= msg->len) {
            more_to_send = 0;
        }
    }//while

    dlog(6, "Send complete...\n");
    // Done!

    return total_bytes_sent;
}


