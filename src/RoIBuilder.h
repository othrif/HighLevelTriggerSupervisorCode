// Othmane Rifki, R. Blair, & J. Love
// othmane.rifki@cern.ch, jeremy.love@cern.ch

#ifndef _HLTSV_ROIB_H_
#define _HLTSV_ROIB_H_
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include "ROSRobinNP/RobinNPROIB.h"
#include "ROSEventFragment/ROBFragment.h"
#include "ROSDescriptorNP/DataPage.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_queue.h"
#include "tbb/concurrent_vector.h"
#include "tbb/tick_count.h"
#include <unistd.h>
#include "monsvc/MonitoringService.h"
#include <TH1D.h>
#include <TH2D.h>
#include "HLTSV.h"
#include "Issues.h"

const uint  maxLinks=12;
// one beyond because the robinnp adds one
const uint maxSize=130;
const uint maxEvWords=maxSize*maxLinks;
const uint maxThreads=12;
const uint NUMBER_OF_SUBROBS=2;
const double sectomicro = 1000000.;

class builtEv
{
 public:
  builtEv(uint64_t);
  ~builtEv();
  tbb::tick_count start() 
    { return m_start;};
  tbb::tick_count complete() 
    { return m_complete;};
  uint32_t * data() { return m_data;};
  uint32_t count() { return m_count;};
  uint32_t size() {return m_size;};
  uint64_t l1id64() {return m_l1id64;};
  //change to ->  const std::vector<uint32_t>& links() {return m_links;} const;
  const uint32_t * links() {return m_links;} ;
  void free() {if( m_data) delete[] m_data;m_data=0;return;};
  void add(uint32_t w,uint32_t *d,uint32_t link,ROS::ROIBOutputElement &fragment);
  tbb::concurrent_vector<ROS::ROIBOutputElement> associatedPages(){return pages;}
  void finish();
  tbb::concurrent_queue<std::pair<uint32_t,uint32_t>> getErrors()
    {return m_errors;}
  void addError(uint32_t link,uint32_t val)
  {m_errors.push(std::make_pair(link,val));}
 private:
  tbb::tick_count m_start;
  tbb::tick_count m_complete;
  uint32_t m_size;
  uint32_t m_count;
  uint64_t m_l1id64;
  uint32_t * m_data;
  uint32_t m_links[maxLinks];
  tbb::concurrent_vector<ROS::ROIBOutputElement> pages;
  tbb::concurrent_queue<std::pair<uint32_t,uint32_t>> m_errors;
};
class RoIBuilder
{
 private:
  TH1D * m_timeComplete_hist;
  TH1D * m_timeProcess_hist;
  TH1D * m_nFrags_hist;
  TH1D * m_NPending_hist;
  TH1D * m_NResultHandled_hist;
  TH1D * m_NumNPDMAPagesFree_hist[24];//No hists for more than 2 RobinNPs
  TH1D * m_timeout_hist;
  TH1D * m_missedLink_hist;
  TH1D * m_lastChan_hist;
  TH1D * m_firstChan_hist;
  TH1D * m_readLink_hist[maxThreads];
  TH1D * m_subrobReq_hist[maxThreads];
  TH1D * m_backlog_hist;
  TH2D * m_fragSize_hist;
  TH2D * m_fragError_hist;
  ROS::RobinNPROIB * m_module;
  ROS::RobinNPROLStats * m_rolStats;
  uint32_t m_nrols;
  uint32_t m_nactive;
  float m_timeout;
  uint32_t m_limit;
  uint32_t m_time_Build;
  uint32_t m_time_Process;
  uint32_t m_NPending;
  uint32_t m_backlog;
  uint64_t m_sleep=5000;
  double m_fraction=0.5;
  std::vector<uint64_t> m_timedoutL1ID;
  tbb::atomic<uint64_t> m_rolSize[maxLinks];
  tbb::tick_count m_rolTime;
  tbb::concurrent_queue<uint64_t> m_l1ids;
  std::set<uint32_t> m_active_chan;
  typedef tbb::concurrent_hash_map<uint64_t,builtEv *> EventList;
  EventList m_events;
  std::vector<std::thread> m_rcv_threads;
  std::thread m_result_thread;
  void m_rcv_proc(uint32_t);
  tbb::concurrent_bounded_queue<builtEv *> m_done;
  std::mutex          m_result_mutex[NUMBER_OF_SUBROBS];
  tbb::concurrent_queue<long unsigned int>  m_result_lists;
  void check_results( );
  void freePages(tbb::concurrent_vector<ROS::ROIBOutputElement> evPages);
  bool isLateFragment(uint64_t el1id);
  
 public:
  bool m_running;
  bool m_stop;
  RoIBuilder(ROS::RobinNPROIB *,std::vector<uint32_t>);
  ~RoIBuilder();
  bool getNext(uint32_t &,uint32_t &,uint32_t * &,uint32_t &,uint64_t &);
  void release(uint64_t);
  void getISInfo( hltsv::HLTSV * info);
  void setTimeout(float timeout){m_limit=timeout;}
  void setSleep(uint64_t sleep){m_sleep=sleep;}
  void setFraction(double fraction){m_fraction=fraction;}
};

enum roiStatusError{
  roiStatusOk                  = 0x00000000,  // fragment OK

  // HLTSV
  INCOMPLETE_ROI               = 0x00010000,  // bit 16 // Incomplete RoI should be set by the HLTSV in the ROB specific status

  // RobinNP flagged errors
  roiStatusTxError		       = 0x04000000,  // bit 26 // transmission error. Set on any S-Link or fragment format error condition
  roiStatusSeqError		       = 0x02000000,  // bit 25 // out-of-sequence fragment
  roiStatusFormatError	       = 0x00800000,  // bit 23 // UPF: Major format version mismatch
  roiStatusMarkerError	       = 0x00400000,  // bit 22 // UPF: invalid header marker
  roiStatusEofError		       = 0x00200000,  // bit 21 // UPF: missing EOF
  roiStatusCtlError		       = 0x00080000,  // bit 19 // UPF: ctl word error
  roiStatusDataError	       = 0x00040000,  // bit 18 // UPF: data block error
  roiStatusSizeError	       = 0x00020000,  // bit 17  // UPF: fragment size error
  roiGenStatusError		       = 0x00000008,  // bit 3  // data error: corrupted fragment
  //------------
  roiErrorMask = roiStatusTxError | roiStatusSeqError | roiStatusFormatError | roiStatusMarkerError | roiStatusEofError | \
  roiStatusCtlError | roiStatusDataError | roiStatusSizeError | roiGenStatusError

};

#endif
