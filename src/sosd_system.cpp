#include "sosd_system.h"
#include <sys/stat.h>
#include <string.h>

using namespace std;

static ProcData *oldData = nullptr;
static ProcData *newData = nullptr;
static ProcData *periodData = nullptr;

void make_the_buffer(void) {
    /* construct a buffer for putting the data into the queues
       and/or sending upstream to the aggregator */
}

void sample_value (const char * name, double value) {
    printf("%s: %f\n", name, value); fflush(stdout);
    /* pack the name, value into the buffer */
}

void flush_the_buffer(void) {
    /* put the buffer on the local DB queue
       and/or send it upstream to the aggregator */
    /* do whatever other cleanup is necessary */
}

inline bool file_exists (const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

bool parse_proc_self_status(void) {
  if (!file_exists("/proc/self/status")) { return false; }
  FILE *f = fopen("/proc/self/status", "r");
  const std::string prefix("Vm");
  if (f) {
    char line[4096] = {0};
    while ( fgets( line, 4096, f)) {
        string tmp(line);
        if (!tmp.compare(0,prefix.size(),prefix)) {
        // this requires c++11, so maybe hold off on that for now.
        /*
            const std::regex separator(":");
            std::sregex_token_iterator token(tmp.begin(), tmp.end(), separator, -1);
            std::sregex_token_iterator end;
            string name = *token++;
            if (token != end) {
                string value = *token;
                char* pEnd;
                double d1 = strtod (value.c_str(), &pEnd);
                string mname("status:" + name);
                if (pEnd) { sample_value(mname, d1); }
            }
            */
            char * str = strdup(tmp.c_str());
            char * name = strtok(str, ":");
            if (name == NULL) continue;
            char * value = strtok(NULL, ":");
            if (value == NULL) continue;
            double d1 = strtod (value, NULL);
            string mname("status:" + string(name));
            sample_value(mname.c_str(), d1);
        }
    }
    fclose(f);
  } else {
    return false;
  }
  return true;
}

bool parse_proc_meminfo() {
  FILE *f = fopen("/proc/meminfo", "r");
  if (f) {
    char line[4096] = {0};
    while ( fgets( line, 4096, f)) {
        string tmp(line);
        /*
        const std::regex separator(":");
        std::sregex_token_iterator token(tmp.begin(), tmp.end(), separator, -1);
        std::sregex_token_iterator end;
        string name = *token++;
        if (token != end) {
            string value = *token;
            char* pEnd;
            double d1 = strtod (value.c_str(), &pEnd);
            string mname("meminfo:" + name);
            if (pEnd) { sample_value(mname, d1); }
        } */
            char * str = strdup(tmp.c_str());
            char * name = strtok(str, ":");
            if (name == NULL) continue;
            char * value = strtok(NULL, ":");
            if (value == NULL) continue;
            double d1 = strtod (value, NULL);
            string mname("meminfo:" + string(name));
            sample_value(mname.c_str(), d1);
    }
    fclose(f);
  } else {
    return false;
  }
  return true;
}

ProcData* parse_proc_stat(void) {
  if (!file_exists("/proc/stat")) { return nullptr; }

  /*  Reading proc/stat as a file  */
  FILE * pFile;
  char line[128];
  char dummy[32];
  pFile = fopen ("/proc/stat","r");
  ProcData* procData = new ProcData();
  if (pFile == NULL) perror ("Error opening file");
  else {
    CPUStat* cpu_stat;
    while ( fgets( line, 128, pFile)) {
      if ( strncmp (line, "cpu", 3) == 0 ) { 
        cpu_stat = new CPUStat();
        /*  Note, this will only work on linux 2.6.24 through 3.5  */
        sscanf(line, "%s %lld %lld %lld %lld %lld %lld %lld %lld %lld\n", 
            cpu_stat->name, &cpu_stat->user, &cpu_stat->nice, 
            &cpu_stat->system, &cpu_stat->idle, 
            &cpu_stat->iowait, &cpu_stat->irq, &cpu_stat->softirq, 
            &cpu_stat->steal, &cpu_stat->guest);
        procData->cpus.push_back(cpu_stat);
      }
      else if ( strncmp (line, "ctxt", 4) == 0 ) { 
        sscanf(line, "%s %lld\n", dummy, &procData->ctxt);
      } else if ( strncmp (line, "btime", 5) == 0 ) { 
        sscanf(line, "%s %lld\n", dummy, &procData->btime);
      } else if ( strncmp (line, "processes", 9) == 0 ) { 
        sscanf(line, "%s %ld\n", dummy, &procData->processes);
      } else if ( strncmp (line, "procs_running", 13) == 0 ) { 
        sscanf(line, "%s %ld\n", dummy, &procData->procs_running);
      } else if ( strncmp (line, "procs_blocked", 13) == 0 ) { 
        sscanf(line, "%s %ld\n", dummy, &procData->procs_blocked);
      //} else if ( strncmp (line, "softirq", 5) == 0 ) { 
        // softirq 10953997190 0 1380880059 1495447920 1585783785 15525789 0 12 661586214 0 1519806115
        //sscanf(line, "%s %d\n", dummy, &procData->btime);
      }
      // don't waste time parsing anything but the mean
      break;
    }
  }
  fclose (pFile);
#if defined(APEX_HAVE_CRAY_POWER)
  procData->power = read_power();
  procData->power_cap = read_power_cap();
  procData->energy = read_energy();
  procData->freshness = read_freshness();
  procData->generation = read_generation();
#endif
#if defined(APEX_HAVE_POWERCAP_POWER)
  procData->package0 = read_package0();
  procData->dram = read_dram();
#endif
  return procData;
}

ProcData::~ProcData() {
  while (!cpus.empty()) {
    delete cpus.back();
    cpus.pop_back();
  }
}
ProcData* ProcData::diff(ProcData const& rhs) {
  ProcData* d = new ProcData();
  unsigned int i;
  CPUStat* cpu_stat;
  for (i = 0 ; i < cpus.size() ; i++) {
    cpu_stat = new CPUStat();
    strcpy(cpu_stat->name, cpus[i]->name);
    cpu_stat->user = cpus[i]->user - rhs.cpus[i]->user; 
    cpu_stat->nice = cpus[i]->nice - rhs.cpus[i]->nice;
    cpu_stat->system = cpus[i]->system - rhs.cpus[i]->system;
    cpu_stat->idle = cpus[i]->idle - rhs.cpus[i]->idle;
    cpu_stat->iowait = cpus[i]->iowait - rhs.cpus[i]->iowait;
    cpu_stat->irq = cpus[i]->irq - rhs.cpus[i]->irq;
    cpu_stat->softirq = cpus[i]->softirq - rhs.cpus[i]->softirq;
    cpu_stat->steal = cpus[i]->steal - rhs.cpus[i]->steal;
    cpu_stat->guest = cpus[i]->guest - rhs.cpus[i]->guest;
    d->cpus.push_back(cpu_stat);
  }
  d->ctxt = ctxt - rhs.ctxt;
  d->processes = processes - rhs.processes;
  d->procs_running = procs_running - rhs.procs_running;
  d->procs_blocked = procs_blocked - rhs.procs_blocked;
#if defined(APEX_HAVE_CRAY_POWER)
  d->power = power;
  d->power_cap = power_cap;
  d->energy = energy - rhs.energy;
  d->freshness = freshness;
  d->generation = generation;
#endif
#if defined(APEX_HAVE_POWERCAP_POWER)
  d->package0 = package0 - rhs.package0;
  d->dram = dram - rhs.dram;
#endif
  return d;
}

void ProcData::sample_values(void) {
  double total;
  CPUs::iterator iter = cpus.begin();
  CPUStat* cpu_stat=*iter;
  total = (double)(cpu_stat->user + cpu_stat->nice + cpu_stat->system + cpu_stat->idle + cpu_stat->iowait + cpu_stat->irq + cpu_stat->softirq + cpu_stat->steal + cpu_stat->guest);
  total = total * 0.01; // so we have a percentage in the final values
  sample_value("CPU User %",     ((double)(cpu_stat->user))    / total);
  sample_value("CPU Nice %",     ((double)(cpu_stat->nice))    / total);
  sample_value("CPU System %",   ((double)(cpu_stat->system))  / total);
  sample_value("CPU Idle %",     ((double)(cpu_stat->idle))    / total);
  sample_value("CPU I/O Wait %", ((double)(cpu_stat->iowait))  / total);
  sample_value("CPU IRQ %",      ((double)(cpu_stat->irq))     / total);
  sample_value("CPU soft IRQ %", ((double)(cpu_stat->softirq)) / total);
  sample_value("CPU Steal %",    ((double)(cpu_stat->steal))   / total);
  sample_value("CPU Guest %",    ((double)(cpu_stat->guest))   / total);
#if defined(APEX_HAVE_CRAY_POWER)
  sample_value("Power", power);
  sample_value("Power Cap", power_cap);
  sample_value("Energy", energy);
  sample_value("Freshness", freshness);
  sample_value("Generation", generation);
#endif
#if defined(APEX_HAVE_POWERCAP_POWER)
  sample_value("Package-0 Energy", package0);
  sample_value("DRAM Energy", dram);
#endif
}

/* Get initial readings */
extern "C" void SOSD_setup_system_data(void) {
  oldData = parse_proc_stat();
}

extern "C" void SOSD_read_system_data(void) {
  make_the_buffer();
  newData = parse_proc_stat();
  if (newData != nullptr && oldData != nullptr) {
    periodData = newData->diff(*oldData);
    periodData->sample_values();
    delete(oldData);
    delete(periodData);
    oldData = newData;
  }
  /* this is less useful, unless we grab the status of each pid? */
  //parse_proc_self_status();
  parse_proc_meminfo();
  flush_the_buffer();
}

