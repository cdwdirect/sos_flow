#ifndef SIMPLE_SOS_HPP
#define SIMPLE_SOS_HPP

#include <cstdint>
#include <string>
#include <mutex>
#include <map>
#include <type_traits>

extern "C" {
    // SOS will deliver triggers and query results here:
    void  
    handleFeedback(void *sos_context,
                   int msg_type,
                   int msg_size,
                   void *data);

    // Used to call your C++ methods from the C event handlercode:
    void call_ResultsProcessor(void *your_class_ref,
            int data_size,
            const char *data);
}

class SimpleSOS
{
    public:
        SimpleSOS();
        ~SimpleSOS();
        // disallow copy constructor 
        SimpleSOS(const Apollo&) = delete; 
        SimpleSOS& operator=(const SimpleSOS&) = delete;
        //
        enum class FeedbackType : int {
            QUERY,
            CACHE,
            TRIGGER
        };
        //
        //
        bool connect(void *your_handler_class_ptr);
        bool isConnected(void);
        void disconnect(void);
        //
        //TODO: Implement later...
        //enum class ProbeFormat : int {
        //    JSON,
        //    CSV,
        //    CSV_W_HEADER
        //};
        //std::string getSOSDProbe(ProbeFormat fmt);
        //
    private:
        //
        void *sos_ptr;              // type in .cpp: (SOS_runtime *)
        bool  ynConnectedToSOS;
        //
        void *your_handler_class_ptr;

}; //end: SimpleSOS (class)

    

// Useful for converting enum class members into ints:
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) {
    return static_cast<typename std::underlying_type<E>::type>(e);
}



#endif
