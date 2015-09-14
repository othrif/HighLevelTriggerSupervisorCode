// Othmane Rifki & R. Blair
// othmane.rifki@cern.ch

#ifndef _HLTSV_ROIB_H_
#define _HLTSV_ROIB_H_
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include "ROSRobinNP/RobinNPROIB.h"
#include "ROSEventFragment/ROBFragment.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_queue.h"

const uint  maxLinks=12;
const uint maxSize=128;
const uint maxEvWords=maxSize*maxLinks;
class builtEv
{
 public:
  builtEv();
  ~builtEv();
  std::chrono::time_point<std::chrono::high_resolution_clock> start() 
    { return m_start;};
  uint32_t * data() { return m_data;};
  uint32_t count() { return m_count;};
  uint32_t size() {return m_size;};
  //change to ->  const std::vector<uint32_t>& links() {return m_links;} const;
  std::vector<uint32_t> links() {return m_links;} ;
  void free() {if( m_data) delete[] m_data;m_data=0;return;};
  void add(uint32_t w,uint32_t *d,uint32_t link);
 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
  std::atomic<uint32_t> m_size;
  std::atomic<uint32_t> m_count;
  uint32_t * m_data;
  std::vector<uint32_t> m_links;
};
class RoIBuilder
{
 private:
  ROS::RobinNPROIB * m_module;
  uint32_t m_nrols;
  uint32_t m_nactive;
  tbb::concurrent_queue<uint32_t> m_l1ids;
  std::set<uint32_t> m_active_chan;
  typedef tbb::concurrent_hash_map<uint32_t,builtEv *> EventList;
  EventList m_events;
  std::vector<std::thread> m_rcv_threads;
  void m_rcv_proc();
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
