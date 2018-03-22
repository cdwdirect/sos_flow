
/*
 * extract_to_adios.c    (pull out tau values and forward them to adios output - ADIOS dependency)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <array>
#include <thread>
#include <map>
#include <set>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>

#include "sos.h"
#include "sosa.h"
#define _NOMPI
#include "adios.h"
#include "adios_types.h"
#include "adios_error.h"

#define USAGE "./extract_to_adios -d <initial_delay_seconds> -m <adios_method>\n\twhere:\nadios_method one of: POSIX, MPI, DIMES, DATASPACES, FLEXPATH\n"

class Options {
//private:
public:
    std::string method;
    int64_t metadata_group;
    int initial_delay_seconds;
    uint64_t expected_aggregators;
    uint64_t expected_publishers;
    std::string sos_meetup_path;
    std::vector<std::string> aggregator_hostnames;
    int sos_cmd_port;
    uint64_t max_frames;
    Options(int argc, char *argv[]) : 
        method("POSIX"), metadata_group(0UL), initial_delay_seconds(0),
        expected_aggregators(1), expected_publishers(4), 
        sos_meetup_path("."), sos_cmd_port(22500), max_frames(1) {
        if ( argc < 3 ) { 
            fprintf(stderr, "%s\n", USAGE); 
            exit(1); 
        }

        int elem, next_elem = 0;
        /* Process command-line arguments */
        for (elem = 1; elem < argc; ) {
            if ((next_elem = elem + 1) == argc) {
                fprintf(stderr, "%s\n", USAGE);
                exit(1);
            }

            if ( strcmp(argv[elem], "-d"  ) == 0) {
                initial_delay_seconds  = atoi(argv[next_elem]);
            } else if ( strcmp(argv[elem], "-m"  ) == 0) {
                const char * method_string = argv[next_elem];
                method = std::string(method_string);
            } else {
                fprintf(stderr, "ERROR: Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
                fprintf(stderr, "%s\n", USAGE);
                exit(1);
            }
            elem = next_elem + 1;
        }
    }
    std::string& get_method(void) { return method; }
    int get_delay(void) { return initial_delay_seconds; }
    uint64_t get_aggregators(void) { return expected_aggregators; }
    uint64_t get_publishers(void) { return expected_publishers; }
    std::string& get_meetup_path(void) { return sos_meetup_path; }
    void add_aggregator_hostname(std::string hostname) { aggregator_hostnames.push_back(hostname); }
    std::string& get_aggregator_hostname(int index) { 
        static std::string unknown("unknown");
        if (index >= aggregator_hostnames.size()) {
            return unknown;
        }
        return aggregator_hostnames[index]; 
    }
    int get_cmd_port(void) { return sos_cmd_port; }
    int get_max_frames(void) { return max_frames; }
};

class AdiosVariables {
public:
    int64_t group;
    int64_t node;
    int64_t thread;
    int64_t program_name;
    int64_t value_name;
    int64_t metadata_name;
    int64_t file;
};

SOS_runtime * initialize(int argc, char* argv[]) {
    SOS_runtime *SOS = NULL;
    SOS_init(&SOS, SOS_ROLE_ANALYTICS, SOS_RECEIVES_NO_FEEDBACK, NULL);
    if (SOS == NULL) {
        fprintf(stderr, "ERROR: Could not initialize SOS.\n");
        exit (EXIT_FAILURE);
    }
    srandom(SOS->my_guid);
    return(SOS);
}

void wait_for_aggregators(Options options) {
    for (int i = 0 ; i < options.get_aggregators() ; i++) {
        std::stringstream meetup;
        struct stat buffer;
        /* Todo: pad the zeroes correctly */
        meetup << options.get_meetup_path() << "/sosd.0000" << i << ".key";
        while(stat(meetup.str().c_str(), &buffer) != 0) {
            usleep(10);
        }
        std::string line1;
        std::string line2;
        std::ifstream keyfile(meetup.str());
        std::getline(keyfile, line1);
        std::getline(keyfile, line2);
        keyfile.close();
    }
}

/* one thread, executing a query and returning the results */
void one_thread_query(Options& options, SOS_runtime *SOS, const std::string& query, int aggregator, SOSA_results * results) {
    SOSA_results_init(SOS, &results);
    SOSA_results_wipe(results);
    SOSA_exec_query(SOS, (char*)query.c_str(), (char*)options.get_aggregator_hostname(aggregator).c_str(), options.get_cmd_port());
}

