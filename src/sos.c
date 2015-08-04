
/*
 * sos.c                 SOS library routines
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "sos.h"
#include "sos_debug.h"


MPI_Comm SOS_COMM_WORLD;
MPI_Comm SOS_COMM_LOCAL;

MPI_Comm SOS_ICOMM_APP;
MPI_Comm SOS_ICOMM_MONITOR;
MPI_Comm SOS_ICOMM_DB;
MPI_Comm SOS_ICOMM_POWSCHED;
MPI_Comm SOS_ICOMM_ANALYTICS;


/* Private functions (not in the header file) */
void SOS_announce_to( SOS_pub_handle *pub, MPI_Comm target_comm, int target_rank);
void SOS_publish_to( SOS_pub_handle *pub, MPI_Comm target_comm, int target_rank);
void SOS_free_pub(SOS_pub_handle *pub);
void SOS_free_sub(SOS_sub_handle *sub);
void SOS_expand_data(SOS_pub_handle *pub);
void* SOS_refresh_sub( void *arg );


/* **************************************** */
/* [util]                                   */
/* **************************************** */


void SOS_init( int *argc, char ***argv, SOS_role role ) {
    int i;

    SOS.status = SOS_STATUS_INIT;
    SOS.role = role;

    SOS_SET_WHOAMI(whoami, "SOS_init");
    dlog(2, "[%s]: Initializing SOS...;\n", whoami);

    if (SOS.role == SOS_CLIENT) {
        SOS.config.cmd_port = atoi(getenv("SOS_CMD_PORT"));
        SOS.config.cmd_host = SOS_DEFAULT_LOCALHOST;
    } else {
        /* 
         * [sosd] and other roles will set POST + HOST manually...
         * but ALL clients should be connecting to the daemon on
         * the node where they are executing.
         */
    }
    
    SOS.config.buffer_len = SOS_DEFAULT_BUFFER_LEN;
    SOS.config.cmd_timeout = SOS_DEFAULT_CMD_TIMEOUT;
    SOS.config.argc = *argc;
    SOS.config.argv = *argv;

    
    SOS.uid.pub.next = SOS.uid.sub.next = SOS.uid.seq.next = 0;
    SOS.uid.pub.last = SOS.uid.sub.last = SOS.uid.seq.last = SOS_DEFAULT_LONG_MAX;
    pthread_mutex_init( &(SOS.uid.pub.lock), NULL );
    pthread_mutex_init( &(SOS.uid.sub.lock), NULL );
    pthread_mutex_init( &(SOS.uid.seq.lock), NULL );

    
    SOS.ring.send.read_pos = SOS.ring.send.write_pos = 0;
    SOS.ring.recv.read_pos = SOS.ring.recv.write_pos = 0;
    SOS.ring.send.size = SOS_DEFAULT_RING_SIZE;
    SOS.ring.recv.size = SOS_DEFAULT_RING_SIZE;
    SOS.ring.send.bytes = ( SOS.ring.send.size * sizeof(void *) );
    SOS.ring.recv.bytes = ( SOS.ring.recv.size * sizeof(void *) );
    SOS.ring.send.heap = (void **) malloc( SOS.ring.send.bytes );
    SOS.ring.recv.heap = (void **) malloc( SOS.ring.recv.bytes );
    memset( SOS.ring.send.heap, '\0', SOS.ring.send.bytes ); 
    memset( SOS.ring.recv.heap, '\0', SOS.ring.recv.bytes );
    pthread_mutex_init( &(SOS.ring.send.lock), NULL );
    pthread_mutex_init( &(SOS.ring.recv.lock), NULL );
   
    /*
     *  TODO:{ CHAD, INIT }
     *       Here we will request certain configuration things
     *       from the daemon, such as our client ID.
     */ 
    SOS.client_id = "TEMP_ID";

    
    /*
     *  TODO:{ CHAD, INIT } Start any threads...
     */
    
    return;
}



void SOS_finalize() {

    /* This will 'notify' any SOS threads to break out of their loops
     * and return here.  */
    SOS.status = SOS_STATUS_SHUTDOWN;

    /*
     *  TODO:{ CHAD, FINALIZE, THREADS } Join any threads.
     */

    pthread_mutex_destroy( &SOS.ring.send.lock );
    pthread_mutex_destroy( &SOS.ring.recv.lock );
    pthread_mutex_destroy( &SOS.uid.pub.lock   );
    pthread_mutex_destroy( &SOS.uid.sub.lock   );
    pthread_mutex_destroy( &SOS.uid.seq.lock   );
    pthread_mutex_destroy( &SOS.global_lock    );
    
    free( SOS.ring.send.heap );
    free( SOS.ring.recv.heap );
    
    return;
}




int SOS_next_id() {
    int next_serial;

    pthread_mutex_lock(&SOS_MUTEX_SERIAL);
    if (SOS_SERIAL_GENERIC_VAL > SOS_DEFAULT_SERIAL_GENERIC_MAX) {
        /* Default behavior is to cycle through values. */
        SOS_SERIAL_GENERIC_VAL = 0;
    }
    next_serial = SOS_SERIAL_GENERIC_VAL++;
    pthread_mutex_unlock(&SOS_MUTEX_SERIAL);

    return next_serial;
}



void SOS_expand_data( SOS_pub_handle *pub ) {
    int n;
    SOS_data **expanded_data;

    expanded_data = malloc((pub->elem_max + SOS_DEFAULT_ELEM_MAX) * sizeof(SOS_data *));
    memcpy(expanded_data, pub->data, (pub->elem_max * sizeof(SOS_data *)));
    for (n = pub->elem_max; n < (pub->elem_max + SOS_DEFAULT_ELEM_MAX); n++) {
        expanded_data[n] = malloc(sizeof(SOS_data));
        memset(expanded_data[n], '\0', sizeof(SOS_data)); }
    free(pub->data);
    pub->data = expanded_data;
    pub->elem_max = (pub->elem_max + SOS_DEFAULT_ELEM_MAX);

    return;
}



int SOS_msg_origin_puid( char *msg ) {
    int origin_puid;
    memcpy(&origin_puid, msg, sizeof(int));
    return origin_puid;
}

int SOS_msg_origin_rank( char *msg ) {
    int origin_rank;
    memcpy(&origin_rank, (msg + sizeof(int)), sizeof(int));
    return origin_rank;
}

SOS_role SOS_msg_origin_role( char *msg ) {
    int origin_role;
    memcpy(&origin_role, (msg + (sizeof(int) * 2)), sizeof(int));
    return (SOS_role) origin_role;
}


