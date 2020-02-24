
#ifndef SOS_TARGET_H
#define SOS_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif

    int SOS_target_init(SOS_runtime *sos_context, SOS_socket **target,
            const char *host, int port);

    int SOS_target_connect(SOS_socket *target);

    int SOS_target_setup_for_accept(SOS_socket *target);

    int SOS_target_accept_connection(SOS_socket *target);

    int SOS_target_send_msg(SOS_socket *target, SOS_buffer *msg);

    int SOS_target_recv_msg(SOS_socket *target, SOS_buffer *reply);

    int SOS_target_recv_n_bytes(void *dest_ptr,
        int bytes_requested, SOS_socket *source);

    int SOS_target_disconnect(SOS_socket *tgt_conn);

    int SOS_target_destroy(SOS_socket *target);

    // Serialize information about this connection.
    // NOTE: It makes sense to add this here because we'll
    //       re-use it for discovery as well as utilities
    int SOS_target_description_write(FILE *into_file, SOS_socket *from_target);
    int SOS_target_description_read(SOS_socket *into_tgt, FILE *from_file);



#ifdef __cplusplus
}
#endif

#endif
