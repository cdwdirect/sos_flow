#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include "sos.h"
#include "sosa.h"

int id = -1;
SOS_runtime *mySOS = NULL;

void
SOS_msg_handler(
        void *sos_context,
        int payload_type,
        int payload_size,
        void *payload_data)
{
    SOSA_results *results = NULL;

    switch (payload_type) {

    //-------------------------
    case SOS_FEEDBACK_TYPE_PAYLOAD:
        printf("demo_app %d : Received %d-byte payload --> \"%s\"\n",
        		id,
                payload_size,
                (unsigned char *) payload_data);
        fflush(stdout);
        break;
    //-------------------------
    default:
        break;

    }

    return;
}

int main(int argc, char **argv){

	printf("PATH : %s\n", getenv("PATH"));
	char *sos_handle;
	sos_handle = getenv("SOS_HANDLE");
	if(sos_handle == NULL)
		printf("== test-trigger.c: Need to declare SOS_HANDLE\n");

	if(getenv("SOS_CMD_PORT") == NULL)
		printf("== test-trigger.c: Need to declare SOS_CMD_PORT\n");

	id = atoi(getenv("SOS_CMD_PORT"));


	printf("----APP START----\n");
	SOS_init(&mySOS, SOS_ROLE_CLIENT, SOS_RECEIVES_DIRECT_MESSAGES, SOS_msg_handler);

	if (mySOS == NULL) {
	    printf("== test-trigger.c: Unable to connect to SOS daemon!\n");
	    fflush(stderr);
	    exit(EXIT_FAILURE);
	}
	//int value0 = 0;
	srand(time(NULL));
	int iter;

    SOS_sense_register(mySOS, sos_handle);

	while (1)
	{
		sleep(1000);
	}

	printf("----APP END----\n");
}