void SOS_strip_str( char *str ) {
    int i, len;
    len = strlen(str);

    for (i = 0; i < len; i++) {
        if (str[i] == '\"') str[i] = '\'';
        if (str[i] < ' ' || str[i] > '~') str[i] = '#';
    }
  
    return;
}



void SOS_apply_announce( SOS_pub_handle *pub, char *msg, int msg_size ) {
    //for extracting values from the msg:
    SOS_type val_type;
    int val_id;
    int val_name_len;
    char *val_name;
    int ptr;
    //misc
    int first_announce;

    char whoami[SOS_DEFAULT_STRING_LEN] = "[...]";
/*  memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);
    switch (SOS_ROLE) {
    case SOS_APP       : sprintf(whoami, "app(%d).apply_announce", SOS_RANK); break;
    case SOS_MONITOR   : sprintf(whoami, "monitor(%d).apply_announce", SOS_RANK); break;
    case SOS_DB        : sprintf(whoami, "db(%d).apply_announce", SOS_RANK); break;
    case SOS_POWSCHED  : sprintf(whoami, "powsched(%d).apply_announce", SOS_RANK); break;
    case SOS_ANALYTICS : sprintf(whoami, "analytics(%d).apply_announce", SOS_RANK); break;
    default: sprintf(whoami, "UNKOWN.apply_announce"); break;
    }
*/

    if (pub->elem_count < 1) {
        first_announce = 1;
        dlog(6, "[%s]: Applying a first announce to this publication.\n", whoami);
    } else {
        first_announce = 0;
        dlog(6, "[%s]: Applying a re-announce.\n", whoami);
    }
    dlog(6, "[%s]: >>> pub->elem_count=%d   pub->elem_max=%d   msg_size=%d\n", whoami, pub->elem_count, pub->elem_max, msg_size);

    ptr = 0;

    /* De-serialize the publication header... */

    if (!first_announce) {
        free(pub->origin_prog);
        free(pub->title);
    }

    pub->announced = 0;

    memcpy(&(pub->origin_puid), (msg + ptr), sizeof(int));  ptr += sizeof(int);           //[origin_puid]
    memcpy(&(pub->origin_rank), (msg + ptr), sizeof(int));  ptr += sizeof(int);           //[origin_rank]
    memcpy(&(pub->origin_role), (msg + ptr), sizeof(int));  ptr += sizeof(int);           //[origin_role]
    memcpy(&(pub->target_list), (msg + ptr), sizeof(int));  ptr += sizeof(int);           //[target_list]

    memcpy(&val_name_len, (msg + ptr), sizeof(int));        ptr += sizeof(int);           //[origin_prog_len]
    pub->origin_prog = (char*) malloc(1 + val_name_len);                                  //#
    memset(pub->origin_prog, '\0', (1 + val_name_len));                                   //#
    memcpy(pub->origin_prog, (msg + ptr), val_name_len);    ptr += (1 + val_name_len);    //[origin_prog_name]
  
    memcpy(&val_name_len, (msg + ptr), sizeof(int));        ptr += sizeof(int);           //[title_len]
    pub->title = (char*) malloc(1 + val_name_len);                                        //#
    memset(pub->title, '\0', (1 + val_name_len));                                         //#
    memcpy(pub->title, (msg + ptr), val_name_len);          ptr += (1 + val_name_len);    //[title]
 
    dlog(7, "[%s]: Publication header information...\n[%s]:\t"
	 "pub->origin_puid == %d\n[%s]:\tpub->origin_rank == %d\n[%s]:\tpub->origin_role == %d\n[%s]:\t"
	 "pub->origin_prog == \"%s\"\n[%s]:\tpub->title == \"%s\"\n",
	 whoami, whoami, pub->origin_puid, whoami, pub->origin_rank, whoami, pub->origin_role,
	 whoami, pub->origin_prog, whoami, pub->title);

    /* De-serialize the data elements... */

    dlog(6, "[%s]: BEFORE>>> pub->elem_count=%d   pub->elem_max=%d   msg_size=%d\n", whoami, pub->elem_count, pub->elem_max, msg_size);

    while (ptr < msg_size) {
        memcpy(&val_type, (msg + ptr), sizeof(int)); ptr += sizeof(int);
        memcpy(&val_id, (msg + ptr), sizeof(int)); ptr += sizeof(int);
        memcpy(&val_name_len, (msg + ptr), sizeof(int)); ptr+= sizeof(int);
        val_name = (msg + ptr);

        dlog(6, "[%s]: type=%d   id=%d   name_len=%d   name=\"%s\"\n", whoami, val_type, val_id, val_name_len, val_name);

        while (val_id >= pub->elem_max) {
            /* If there is not room for this entry, make room. */
            dlog(5, "[%s]: expanding elem_max from %d ", whoami, pub->elem_max);
            SOS_expand_data(pub);
            dlog(5, "to %d.\n", pub->elem_max);
        }

        if (!first_announce) {
            /* We're "re-announcing": */
            free(pub->data[val_id]->name);
            pub->data[val_id]->name = (char *) malloc(val_name_len + 1);
            memcpy(pub->data[val_id]->name, val_name, val_name_len);
            ptr += (1 + val_name_len);
        } else {
            /* This is a new announcement, so count up the elements: */
            if (val_type == SOS_STRING) pub->data[val_id]->val.c_val = (char*)SOS_TEMP_STRING;
            pub->data[val_id]->name = (char *) malloc(val_name_len + 1);
            memset(pub->data[val_id]->name, '\0', (val_name_len + 1));
            memcpy(pub->data[val_id]->name, val_name, val_name_len);
            ptr += (1 + val_name_len);
            pub->elem_count++;
        }

        pub->data[val_id]->name[val_name_len] = '\0';
        pub->data[val_id]->type = val_type;
        pub->data[val_id]->id = val_id;
    }

    dlog(6, "[%s]: AFTER<<< pub->elem_count=%d   pub->elem_max=%d   msg_size=%d\n", whoami, pub->elem_count, pub->elem_max, msg_size);

    return;
}



