// Othmane Rifki & R. Blair
// othmane.rifki@cern.ch

#ifndef _HLTSV_ROIB_H_
#define _HLTSV_ROIB_H_
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include "ROSRobinNP/RobinNPROIB.h"
#include "ROSEventFragment/ROBFragment.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_queue.h"
#include <unistd.h>
#include "monsvc/MonitoringService.h"
#include <TH1F.h>

const uint  maxLinks=12;
// one beyond because the robinnp adds one
const uint maxSize=129;
const uint maxEvWords=maxSize*maxLinks;
const uint maxThreads=12;
class builtEv
{
 public:
  builtEv(uint64_t);
  ~builtEv();
  std::chrono::time_point<std::chrono::high_resolution_clock> start() 
    { return m_start;};
  std::chrono::time_point<std::chrono::high_resolution_clock> complete() 
    { return m_complete;};
  uint32_t * data() { return m_data;};
  uint32_t count() { return m_count;};
  uint32_t size() {return m_size;};
  uint64_t l1id64() {return m_l1id64;};
  //change to ->  const std::vector<uint32_t>& links() {return m_links;} const;
  const uint32_t * links() {return m_links;} ;
  void free() {if( m_data) delete[] m_data;m_data=0;return;};
  void add(uint32_t w,uint32_t *d,uint32_t link);
  void finish();
 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_complete;
  uint32_t m_size;
  uint32_t m_count;
  uint64_t m_l1id64;
  uint32_t * m_data;
  uint32_t m_links[maxLinks];
};
class RoIBuilder
{
 private:
  TH1F * m_timeComplete_hist;
  TH1F * m_timeProcess_hist;
  TH1F * m_nFrags_hist;
  TH1F * m_NPending_hist;
  TH1F * m_timeout_hist;
  TH1F * m_missedLink_hist;
  TH1F * m_readLink_hist[maxThreads];
  TH1F * m_subrobReq_hist[maxThreads];
  TH1F * m_backlog_hist;
  ROS::RobinNPROIB * m_module;
  uint32_t m_nrols;
  uint32_t m_nactive;
  tbb::concurrent_queue<uint64_t> m_l1ids;
  std::set<uint32_t> m_active_chan;
  typedef tbb::concurrent_hash_map<uint64_t,builtEv *> EventList;
  EventList m_events;
  std::vector<std::thread> m_rcv_threads;
  void m_rcv_proc(uint32_t);
  tbb::concurrent_bounded_queue<builtEv *> m_done;
 public:
  bool m_running;
  bool m_stop;
  RoIBuilder(ROS::RobinNPROIB *,std::vector<uint32_t>,uint32_t);
  ~RoIBuilder();
  bool getNext(uint32_t &,uint32_t &,uint32_t * &,uint32_t &);
  void release(uint32_t);
};
#endif