SOSA_results ** execute_query(Options& options, SOS_runtime *SOS, const std::string& query, int columns) {
    SOSA_results ** all_results = new SOSA_results*[options.get_aggregators()];
#if __THREADED__
    std::thread threads[options.get_aggregators()];
    for (int i = 0 ; i < options.get_aggregators() ; i++) {
        threads[i] = std::thread(one_thread_query, options, SOS, query, i, &(all_results[i]));
    }
    for (int i = 0 ; i < options.get_aggregators() ; i++) {
        threads[i].join();
    }
#else
    for (int i = 0 ; i < options.get_aggregators() ; i++) {
        SOSA_results_init(SOS, &(all_results[i]));
        SOSA_results_wipe((all_results[i]));
        SOSA_exec_query(SOS, (char*)query.c_str(), (char*)options.get_aggregator_hostname(i).c_str(), options.get_cmd_port());
    }
#endif
    return (all_results);
}

void cleanup_query(Options options, SOSA_results ** all_results) {
    for (int i = 0 ; i < options.get_aggregators() ; i++) {
        SOSA_results_destroy(all_results[i]);
    }
}

void wait_for_publishers(Options options, SOS_runtime *SOS, int frame) {
    std::stringstream query;
    query << "select count(distinct guid) from viewCombined where frame = " << frame << ";";
    SOSA_results ** all_results = execute_query(options, SOS, query.str(), 1);
    cleanup_query(options, all_results);
}

class metadata_key {
public:
    std::string prog_name;
    std::string node;
    std::string thread;
    std::string value_name;
    metadata_key(std::string p, std::string n, std::string t, std::string v) :
    prog_name(p), node(n), thread(t), value_name(v) {};
};

void write_frame_metadata(Options options, SOS_runtime *SOS, int frame, AdiosVariables& adios_vars) {
    std::stringstream query;
    query << "select prog_name, comm_rank, value_name, value from viewCombined ";
    query << "where val_name like 'TAU::%::Metadata::%' and frame = ";
    query << frame << " order by prog_name, comm_rank, val_name;";
    SOSA_results ** all_results = execute_query(options, SOS, query.str(), 4);

    /* write the metadata group:
     */
    // todo: reserve space for all combinations
    std::map<std::string,int> prog_names;
    std::map<std::string,int> nodes;
    std::map<std::string,int> threads;
    std::map<std::string,int> value_names;
    for (int a = 0 ; a < options.get_aggregators() ; a++) {
        for (int r = 0 ; r < all_results[a]->row_count ; r++) {
        }
    }

    /* Dimensions 
     * Metadata 
     *   Program_name
     *   Node
     *   Thread
     *   value_name
     */

    if (prog_names.size() > 0) {
        char dimstr[32];
        // Define an adios group
        adios_declare_group(&adios_vars.group, "TAU metadata", "", adios_stat_default);
        adios_select_method(adios_vars.group, options.method.c_str(), "", "");
        // Define the dimensions
        sprintf(dimstr, "%lu", prog_names.size());
        adios_vars.program_name = adios_define_var(adios_vars.group, "program_count", "", adios_integer, dimstr, dimstr, "0");
        adios_set_transform(adios_vars.program_name, "none");
        sprintf(dimstr, "%lu", nodes.size());
        adios_vars.node = adios_define_var(adios_vars.group, "node_count", "", adios_integer, dimstr, dimstr, "0");
        adios_set_transform(adios_vars.node, "none");
        sprintf(dimstr, "%lu", threads.size());
        adios_vars.thread = adios_define_var(adios_vars.group, "thread_count", "", adios_integer, dimstr, dimstr, "0");
        adios_set_transform(adios_vars.thread, "none");
        sprintf(dimstr, "%lu", value_names.size());
        adios_vars.metadata_name = adios_define_var(adios_vars.group, "metadata_name_count", "", adios_integer, dimstr, dimstr, "0");
        adios_set_transform(adios_vars.metadata_name, "none");
        // construct the arrays
        // write the data
    }

    cleanup_query(options, all_results);
}

int main(int argc, char *argv[]) {
    MPI_Comm comm = 0; // dummy MPI
    Options options(argc, argv);
    AdiosVariables adios_vars;
    //Dimensions dimensions();

    SOS_runtime * SOS = initialize(argc, argv);
    adios_init_noxml(comm);
    adios_set_max_buffer_size(1);
    sleep(options.get_delay());

    wait_for_aggregators(options);

    /* Dimensions 
     * TAU timers:
     *   Node
     *   Thread
     *   Timer Name
     *   Metric (count, inclusive_x, exclusive_x)
     *   Phase (SOS frame)
     */
    /* Dimensions 
     * TAU Counters:
     *   Node
     *   Thread
     *   Counter Name
     *   Metric (count, min, mean, max, sumsqr)
     *   Context?
     */

    for (int frame = 0 ; frame < options.get_max_frames() ; frame++) {
        wait_for_publishers(options, SOS, frame);
        adios_open(&adios_vars.file, "TAU", "TAU_data.bp", "w", comm);
        write_frame_metadata(options, SOS, frame, adios_vars);
        //write_frame_timers(options, SOS, frame);
        //write_frame_counters(options, SOS, frame);
    }

    adios_finalize(comm);
    SOS_finalize(SOS);

    return (EXIT_SUCCESS);
}