void SOS_apply_publish( SOS_pub_handle *pub, char *msg, int msg_len ) {
    //for extracting values from the msg:
    int ptr;
    int val_id;
    int val_len;
    double val_pack_ts;
    double val_send_ts;
    double val_recv_ts;
    //for inserting a string:
    char *new_str;
    //misc
    int i;
    char whoami[SOS_DEFAULT_STRING_LEN];


    memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);
    switch (SOS_ROLE) {
    case SOS_APP       : sprintf(whoami, "app(%d).apply_publish", SOS_RANK); break;
    case SOS_MONITOR   : sprintf(whoami, "monitor(%d).apply_publish", SOS_RANK); break;
    case SOS_DB        : sprintf(whoami, "db(%d).apply_publish", SOS_RANK); break;
    case SOS_POWSCHED  : sprintf(whoami, "powsched(%d).apply_publish", SOS_RANK); break;
    case SOS_ANALYTICS : sprintf(whoami, "analytics(%d).apply_publush", SOS_RANK); break;
    default: sprintf(whoami, "UNKOWN.apply_publish"); break;
    }

    if (SOS_DEBUG > 6) {
        for (i = 0; i < msg_len; i++) {
            printf("[%s]:   msg[%d] == %d\n", whoami, i, (int)msg[i]);
        }
    }

    dlog(6, "[%s]: Start... (pub->elem_count == %d)\n", whoami, pub->elem_count);

    ptr = 0;

    /* EVERY pub-related message gets a 3-int header for use in identification.
     * We pull it in during the announce, and it is still useful
     * for a message cracker to pick out the right data structure,
     * but for now we can skip it.
     *
     * Skipping:  [ active ][  rank  ][  role  ]...   */

    ptr += (sizeof(int) * 3);

    SOS_TIME( val_recv_ts );

    while (ptr < msg_len) {

        memcpy(&val_id,      (msg + ptr), sizeof(int));     ptr += sizeof(int);
        memcpy(&val_pack_ts, (msg + ptr), sizeof(double));  ptr += sizeof(double);
        memcpy(&val_send_ts, (msg + ptr), sizeof(double));  ptr += sizeof(double);
        memcpy(&val_len,     (msg + ptr), sizeof(int));     ptr += sizeof(int);

        if (val_id >= pub->elem_count) {
            SOS_warn_user(1, "[%s]: Publishing a value that is larger than the receiving data object:\n", whoami);
            SOS_warn_user(1, "[%s]:    pub{%s}->elem_count == %d;  new_val_id == %d;\n", whoami, pub->title, pub->elem_count, val_id);
            SOS_warn_user(1, "[%s]: This indicates that novel NAMEs were packed without an ANNOUNCE call.\n", whoami);
            SOS_warn_user(1, "[%s]: Discarding this entry and doing nothing.\n", whoami);
        } else {
            pub->data[val_id]->pack_ts = val_pack_ts;
            pub->data[val_id]->send_ts = val_send_ts;
            pub->data[val_id]->recv_ts = val_recv_ts;
            pub->data[val_id]->dirty = 1;
        }

        switch (pub->data[val_id]->type) {
        case SOS_INT : memcpy(   &(pub->data[val_id]->val.i_val), (msg + ptr), val_len); ptr += val_len; break;
        case SOS_LONG : memcpy(  &(pub->data[val_id]->val.l_val), (msg + ptr), val_len); ptr += val_len; break;
        case SOS_DOUBLE : memcpy(&(pub->data[val_id]->val.d_val), (msg + ptr), val_len); ptr += val_len; break;
        case SOS_STRING :
            if (pub->data[val_id]->val.c_val != (char*)SOS_TEMP_STRING) { 
                free(pub->data[val_id]->val.c_val);
            }
            pub->data[val_id]->val.c_val = (char *) malloc((1 + val_len) * sizeof(char));
            memset(pub->data[val_id]->val.c_val, '\0', (1 + val_len));
            memcpy(pub->data[val_id]->val.c_val, (msg + ptr), val_len); ptr += (1 + val_len); break;
        }

        switch (pub->data[val_id]->type) {
        case SOS_INT :    dlog(5, "[%s]: >>>> pub(\"%s\")->data[%d]->val == \"%d\"\n",  whoami, pub->title, val_id, pub->data[val_id]->val.i_val); break;
        case SOS_LONG :   dlog(5, "[%s]: >>>> pub(\"%s\")->data[%d]->val == \"%ld\"\n", whoami, pub->title, val_id, pub->data[val_id]->val.l_val); break;
        case SOS_DOUBLE : dlog(5, "[%s]: >>>> pub(\"%s\")->data[%d]->val == \"%lf\"\n", whoami, pub->title, val_id, pub->data[val_id]->val.d_val); break;
        case SOS_STRING : dlog(5, "[%s]: >>>> pub(\"%s\")->data[%d]->val == \"%s\"\n",  whoami, pub->title, val_id, pub->data[val_id]->val.c_val); break;
        }

        if (SOS_ROLE == SOS_DB) {
            dlog(7, "[%s]:                        ->pack_ts == %lf\n", whoami, pub->data[val_id]->pack_ts);
            dlog(7, "[%s]:                        ->send_ts == %lf\n", whoami, pub->data[val_id]->send_ts);
            dlog(7, "[%s]:                        ->recv_ts == %lf\n", whoami, pub->data[val_id]->recv_ts);
        }

    }

    dlog(6, "[%s]: ...done.\n", whoami);

    return;
}



