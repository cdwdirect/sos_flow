#pragma once

#include <vector>
#include <fstream>

class CPUStat {
public:
  char name[32];
  long long user;
  long long nice;
  long long system;
  long long idle;
  long long iowait;
  long long irq;
  long long softirq;
  long long steal;
  long long guest;
};

typedef std::vector<CPUStat*> CPUs;

class ProcData {
public:
  CPUs cpus;
  long long ctxt;
  long long btime;
  long processes;
  long procs_running;
  long procs_blocked;
  long power;
  long energy;
  long freshness;
  long generation;
  long power_cap;
  long startup;
  long version;
  long long package0;
  long long dram;
  ~ProcData(void);
  ProcData* diff(const ProcData& rhs);
  void sample_values();
};

#if defined(APEX_HAVE_CRAY_POWER)
int read_cray_file(const char * filename) {
  int tmpint; 
  std::string tmpstr; 
  std::ifstream infile(filename); 
  if (infile.good()) { 
    while (infile >> tmpint >> tmpstr) { 
      return tmpint; /* return the first value encountered. */ \
    } 
  } 
  return 0; 
}

#define read_energy() read_cray_file("/sys/cray/pm_counters/energy")
#define read_freshness() read_cray_file("/sys/cray/pm_counters/freshness")
#define read_generation() read_cray_file("/sys/cray/pm_counters/generation")
#define read_power() read_cray_file("/sys/cray/pm_counters/power")
#define read_power_cap() read_cray_file("/sys/cray/pm_counters/power_cap")
#define read_startup() read_cray_file("/sys/cray/pm_counters/startup")
#define read_version() read_cray_file("/sys/cray/pm_counters/version")

#endif

