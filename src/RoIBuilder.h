#ifndef _HLTSV_ROIB_H_
#define _HLTSV_ROIB_H_
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include "ROSRobinNP/RobinNPROIB.h"
#include "ROSEventFragment/ROBFragment.h"
const uint  maxLinks=12;
const uint maxEvWords=128*maxLinks;
class builtEv
{
 public:
  builtEv();
~builtEv();
 std::chrono::time_point<std::chrono::steady_clock> m_start;
 uint32_t * data() { return m_data;};
 uint32_t count() { return m_count;};
 uint32_t size() {return m_size;};
 void free() {if( m_data) delete m_data;m_data=0;return;};
 void add(uint32_t w,uint32_t *d) {
   memcpy(&m_data[m_size],d,sizeof(uint32_t)*(w>0?w-1:0));
   m_size+=(w>0?w-1:0);m_count++;};
 private:
 uint32_t m_size;
 uint32_t m_count;
 uint32_t * m_data;
};
class RoIBuilder
{
 private:
  ROS::RobinNPROIB * m_module;
  uint32_t m_nrols;
  uint32_t m_nactive;
  std::set<uint32_t> m_active_chan;
  std::map<uint32_t,builtEv *> m_events;
  std::set<uint32_t> m_l1ids;
  std::mutex m_mutex;
  std::vector<std::thread> m_rcv_threads;
  void m_rcv_proc();
  bool m_new_data;
 public:
  bool m_running;
  bool m_stop;
  RoIBuilder(ROS::RobinNPROIB *,std::vector<uint32_t>,uint32_t);
  ~RoIBuilder();
  bool getNext(uint32_t &,uint32_t &,uint32_t * &,uint32_t &);
  void release(uint32_t);
};
#endif