int SOS_pack( SOS_pub_handle *pub, const char *name, SOS_type pack_type, SOS_val pack_val ) {
    //counter variables
    int i, n;
    //variables for working with adding pack_val SOS_STRINGs
    int new_str_len;
    char *new_str_ptr;
    char *pub_str_ptr;
    char *new_name;
    //misc
    char whoami[SOS_DEFAULT_STRING_LEN];

    memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);
    switch (SOS_ROLE) {
    case SOS_APP       : sprintf(whoami, "app(%d).pack", SOS_RANK); break;
    case SOS_MONITOR   : sprintf(whoami, "monitor(%d).pack", SOS_RANK); break;
    case SOS_DB        : sprintf(whoami, "db(%d).pack", SOS_RANK); break;
    case SOS_POWSCHED  : sprintf(whoami, "powsched(%d).pack", SOS_RANK); break;
    case SOS_ANALYTICS : sprintf(whoami, "analytics(%d).pack", SOS_RANK); break;
    default: sprintf(whoami, "UNKOWN.pack"); break;
    }

    //try to find the name in the existing pub schema:
    for (i = 0; i < pub->elem_count; i++) {
        if (strcmp(pub->data[i]->name, name) == 0) {

            dlog(6, "[%s]: (%s) name located at position %d.\n", whoami, name, i);

            switch (pack_type) {

            case SOS_STRING :
                pub_str_ptr = pub->data[i]->val.c_val;
                new_str_ptr = pack_val.c_val;
                new_str_len = strlen(new_str_ptr);

                if (strcmp(pub_str_ptr, new_str_ptr) == 0) {
                    dlog(5, "[%s]: Packed value is identical to existing value.  Updating timestamp and skipping.\n", whoami);
                    SOS_TIME(pub->data[i]->pack_ts);
                    return i;
                }

                free(pub_str_ptr);
                pub_str_ptr = malloc(new_str_len + 1);
                strncpy(pub_str_ptr, new_str_ptr, new_str_len);
                pub_str_ptr[new_str_len + 1] = '\0';

                dlog(6, "[%s]: assigning a new string.   \"%s\"   (updating)\n", whoami, pack_val.c_val);
                pub->data[i]->val = (SOS_val) pub_str_ptr;
                break;

            case SOS_INT :
            case SOS_LONG :
            case SOS_DOUBLE :

                /* Test if the values are equal, otherwise fall through to the non-string assignment. */

                if (pack_type == SOS_INT && (pub->data[i]->val.i_val == pack_val.i_val)) {
                    dlog(5, "[%s]: Packed value is identical to existing value.  Updating timestamp and skipping.\n", whoami);
                    SOS_TIME(pub->data[i]->pack_ts);
                    return i;
                } else if (pack_type == SOS_LONG && (pub->data[i]->val.l_val == pack_val.l_val)) {
                    dlog(5, "[%s]: Packed value is identical to existing value.  Updating timestamp and skipping.\n", whoami);
                    SOS_TIME(pub->data[i]->pack_ts);
                    return i;
                } else if (pack_type == SOS_DOUBLE) {
                    /*
                     *  TODO:{ PACK } Insert proper floating-point comparator here.
                     */
                }

            default :
                dlog(6, "[%s]: assigning a new value.   \"%ld\"   (updating)\n", whoami, pack_val.l_val);
                pub->data[i]->val = pack_val;
                break;
            }
            pub->data[i]->type = pack_type;
            pub->data[i]->dirty = 1;
            SOS_TIME(pub->data[i]->pack_ts);

            dlog(6, "[%s]: (%s) successfully updated [%s] at position %d.\n", whoami, name, pub->data[i]->name, i);
            dlog(6, "[%s]: --------------------------------------------------------------\n", whoami);

            return i;
        }
    }

    dlog(6, "[%s]: (%s) name does not exist in schema yet, attempting to add it.\n", whoami, name);

    //name does not exist in the existing schema, add it:
    pub->announced = 0;
    new_str_len = strlen(name);
    new_name = malloc(new_str_len + 1);
    memset(new_name, '\0', (new_str_len + 1));
    strncpy(new_name, name, new_str_len);
    new_name[new_str_len] = '\0';

    if (pub->elem_count < pub->elem_max) {
        i = pub->elem_count;
        pub->elem_count++;

        dlog(6, "[%s]: (%s) inserting into position %d\n", whoami, name, i);

        switch (pack_type) {

        case SOS_STRING :
            new_str_ptr = pack_val.c_val;
            new_str_len = strlen(new_str_ptr);
            pub_str_ptr = malloc(new_str_len + 1);
            memset(pub_str_ptr, '\0', (new_str_len + 1));
            strncpy(pub_str_ptr, new_str_ptr, new_str_len);
            pub_str_ptr[new_str_len + 1] = '\0';
            dlog(6, "[%s]: assigning a new string.   \"%s\"   (insert)\n", whoami, pub_str_ptr);
            pub->data[i]->val = (SOS_val) pub_str_ptr;
            break;

        default :
            dlog(6, "[%s]: assigning a new value.   \"%ld\"   (insert)\n", whoami, pack_val.l_val);
            pub->data[i]->val = pack_val;
            break;
        }

        dlog(6, "[%s]: (%s) data copied in successfully.\n", whoami, name);

        pub->data[i]->id = i;
        pub->data[i]->name = new_name;
        pub->data[i]->type = pack_type;
        pub->data[i]->dirty = 1;
        SOS_TIME(pub->data[i]->pack_ts);

        dlog(6, "[%s]: (%s) successfully inserted [%s] at position %d. (DONE)\n", whoami, name, pub->data[i]->name, i);
        dlog(6, "[%s]: --------------------------------------------------------------\n", whoami);

        return i;

    } else {

        dlog(6, "[%s]: (%s) the data object is full, expanding it.  (pub->elem_max=%d)\n", whoami, name, pub->elem_max);

        SOS_expand_data(pub);
        pub->elem_count++;

        dlog(6, "[%s]: (%s) data object has been expanded successfully.  (pub->elem_max=%d)\n", whoami, name, pub->elem_max);

        //[step 2/2]: insert the new name
        switch (pack_type) {

        case SOS_STRING :
            new_str_ptr = pack_val.c_val;
            new_str_len = strlen(new_str_ptr);
            pub_str_ptr = malloc(new_str_len + 1);
            strncpy(pub_str_ptr, new_str_ptr, new_str_len);
            pub_str_ptr[new_str_len + 1] = '\0';
            dlog(6, "[%s]: assigning a new string.   \"%s\"   (expanded)\n", whoami, pack_val.c_val);
            pub->data[i]->val = (SOS_val) pub_str_ptr;
            break;

        default :
            dlog(6, "[%s]: assigning a new value.   \"%ld\"   (expanded)\n", whoami, pack_val.l_val);
            pub->data[i]->val = pack_val;
            break;

        }

        dlog(6, "[%s]: ALMOST DONE....\n", whoami);

        pub->data[i]->id = i;
        pub->data[i]->name = new_name;
        pub->data[i]->type = pack_type;
        pub->data[i]->dirty = 1;
        SOS_TIME(pub->data[i]->pack_ts);

        dlog(6, "[%s]: (%s) successfully inserted [%s] at position %d. (DONE)\n", whoami, name, pub->data[i]->name, i);
        dlog(6, "[%s]: --------------------------------------------------------------\n", whoami);

        return i;
    }

    //shouln't ever get here.
    return -1;
}

void SOS_repack( SOS_pub_handle *pub, int index, SOS_val pack_val ) {
    SOS_data *data;
    int len;

    data = pub->data[index];

    switch (data->type) {

    case SOS_STRING:
        /* Determine if the string has changed, and if so, free/malloc space for new one. */
        if (strcmp(data->val.c_val, pack_val.c_val) == 0) {
            /* Update the time stamp only. */
            SOS_TIME( data->pack_ts );
        } else {
            /* Novel string is being packed, free the old one, allocate a copy. */
            free(data->val.c_val);
            len = strlen(pack_val.c_val);
            data->val.c_val = (char *) malloc(sizeof(char) * (len + 1));
            memset(data->val.c_val, '\0', len);
            memcpy(data->val.c_val, pack_val.c_val, len);
            SOS_TIME( data->pack_ts );
        }
        break;

    case SOS_INT:
    case SOS_LONG:
    case SOS_DOUBLE:
        data->val = pack_val;
        SOS_TIME(data->pack_ts);
        break;
    }

    data->dirty = 1;

    return;
}

