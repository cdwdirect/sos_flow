#include "sos.h"
#include "sosd.h"
#include "sosd_system.h"
#include "sos_debug.h"
#include <sys/stat.h>
#include <string.h>
#include <sstream>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <set>
#include <iostream>

using namespace std;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

#define SOS_HAVE_POWERCAP_POWER

static ProcData *oldData = nullptr;
static ProcData *newData = nullptr;
static ProcData *periodData = nullptr;
static SOS_pub *pub = nullptr;
static std::set<SOS_pub*> pubs;
static std::set<int> pids;

void sample_value (const char * name, double value) {
    SOS_pack(pub, name, SOS_VAL_TYPE_DOUBLE, &value);
    /* pack the name, value into the buffer */
}

inline bool file_exists (const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

#if defined(SOS_HAVE_POWERCAP_POWER)
/*
 * This isn't really the right way to do this. What should be done
 * is:
 * 1) read /sys/class/powercap/intel-rapl/intel-rapl:0/name to get the counter name (once)
 * 2) read /sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj to get the value
 * 3) for i in /sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:*:*
 *    do 1), 2) above for each
 *
 * This was a quick hack to get basic support for KNL.
 */
inline long long read_package0 (void) {
  long long tmplong;
  FILE *fff;
  fff=fopen("/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj","r");
  if (fff==NULL) {
    std::cerr << "Error opening package0!" << std::endl;
  } else {
    int num = fscanf(fff,"%lld",&tmplong);
    if (num <= 0) {
        std::cerr << "Error reading package0 power!" << std::endl;
    }
    fclose(fff);
  }
  return tmplong/1000000;
}

inline long long  read_dram (void) {
  //std::cout << "Reading dram" << std::endl;
  long long  tmplong;
  FILE *fff;
  fff=fopen("/sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:0/energy_uj","r");
  if (fff==NULL) {
    std::cerr << "Error opening dram!" << std::endl;
  } else {
    int num = fscanf(fff,"%lld",&tmplong);
    if (num <= 0) {
        std::cerr << "Error reading dram power!" << std::endl;
    }
    fclose(fff);
  }
  return tmplong/1000000;
}
#endif

bool parse_proc_self_status(SOS_pub *pid_pub) {
  std::stringstream buf;
  buf << "/proc/" << pid_pub->process_id << "/status";
  if (!file_exists(buf.str().c_str())) { return false; }
  FILE *f = fopen(buf.str().c_str(), "r");
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
            stringstream mname;
            //mname << pid << ":status:" << string(name);
            mname << "status:" << string(name);
            SOS_pack(pid_pub, mname.str().c_str(), SOS_VAL_TYPE_DOUBLE, &d1);
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
#if defined(SOS_HAVE_CRAY_POWER)
  procData->power = read_power();
  procData->power_cap = read_power_cap();
  procData->energy = read_energy();
  procData->freshness = read_freshness();
  procData->generation = read_generation();
#endif
#if defined(SOS_HAVE_POWERCAP_POWER)
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
#if defined(SOS_HAVE_CRAY_POWER)
  d->power = power;
  d->power_cap = power_cap;
  d->energy = energy - rhs.energy;
  d->freshness = freshness;
  d->generation = generation;
#endif
#if defined(SOS_HAVE_POWERCAP_POWER)
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
#if defined(SOS_HAVE_CRAY_POWER)
  sample_value("Power", power);
  sample_value("Power Cap", power_cap);
  sample_value("Energy", energy);
  sample_value("Freshness", freshness);
  sample_value("Generation", generation);
#endif
#if defined(SOS_HAVE_POWERCAP_POWER)
  sample_value("Package-0 Energy", package0);
  sample_value("DRAM Energy", dram);
#endif
}

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)


/* Get initial readings */
extern "C" void SOSD_setup_system_data(void) {
  oldData = parse_proc_stat();
}

void SOSD_setup_system_monitor_pub(void) {
    SOS_runtime *SOS = SOSD.sos_context;

    char * pub_title = strdup("system monitor");
    SOS_pub_init(SOSD.sos_context, &pub, pub_title, SOS_NATURE_SOS);
    SOS_pipe_init(SOSD.sos_context, &(pub->snap_queue), sizeof(SOS_val_snap *));
    std::stringstream version;
    version << STRINGIFY(SOS_VERSION_MAJOR) << "." << STRINGIFY(SOS_VERSION_MINOR);
    strcpy (pub->prog_ver, version.str().c_str());
    pub->meta.channel     = 1;
    pub->meta.nature      = SOS_NATURE_EXEC_WORK;
    pub->meta.layer       = SOS_LAYER_APP;
    pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    pub->meta.retain_hint = SOS_RETAIN_DEFAULT;

    pids.insert(0);
}

extern "C" void SOSD_add_pid_to_track(SOS_pub *pid_pub) {
  auto it = pids.find(pid_pub->process_id);
  if (it != pids.end()) { return; }
  if (pid_pub == pub) { return; }
  /* make our pub */
  SOS_pub * my_pub;
  std::stringstream pub_title;
  pub_title << "process monitor: " << pid_pub->title;
  SOS_pub_init(SOSD.sos_context, &my_pub, const_cast<char*>(pub_title.str().c_str()), SOS_NATURE_CREATE_OUTPUT);
  std::stringstream version;
  version << STRINGIFY(SOS_VERSION_MAJOR) << "." << STRINGIFY(SOS_VERSION_MINOR);
  strcpy (my_pub->prog_ver, version.str().c_str());
  my_pub->meta.channel     = 1;
  my_pub->meta.nature      = SOS_NATURE_EXEC_WORK;
  my_pub->meta.layer       = SOS_LAYER_APP;
  my_pub->meta.pri_hint    = SOS_PRI_DEFAULT;
  my_pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
  my_pub->meta.retain_hint = SOS_RETAIN_DEFAULT;
  my_pub->process_id = pid_pub->process_id;
  strcpy(my_pub->prog_name, pid_pub->prog_name);

  /* do some other setup, because we aren't a regular client */
  SOS_pipe_init(SOSD.sos_context, &(my_pub->snap_queue), sizeof(SOS_val_snap *));
  
  pubs.insert(my_pub);
  pids.insert(pid_pub->process_id);
}

extern "C" void SOSD_read_system_data(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_read_system_status");
    static bool got_pub = false;
    if (!got_pub) {
        SOSD_setup_system_monitor_pub();
        got_pub = true;
    }
    newData = parse_proc_stat();
    if (newData != nullptr && oldData != nullptr) {
        periodData = newData->diff(*oldData);
        periodData->sample_values();
        delete(oldData);
        delete(periodData);
        oldData = newData;
    }
    parse_proc_meminfo();
    dlog(8, "    Publishing: pub->title==\"%s\"\n", pub->title);
    SOS_publish(pub);
    /* this is less useful, unless we grab the status of each pid? */
    for (auto pid_pub : pubs) {
        parse_proc_self_status(pid_pub);
        dlog(8, "    Publishing: pid_pub->title==\"%s\"\n", pid_pub->title);
        SOS_publish(pid_pub);
    }
}

#pragma GCC diagnostic pop
