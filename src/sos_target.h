
#ifndef SOS_TARGET_H
#define SOS_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif

    int SOS_target_init(SOS_runtime *sos_context, SOS_socket **target,
            char *host, int port);

    int SOS_target_connect(SOS_socket *target);

    int SOS_target_setup_for_accept(SOS_socket *target);

    int SOS_target_accept_connection(SOS_socket *target);

    int SOS_target_send_msg(SOS_socket *target, SOS_buffer *msg);

    int SOS_target_recv_msg(SOS_socket *target, SOS_buffer *reply);

    int SOS_target_disconnect(SOS_socket *tgt_conn);

    int SOS_target_destroy(SOS_socket *target);

#ifdef __cplusplus
}
#endif

#endif