SOS_val SOS_get_val(SOS_pub_handle *pub, char *name) {
    int i;

    for(i = 0; i < pub->elem_count; i++) {
        if (strcmp(name, pub->data[i]->name) == 0) return pub->data[i]->val;
    }

    return (SOS_val) 0;

}

SOS_pub_handle* SOS_new_pub(char *title) {
    int i;
    SOS_pub_handle *new_pub;

    new_pub = malloc(sizeof(SOS_pub_handle));
    memset(new_pub, '\0', sizeof(SOS_pub_handle));

    new_pub->origin_puid   = -1;     /* <-- This gets set during initial announce... */
    new_pub->announced     = 1;      /* <-- pub->announced will get zero'ed during pack()'ing */
    new_pub->origin_role   = SOS_ROLE;
    new_pub->origin_rank   = SOS_RANK;
    new_pub->origin_thread = -1;
    new_pub->origin_prog   = SOS_ARGV[0];
    new_pub->target_list   = SOS_DEFAULT_PUB_TARGET_LIST;
    new_pub->pragma_tag    = 0;
    new_pub->elem_count    = 0;
    new_pub->elem_max = SOS_DEFAULT_ELEM_MAX;
    new_pub->data = malloc(sizeof(SOS_data *) * SOS_DEFAULT_ELEM_MAX);

    new_pub->title = (char *) malloc(strlen(title) + 1);
    memset(new_pub->title, '\0', (strlen(title) + 1));
    strcpy(new_pub->title, title);

    for (i = 0; i < SOS_DEFAULT_ELEM_MAX; i++) {
        new_pub->data[i] = malloc(sizeof(SOS_data));
        memset(new_pub->data[i], '\0', sizeof(SOS_data));
    }
    return new_pub;
}


SOS_sub_handle* SOS_new_sub() {
    SOS_sub_handle *new_sub;

    new_sub = malloc(sizeof(SOS_sub_handle));
    memset(new_sub, '\0', sizeof(SOS_sub_handle));
    new_sub->active = 1;
    new_sub->pub = SOS_new_pub("---empty---");

    return new_sub;
}

void SOS_free_pub(SOS_pub_handle *pub) {

    /* TODO:{ FREE_PUB, CHAD } */
  
    return;
}

void SOS_free_sub(SOS_sub_handle *sub) {

    /* TODO:{ FREE_SUB, CHAD } */

    return;
}



void SOS_display_pub(SOS_pub_handle *pub, FILE *output_to) {
    if (SOS_DEBUG != 0 ) {
        int i;
        int rank;

        /* TODO:{ DISPLAY_PUB, CHAD }
         *
         * This needs to get cleaned up and restored to a the useful CSV/TSV that it was.
         */
  
        const char *SOS_TYPE_LOOKUP[4] = {"SOS_INT", "SOS_LONG", "SOS_DOUBLE", "SOS_STRING"};

        fprintf(output_to, "\n/---------------------------------------------------------------\\\n");
        fprintf(output_to, "|  %15s(%4d) : origin   %19s : title |\n", pub->origin_prog, pub->origin_rank, pub->title);
        fprintf(output_to, "|  %3d of %3d elements used.                                    |\n", pub->elem_count, pub->elem_max);
        fprintf(output_to, "|---------------------------------------------------------------|\n");
        fprintf(output_to, "|       index,          id,        type,                   name | = <value>\n");
        fprintf(output_to, "|---------------------------------------------------------------|\n");
        for (i = 0; i < pub->elem_count; i++) {
            fprintf(output_to, "| %11d,%12d,%12s,", i, pub->data[i]->id, SOS_TYPE_LOOKUP[pub->data[i]->type]);
            fprintf(output_to, " %c %20s | = ", ((pub->data[i]->dirty) ? '*' : ' '), pub->data[i]->name);
            switch (pub->data[i]->type) {
            case SOS_INT : fprintf(output_to, "%d", pub->data[i]->val.i_val); break;
            case SOS_LONG : fprintf(output_to, "%ld", pub->data[i]->val.l_val); break;
            case SOS_DOUBLE : fprintf(output_to, "%lf", pub->data[i]->val.d_val); break;
            case SOS_STRING : fprintf(output_to, "\"%s\"", pub->data[i]->val.c_val); break; }
            fprintf(output_to, "\n");
        }
        fprintf(output_to, "\\---------------------------------------------------------------/\n\n");
    }
    return;
}




/* **************************************** */
/* [pub]                                    */
/* **************************************** */


void SOS_announce( SOS_pub_handle *pub ) {
    SOS_announce_to(pub, SOS_ICOMM_MONITOR, SOS_PAIRED_MONITOR);
    return;
}


void SOS_publish( SOS_pub_handle *pub ) {
    SOS_publish_to(pub, SOS_ICOMM_MONITOR, SOS_PAIRED_MONITOR);
    return;
}

