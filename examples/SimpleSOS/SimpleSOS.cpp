// file: SimpleSOS.cpp
// desc: Super-simple skeleton of a C++ program that calls SOS for analysis
//       and provides a mechanism to pass results back into your C++ class.
//       This is not a full wrapping of the SOS API, so to use this, make
//       a copy of it and modify it to suit your needs, per the provided
//       instructions in comments below.  Search for the string "USERTODO"
//       for all code locations you need to modify.
//       ...
//       Make a COPY of this code that you modify for your project.
//

#include <iostream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <typeinfo>

#include "include/SimpleSOS.hpp"
//
#include "sos.h"
#include "sos_types.h"
//
//
//  USERTODO: Include a header file for YOUR analysis class here:

//
//
//


extern "C" void 
handleFeedback(void *sos_context, int msg_type, int msg_size, void *data)
{
    SOS_msg_header header;
    int   offset = 0;
    char *tree;
    struct YourCPPClass;

    switch (msg_type) {
        //
        case SOS_FEEDBACK_TYPE_QUERY:
        case SOS_FEEDBACK_TYPE_CACHE:
        case SOS_FEEDBACK_TYPE_PAYLOAD:

    break;
    }
    void *your_class_ref =
        SOS_reference_get((SOS_runtime *)sos_context, "CPP_HANDLER_CLASS_PTR");
    
    call_ResultsProcessor((struct YourCPPClass *)your_class_ref, int data_size, (char *) data);

    return;
}

extern "C" void
call_ResultsProcessor(void *your_class_ref, int dataType, int dataSize, const char *data)
{

    SimpleSOS::FeedBackType feedbackType;
    switch (dataType) {
        SOS_FEEDBACK_TYPE_QUERY: feedbackType = FeedbackType.QUERY; break;
        SOS_FEEDBACK_TYPE_CACHE: feedbackType = FeedbackType.CACHE; break;
        SOS_FEEDBACK_TYPE_PAYLOAD: feedbackType = FeedbackType.TRIGGER; break;
    }

    //USERTODO:
    // Here is what this should look like:
    //
    //   YourClass *yourclass = (YourClass *) your_class_ref;
    //   yourclass->handleResults(feedbackType, dataSize, data);
    //   return;
    //
    //       (int) dataSize    : How many bytes of data we're sending.
    //       (const char) data : The data
    //
    // NOTE: After leaving this function, accessing the contents of *data
    //       may result in undefined behavior. This is your one shot to use it.
    //       You are encouraged to extract the values or make a copy of
    //       the buffer before returning from your handleResults(...) method.

    return;
}


SimpleSOS::SimpleSOS()
{
    ynConnectedToSOS = false;
    sos_ptr = NULL;

    return;
}

//TODO: Implement later...
//std::string
//SimpleSOS::getSOSDProbe(ProbeFormat fmt)
//{
//    std::string result;
//
//    //
//
//    return result;
//}

SimpleSOS::~SimpleSOS()
{
    if (ynConnectedToSOS) {
        this->disconnect();
    }
    return;
}

bool connect(void *your_handler_class_ptr)
{
    SOS_init(&sos_ptr, SOS_ROLE_CLIENT,
            SOS_RECEIVES_DIRECT_MESSAGES, handleFeedback);
    
    if (sos_ptr == NULL) {
        if (SIMPLESOS_VERBOSE_ERRORS) {
            fprintf(stderr, "SimpleSOS: Unable to communicate with the SOS daemon.\n");
            fflush(stderr);
        }
        return;
    }

    SOS_reference_set((SOS_runtime *)sos_ptr, "CPP_HANDLER_CLASS_PTR", (void *) your_handler_class_ptr); 
    SOS_sense_register((SOS_runtime *)sos_ptr, "CPP_HANDLER_CLASS_PTR"); 

    ynConnectedToSOS = true; 
    return ynConnectedToSOS;
}

void disconnect(void)
{
    if (sos_ptr != NULL) {
        SOS_finalize((SOS_runtime *) sos_ptr);
    }
    ynConnectedToSOS = false;
    sos_ptr = NULL;
    return;
}

bool SimpleSOS::isConnected(void)
{
    return ynConnectedToSOS;
}