void SOS_announce_to( SOS_pub_handle *pub, MPI_Comm target_comm, int target_rank ) {
    int i, n_byte, name_len;
    int composite_tag;
    int ptr;
    int composite_len;
    char *c_ptr;
    char *buffer;
    //Used for packing, to increase readability:
    int origin_prog_len;
    int title_len;
    int pack_type;
    int pack_id;
    int pack_name_len;
    char *pack_name;
    //misc
    char whoami[SOS_DEFAULT_STRING_LEN];

    pthread_mutex_lock(&SOS_MUTEX_ANNOUNCE_TO);

    buffer = SOS_ann_buffer;
  
    memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);
    switch (SOS_ROLE) {
    case SOS_APP       : sprintf(whoami, "app(%d).announce_to", SOS_RANK); break;
    case SOS_MONITOR   : sprintf(whoami, "monitor(%d).announce_to", SOS_RANK); break;
    case SOS_DB        : sprintf(whoami, "db(%d).announce_to", SOS_RANK); break;
    case SOS_POWSCHED  : sprintf(whoami, "powsched(%d).announce_to", SOS_RANK); break;
    case SOS_ANALYTICS : sprintf(whoami, "analytics(%d).announce_to", SOS_RANK); break;
    default: sprintf(whoami, "UNKOWN.announce_to"); break;
    }

    composite_len = 0;

    /* Assign a per-process unique ID to this pub, if appropriate. */

    if ((pub->origin_rank == SOS_RANK) && (pub->origin_role == SOS_ROLE)) {
        if (pub->origin_puid == -1) {
            /* This is an initial announcement. */
            pthread_mutex_lock(&SOS_MUTEX_SERIAL);
            pub->origin_puid = SOS_SERIAL_PUB_VAL++;
            pthread_mutex_unlock(&SOS_MUTEX_SERIAL);
        } else {
            /* This is a re-announcement, leave the pub UID alone. */
        }
    }

    /* Make space for fixed header data.
     *
     * NOTE: [ active ][  rank  ][  role  ] are ALWAYS the first
     *       3 (int) components at the head of every pub-related message!
     */

    composite_len += sizeof(int);                       //[origin_puid]
    composite_len += sizeof(int);                       //[origin_rank]
    composite_len += sizeof(int);                       //[origin_role]

    composite_len += sizeof(int);                       //[target_list]
    composite_len += sizeof(int);                       //[origin_prog_len]
    composite_len += 1 + strlen(pub->origin_prog);      //[origin_prog]
    composite_len += sizeof(int);                       //[pub_title_len]
    composite_len += 1 + strlen(pub->title);            //[pub_title]

    /* TODO:{ PRAGMA_TAG, ORIGIN_THREAD } */
  
    dlog(7, "[%s]: pub->origin_prog == \"%s\"\n", whoami, pub->origin_prog);

    /* Make space for each element... (structure only) */    

    for (i = 0; i < pub->elem_count; i++) {
        composite_len += sizeof(int);                     //[type]
        composite_len += sizeof(int);                     //[id]
        composite_len += sizeof(int);                     //[name_len]
        composite_len += 1 + strlen(pub->data[i]->name);  //[name]
    }

    //buffer = (char *) malloc(composite_len);
    //if (buffer == NULL) {
    //  dlog(0, "[%s]: FAILURE!! Could not allocate memory.  char *buffer == NULL; /* after malloc()! */", whoami);
    //  MPI_Abort();
    //  exit(1);
    //}

    memset(buffer, '\0', composite_len);

    dlog(6, "[%s]:  ---- {ANN} NEW\n", whoami);

    ptr = 0;

    /* Insert the header values... */

    dlog(6, "[%s]:   Loading the annoucement with header information...\n[%s]:\t"
         "pub->origin_puid == %d\n[%s]:\tpub->origin_rank == %d\n[%s]:\tpub->origin_role == %d\n[%s]:\t"
         "pub->origin_prog == \"%s\"\n[%s]:\tpub->title == \"%s\"\n",
         whoami, whoami, pub->origin_puid, whoami, pub->origin_rank, whoami, pub->origin_role,
         whoami, pub->origin_prog, whoami, pub->title);

    origin_prog_len = strlen(pub->origin_prog);
    title_len = strlen(pub->title);

    memcpy((buffer + ptr), &(pub->origin_puid), sizeof(int));   ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->origin_rank), sizeof(int));   ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->origin_role), sizeof(int));   ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->target_list), sizeof(int));   ptr += sizeof(int);
    memcpy((buffer + ptr), &origin_prog_len, sizeof(int));      ptr += sizeof(int);
    memcpy((buffer + ptr), pub->origin_prog, origin_prog_len);  ptr += (1 + origin_prog_len);
    memcpy((buffer + ptr), &title_len, sizeof(int));            ptr += sizeof(int);
    memcpy((buffer + ptr), pub->title, title_len);              ptr += (1 + title_len);

    /* Insert the per-element values... */
  
    for (i = 0; i < pub->elem_count; i++) {
        pack_type     = (int) pub->data[i]->type;
        pack_id       = (int) pub->data[i]->id;
        pack_name_len = strlen(pub->data[i]->name);
        dlog(6, "[%s]:    [%d]:  type: %d   \tid: %d   \tname_len: %d   \tname:\"%s\"\n", whoami, ptr, pack_type, pack_id, pack_name_len, pub->data[i]->name);   
        memcpy((buffer + ptr), &pack_type, sizeof(int));             ptr += sizeof(int);
        memcpy((buffer + ptr), &pack_id, sizeof(int));               ptr += sizeof(int);
        memcpy((buffer + ptr), &pack_name_len, sizeof(int));         ptr += sizeof(int);
        memcpy((buffer + ptr), pub->data[i]->name, pack_name_len);  ptr += (1 + pack_name_len);
    }

    dlog(6, "[%s]:  ---- {ANN} DONE   LEN = %d  PTR = %d\n", whoami, composite_len, ptr);

    PMPI_Send(buffer, composite_len, MPI_CHAR, target_rank, SOS_MSG_ANNOUNCE, target_comm);

    //free(buffer);

    dlog(6, "[%s]:  ---- free(buffer); completed.\n", whoami);

    /* 
     * Only SOS_APP can automatically set this.
     *
     * Because SOS modules might need to announced this same pub to many different roles, this
     * is not the place to mark it as 'announced'.  Rather, this flag should be manually
     * set by the SOS module's creator to account for their particular contextual requirements.
     *
     * So long as it is set to 0, any call to SOS_publish() will recursively call
     * SOS_announce().  This can save your hide, or be terribly inneficient if you never
     * manually set this after a proper group announcement.
     *
     */

    /* TODO:{ ANNOUNCE, CHAD }
     *
     * Turn this into a bit-field w/per-role discrimination, and then it can be truly automatic.
     */
  
    if (SOS_ROLE == SOS_APP) pub->announced = 1;

    pthread_mutex_unlock(&SOS_MUTEX_ANNOUNCE_TO);
  
    return;
}


void SOS_publish_to( SOS_pub_handle *pub, MPI_Comm target_comm, int target_rank ) {
    int i, n_byte, name_len;
    int ptr;
    int composite_len;
    int composite_tag;
    char *c_ptr;
    char *buffer;
    int pack_id;
    int pack_len;
    double pack_pack_ts;
    double pack_send_ts;
    SOS_val pack_val;
    //misc
    int dbgcnt;
    int oldptrloc;
    char whoami[SOS_DEFAULT_STRING_LEN];

    pthread_mutex_lock(&SOS_MUTEX_PUBLISH_TO);

    buffer = SOS_pub_buffer;
  
    memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);
    switch (SOS_ROLE) {
    case SOS_APP       : sprintf(whoami, "app(%d).publish_to", SOS_RANK); break;
    case SOS_MONITOR   : sprintf(whoami, "monitor(%d).publish_to", SOS_RANK); break;
    case SOS_DB        : sprintf(whoami, "db(%d).publish_to", SOS_RANK); break;
    case SOS_POWSCHED  : sprintf(whoami, "powsched(%d).publish_to", SOS_RANK); break;
    case SOS_ANALYTICS : sprintf(whoami, "analytics(%d).publish_to", SOS_RANK); break;
    default: sprintf(whoami, "UNKOWN.publish_to"); break;
    }

    if (pub->announced == 0) {
        dlog(7, "[%s]: Attempting to publish an SOS_pub_handle announcing.\n[%s]: This is "    \
             "probably forgetting to manually set \"pub->announced = 1;\" within an sos_cloud" \
             " module.\n[%s]: Recursively calling SOS_announce_to() so we're safe,"            \
             "but this is not efficient!\n", whoami, whoami, whoami);

        /* TODO:{ ANNOUNCE, CHAD }
         *
         * Turn this into a bit field per role, like the target list, and then it is truly automatic.
         */

        SOS_announce_to( pub, target_comm, target_rank );
    }

    if ((pub->origin_role == SOS_ROLE) && (pub->origin_rank == SOS_RANK)) {
        SOS_TIME( pack_send_ts );
        dlog(5, "[%s]: pack_send_ts == %lf\n", whoami, pack_send_ts);
    }

    /* Determine the required buffer length for all dirty values. */

    composite_len = 0;

    composite_len += sizeof(int);        // [origin_puid]     /* Standard pub-message three element header. */
    composite_len += sizeof(int);        // [origin_rank]
    composite_len += sizeof(int);        // [origin_role]

    for (i = 0; i < pub->elem_count; i++) {
        if (pub->data[i]->dirty == 0) continue;

        if ((pub->origin_role == SOS_ROLE) && (pub->origin_rank == SOS_RANK)) {
            /* Update the sent_ts value only if we're the original sender. */
            pub->data[i]->send_ts = pack_send_ts;
        }

        composite_len += sizeof(int);      // [   id   ]
        composite_len += sizeof(double);   // [pack_ts ]
        composite_len += sizeof(double);   // [ send_ts]
        composite_len += sizeof(int);      // [ length ]
        switch ( pub->data[i]->type ) {    // [  data  ]
        case SOS_INT :    composite_len += sizeof(int); break;
        case SOS_LONG :   composite_len += sizeof(long); break;
        case SOS_DOUBLE : composite_len += sizeof(double); break;
        case SOS_STRING : composite_len += (1 + strlen(pub->data[i]->val.c_val)); break; }
    }

    /* Fill the buffer with the dirty values. */
 
    //buffer = (char *) malloc(composite_len);

    //if (buffer == NULL) {
    //  dlog(0, "[%s]: FAILURE!! Could not allocate memory.  char *buffer == NULL; /* after malloc()! */", whoami);
    //  MPI_Abort();
    //  exit(1);
    // }

  
  
    memset(buffer, '\0', (composite_len));

    ptr = 0;

    memcpy((buffer + ptr), &(pub->origin_puid), sizeof(int));    ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->origin_rank), sizeof(int));    ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->origin_role), sizeof(int));    ptr += sizeof(int);

    for (i = 0; i < pub->elem_count; i++) {
        if (pub->data[i]->dirty == 0) continue;

        pack_id      = pub->data[i]->id;
        pack_pack_ts = pub->data[i]->pack_ts;
        pack_send_ts = pub->data[i]->send_ts;
        pack_val     = pub->data[i]->val;
        switch ( pub->data[i]->type ) {
        case SOS_INT :    pack_len = sizeof(int);    break;
        case SOS_LONG :   pack_len = sizeof(long);   break;
        case SOS_DOUBLE : pack_len = sizeof(double); break;
        case SOS_STRING : pack_len = (int)(strlen( pub->data[i]->val.c_val)); break; }

        dlog(6, "[%s]:    ----\n", whoami);
        dlog(6, "[%s]:      [%d] + (int):%d    \t\"%d\"\n", whoami, ptr, (int)sizeof(int), pack_id);
        dlog(6, "[%s]:      [%d] + (int):%d    \t\"%d\"\n", whoami, (int)(ptr + sizeof(int)), (int)sizeof(int), pack_len);
        switch ( pub->data[i]->type ) {
        case SOS_INT :    dlog(6, "[%s]:      [%d] + (int):%d    \t\"%d\"\n",  whoami, (int)(ptr + (sizeof(int) * 2)), pack_len, pub->data[i]->val.i_val); break;
        case SOS_LONG :   dlog(6, "[%s]:      [%d] + (long):%d   \t\"%ld\"\n", whoami, (int)(ptr + (sizeof(int) * 2)), pack_len, pub->data[i]->val.l_val); break;
        case SOS_DOUBLE : dlog(6, "[%s]:      [%d] + (double):%d \t\"%lf\"\n", whoami, (int)(ptr + (sizeof(int) * 2)), pack_len, pub->data[i]->val.d_val); break;
        case SOS_STRING : dlog(6, "[%s]:      [%d] + (string):%d \t\"%s\"\n",  whoami, (int)(ptr + (sizeof(int) * 2)), pack_len, pub->data[i]->val.c_val); break; }

        memcpy((buffer + ptr), &pack_id, sizeof(int));         ptr += sizeof(int);
        memcpy((buffer + ptr), &pack_pack_ts, sizeof(double)); ptr += sizeof(double);
        memcpy((buffer + ptr), &pack_send_ts, sizeof(double)); ptr += sizeof(double);
        memcpy((buffer + ptr), &pack_len, sizeof(int));        ptr += sizeof(int);

        /* Above we write in the value of the pack_len, which is as long as an int.
         * Below we write in the data element's value, that has length of pack_len's value. */

        switch (pub->data[i]->type) {
        case SOS_INT :    memcpy((buffer + ptr), &pack_val.i_val, pack_len);   ptr += pack_len;       break;
        case SOS_LONG :   memcpy((buffer + ptr), &pack_val.l_val, pack_len);   ptr += pack_len;       break;
        case SOS_DOUBLE : memcpy((buffer + ptr), &pack_val.d_val, pack_len);   ptr += pack_len;       break;
        case SOS_STRING : memcpy((buffer + ptr), pack_val.c_val,  pack_len);   ptr += (1 + pack_len); break; }

        /*
         *  NOTE: Only SOS_APP roles can safely clear the dirty flag on a send.
         *        Within the sos_cloud roles, there are a variety of valid communication
         *        patterns, and this requires that people clear dirty flags manually.
         *
         *        Dirty doesn't necessarily mean "hasn't been sent to ROLE____, for some
         *        modules, it might mean it has not been comitted to a SQL store, or
         *        perhaps fed into some automatic analysis engine.
         */

        if (SOS_ROLE == SOS_APP) pub->data[i]->dirty = 0;

    }

    PMPI_Send(buffer, composite_len, MPI_CHAR, target_rank, SOS_MSG_PUBLISH, target_comm);

    //free(buffer);

    pthread_mutex_unlock(&SOS_MUTEX_PUBLISH_TO);
  
    dlog(6, "[%s]:  free(buffer); completed\n", whoami);
    return;
}


void SOS_unannounce( SOS_pub_handle *pub ) {

    /* TODO:{ UNANNOUNCE, CHAD } */

    return;
}




/* **************************************** */
/* [sub]                                    */
/* **************************************** */


SOS_sub_handle* SOS_subscribe( SOS_role source_role, int source_rank, char *pub_title, int refresh_delay ) {
    int i, msg_len;
    char *msg;
    MPI_Comm source_comm;
    MPI_Status status;
    SOS_sub_handle *new_sub;

    /* Select the correct MPI communicator. */

    if (SOS_ROLE == source_role) {
        source_comm = SOS_COMM_LOCAL;
    } else {
        switch (source_role) {
        case SOS_APP       : source_comm = SOS_ICOMM_APP; break;
        case SOS_MONITOR   : source_comm = SOS_ICOMM_MONITOR; break;
        case SOS_DB        : source_comm = SOS_ICOMM_DB; break;
        case SOS_POWSCHED  : source_comm = SOS_ICOMM_POWSCHED; break;
        case SOS_ANALYTICS : source_comm = SOS_ICOMM_ANALYTICS; break;
        case SOS_SURPLUS :
        default: return NULL; break;
        }
    }

    new_sub = SOS_new_sub();
    new_sub->source_role = source_role;
    new_sub->source_rank = source_rank;
    new_sub->refresh_delay = refresh_delay;

    /* Send message to target asking for an announcement. */

    PMPI_Send(pub_title, (strlen(pub_title) + 1), MPI_CHAR, source_rank, SOS_MSG_SUBSCRIBE, source_comm);

    /* Process announcement into the subscription. */

    PMPI_Probe(source_rank, SOS_MSG_ANNOUNCE, source_comm, &status);
    PMPI_Get_count(&status, MPI_CHAR, &msg_len);
    msg = (char *)malloc(sizeof(char) * msg_len);
    memset(msg, '\0', (sizeof(char) * msg_len));
    PMPI_Recv(msg, msg_len, MPI_CHAR, source_rank, SOS_MSG_ANNOUNCE, source_comm, &status);
    SOS_apply_announce(new_sub->pub, msg, msg_len);
    free(msg);

    /* Process the initial publication into the subscription. */

    PMPI_Probe(source_rank, SOS_MSG_PUBLISH, source_comm, &status);
    PMPI_Get_count(&status, MPI_CHAR, &msg_len);
    msg = (char *)malloc(sizeof(char) * msg_len);
    memset(msg, '\0', (sizeof(char) * msg_len));
    PMPI_Recv(msg, msg_len, MPI_CHAR, source_rank, SOS_MSG_PUBLISH, source_comm, &status);
    SOS_apply_publish(new_sub->pub, msg, msg_len);
    free(msg);

    /* Create thread and assign it to this sub handle */

    pthread_create(&(new_sub->thread_handle), NULL, (void*) SOS_refresh_sub, (void*) new_sub);

    /* Look for a place in the SOS_SUB_HANDLE[] to stick it, or grow it. */

    for (i = 0; i < SOS_DEFAULT_SUB_MAX; i++) {
        if (SOS_SUB_LIST[i] == NULL) {
            new_sub->suid = i;
            SOS_SUB_LIST[i] = new_sub;
            return new_sub;
        }
    }

    /* Too many subscriptions... */

    new_sub->suid = -1;
    SOS_warn_user(0, "You have maxed out the number of subscriptions!\n");

    return new_sub;
}


void* SOS_refresh_sub( void *arg ) {
    SOS_sub_handle *sub = (SOS_sub_handle*) arg;
    MPI_Comm source_comm;
    MPI_Status status;
    char *msg;
    int msg_len;

    if (sub->source_role == SOS_ROLE) {
        source_comm = SOS_COMM_LOCAL;
    } else {
        switch(sub->source_role) {
        case SOS_APP       : source_comm = SOS_ICOMM_APP;       break;
        case SOS_MONITOR   : source_comm = SOS_ICOMM_MONITOR;   break;
        case SOS_DB        : source_comm = SOS_ICOMM_DB;        break;
        case SOS_POWSCHED  : source_comm = SOS_ICOMM_POWSCHED;  break;
        case SOS_ANALYTICS : source_comm = SOS_ICOMM_ANALYTICS; break;
        case SOS_SURPLUS   : return NULL; /* Charlie don't surf. */
        default: break;
        }
    }

    /*
     * TODO:{ CHAD }
     *
     * Right now there is not a legit pub<--->sub model in place.
     * Because of that, we're using pub titles as a unique key.
     * If multiple pubs are submitted with the same name, searches
     * will (probably) only return the first pub, or if they
     * do not 'break' out, they will send ALL instances of that
     * pub name.  Assuming the key names are all the same, the
     * receiver will get a huge message and during the processing
     * of it will be overwriting values so the pub handle will wind
     * up containing content from some other rank.  Boo.  Obviously
     * this is something to be fixed.
     */
  
    while (sub->active == 1) {
        sleep(sub->refresh_delay);

        /* Notify the target that we want a refresh. */

        PMPI_Send(sub->pub->title, (strlen(sub->pub->title) + 1), MPI_CHAR, sub->source_rank, SOS_MSG_REFRESH, source_comm);

        /* Process the initial publication into the subscription. */

        PMPI_Probe(sub->source_rank, SOS_MSG_PUBLISH, source_comm, &status);
        PMPI_Get_count(&status, MPI_CHAR, &msg_len);
        msg = (char *)malloc(sizeof(char) * msg_len);
        memset(msg, '\0', (sizeof(char) * msg_len));
        PMPI_Recv(msg, msg_len, MPI_CHAR, sub->source_rank, SOS_MSG_PUBLISH, source_comm, &status);
        SOS_apply_publish(sub->pub, msg, msg_len);
        free(msg);
    }
    return NULL;
}


void SOS_unsubscribe(SOS_sub_handle *sub) {
    sub->active = 0;
    pthread_join(sub->thread_handle, NULL);
    if (sub->suid != -1) SOS_SUB_LIST[sub->suid] = NULL;
    SOS_free_sub(sub);
    return;
}
