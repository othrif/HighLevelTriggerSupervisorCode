#include "RoIBuilder.h"
#include <utility>
#include <functional>
#include "ers/ers.h"
#include <sys/syscall.h>
#include <sys/types.h>
#include <atomic>
#include <string>
#include <sstream>
#include <ostream>
std::atomic<unsigned int> nThread(0);
const bool DebugMe=false;
const bool DebugData=false;
const uint32_t n_rcv_threads=2;
const uint32_t maxBacklog=2000;
const uint32_t minBacklog=1500;
builtEv::builtEv() :m_size(0),m_count(0),m_data(0)
{
  m_start=std::chrono::high_resolution_clock::now();
  m_data=new uint32_t[maxEvWords];
  for (uint i=0;i<maxEvWords;i++)m_data[i]=0;
}
builtEv::~builtEv()
{
}
void  RoIBuilder::m_rcv_proc()
{
  const uint32_t nFragsMax=1000;
  uint32_t readCount=0;
  const uint32_t reportMod=1000000;
  bool report=true;
  uint32_t rolId;
  std::queue<ROS::ROIBOutputElement> frags;
  bool madenoise=true;
  pid_t myID=syscall(SYS_gettid);
  const uint32_t myThread=(uint32_t)nThread;
  nThread++;
  uint32_t MaxSubrob=(m_active_chan.size()>6?1:0);
  uint32_t subrob=myThread;
  subrob=subrob%(MaxSubrob+1);
  ERS_LOG(" Receive thread "<<myThread<<" started with id:"<<myID<<
	  " MaxSubrob="<<MaxSubrob<<" starting subrob:"<<subrob);
  if(DebugMe ) ERS_LOG(" Receive thread started");
  // loop readng out until asked to stop
  builtEv * ev(0);
  uint32_t target=maxBacklog;
  while(!m_stop) {
    if(m_module == 0 ) return;
    if( m_running && m_done.size()<target ) {
      if(madenoise) ERS_LOG("thread "<<myThread<<" Done cooling");
      madenoise=false;
      target=maxBacklog;
      if(DebugMe ) ERS_LOG("thread "<<myThread<<" frags no.:"<<frags.size());
      if(frags.empty()){
	// no fragments so try to get one whether or not you can lock the
	// maps
	  if(DebugMe) ERS_LOG(" thread "<<myThread<<" initiating getFragment("
			      <<subrob<<")"); 
	  frags.push(m_module->getFragment(subrob));
	  if(++readCount%reportMod == 0) 
	    ERS_LOG(" thread "<<myThread<<" did "<<readCount<<
		    " getFragment calls"); 
	  if(DebugMe ) 
	    ERS_LOG(" thread "<<myThread<<" Read fragment from subrob "<<
		    subrob<<" with rolid:"<<(frags.front()).rolId);
	  // cycle through the subrobs
	  //	  subrob++;
	  // if(subrob>MaxSubrob) subrob=0;
      }
      // check that the next fragment is okay
      if((frags.front()).page->virtualAddress() == 0) {
	ERS_LOG(" thread "<<myThread<<" invalid data from RobinNP");
	frags.pop();
	continue;
      }
      rolId=(frags.front()).rolId;
      // If this is from an active channel add it to a map indexed by l1id
      if(m_active_chan.find(rolId) != m_active_chan.end())
	{ 
	  if(!m_mutex.try_lock()) {
	    // another thread is building so get a fragment
	    if( frags.size()<nFragsMax ) {
	      if(DebugMe) ERS_LOG(
				  " thread "<<myThread<<
				  " initiating getFragment("
				  <<subrob<<")"); 
	      frags.push(m_module->getFragment(subrob));
	      if(++readCount%reportMod == 0) 
		ERS_LOG(" thread "<<myThread<<" did "<<readCount<<
			" getFragment calls"); 
	      if(DebugMe) 
		ERS_LOG(" thread "<<myThread<<
			" Read fragment (nolock) from subrob "<<
			subrob<<" with rolid:"<<(frags.front()).rolId);
	      // cycle through the subrobs
	      //  subrob++;
	      //if(subrob>MaxSubrob) subrob=0;
	      report=true;
	    } else {
	      if( report && DebugMe)
		ERS_LOG(" too many fragments "<<frags.size()<<" no lock "<<
			readCount);
	      report=false;
	      // stuck - no lock and too much data already
	      sched_yield();
	    }
	    continue;
	  }
	  // we have the lock and a fragment is available to build
	  // so use it then pop it from the queue
	  if(DebugMe ) ERS_LOG("thread "<<myThread<<" Processing fragment");
	  uint32_t l1id=*((frags.front()).page->virtualAddress()+5);
	  if((frags.front()).fragmentStatus != 0) 
	    ERS_LOG("Fragment status " << 
		    std::hex << (frags.front()).fragmentStatus << std::dec<<
		    " link:"<<rolId<<
		    " l1id:"<<l1id);
	  if(m_l1ids.find(l1id) == m_l1ids.end()) 
	    {if(DebugMe) ERS_LOG("thread "<<myThread<<
				 " starting to build lvl1id:"<<l1id);
	      m_l1ids.insert(l1id);
	      ev=new builtEv;
	      m_events.insert(std::pair<uint32_t,builtEv *>(l1id,ev));
	    } else ev=(m_events.find(l1id)->second);
	  ev->add((frags.front()).dataSize,(frags.front()).page->virtualAddress(),rolId);
	  if(ev->count()==m_active_chan.size()) {
	    m_done.push(l1id);
	  }
	  m_mutex.unlock();
	  m_module->recyclePage(frags.front());
	  frags.pop();
	  if(DebugMe) ERS_LOG("thread "<<myThread<<" received data from rol:"<<
			      rolId<<" with l1id:"<<l1id);
	}  else {
      // if data comes in for links not in use throw it away 
	ERS_LOG("thread "<<myThread<<
			    " received data on unused link:"<<rolId);
	m_module->recyclePage(frags.front());
	frags.pop();
      }
    } else {
      if( !madenoise) {
	//	if( DebugMe ) 
	  ERS_LOG("thread "<<myThread<<
		  " cooling our jets, m_running="<<m_running<<
		  " m_l1ids.size()="
		  <<m_l1ids.size());
	madenoise=true;
	target=minBacklog;
      }
      sched_yield();
      continue;
    }
  }
}
RoIBuilder::RoIBuilder(ROS::RobinNPROIB *module, std::vector<uint32_t> chans,uint32_t nrols)
    :m_stop(false),
     m_running(false),
     m_nrols(nrols)
  {
    m_nactive=0;
    m_module=module;
    m_nactive=chans.size();
    if(DebugMe) 
      ERS_LOG("m_nactive:"<<m_nactive<<" nrols:"<<nrols);
    for (auto j:chans) m_active_chan.insert(j);
    for (uint32_t i=0;i<nrols;i++) {
      if(m_active_chan.find(i) != m_active_chan.end() )
	if(DebugMe) ERS_LOG(" ROL "<<i<<" interogated and enabled");
    }
    // for now we spawn readout threads
    for (uint32_t i=0;i<n_rcv_threads;i++) m_rcv_threads.push_back(std::thread(&RoIBuilder::m_rcv_proc,this));
  }
RoIBuilder::~RoIBuilder()
{
  m_running=false;
  m_stop=true;
  for (uint32_t i=0;i<m_rcv_threads.size();i++) m_rcv_threads[i].join();
  if(DebugMe) ERS_LOG(" receive threads and their data cleaned up");
  for (std::map<uint32_t,builtEv *>::iterator i=m_events.begin();i!=m_events.end();i++) delete i->second;
}
void RoIBuilder::release(uint32_t lvl1id)
{
  if(DebugMe) ERS_LOG(" erasing all traces of lvl1id:"<<lvl1id);
  m_mutex.lock();
  m_l1ids.erase(lvl1id);
  m_events.erase(lvl1id);
  m_mutex.unlock();
}
  bool RoIBuilder::getNext(uint32_t & l1id,uint32_t & count,uint32_t * & roi_data,uint32_t  & length)
  {
    const std::chrono::microseconds limit(100000);
    bool timeout=false;
    std::chrono::microseconds elapsed;
    std::set<uint32_t>::iterator l1id_index;
    builtEv * ev;
    count=0;
    m_mutex.lock();
    if(!m_done.empty()) {
      l1id=m_done.front();
      m_done.pop();
      ev=((m_events.find(l1id)->second));
      count=ev->count();
    } else {
    // no completed events so check for stale ones
       std::chrono::time_point<std::chrono::high_resolution_clock>thistime=
	 std::chrono::high_resolution_clock::now();
      for ( l1id_index=m_l1ids.begin();l1id_index!=m_l1ids.end();l1id_index++) {
	l1id=*l1id_index;
	ev=((m_events.find(l1id)->second));
	elapsed=std::chrono::duration_cast<std::chrono::microseconds>
	  (thistime-ev->m_start);
	if(elapsed>limit && ev->count()<m_nactive) {
	  timeout=true;
	  count=ev->count();
	  break;
	}
      }
    }
    if( count == 0 ) {
      // no timeouts and no events
      m_mutex.unlock();
      return false;
    }
    length=ev->size();
    roi_data=ev->data();
    if(DebugMe) ERS_LOG(" lvl1id:"<<l1id<<" has "<<count<<" fragments"<<
			" total size is "<< length << " in words");
    if(count == m_nactive || timeout )
      { 
	std::string linksin;
	if(timeout) {
	  ERS_LOG(" event with lvl1id:"<<l1id<<" timed out "<<
		  elapsed.count());
	  ERS_LOG(" call to getnext returns true with " <<count << 
		    " fragments");
	  for (uint32_t i=0;i<(ev->links()).size();i++) 
	    linksin+=
	      static_cast<std::ostringstream*>( &(std::ostringstream() << (ev->links()).at(i)))->str()+" ";
	  ERS_LOG("links:"<<linksin.c_str()<<"reported"); 
	  //	  ERS_LOG(" data dump:");
	  //	  for (uint32_t j=0;j<length;j++) 
	  //	    std::cout <<j<<":"<<std::hex<<roi_data[j];
	}
	if(DebugMe) 
	  {
	    if(!timeout) {
	      for (uint32_t i=0;i<(ev->links()).size();i++) 
		linksin+=
		  static_cast<std::ostringstream*>( &(std::ostringstream() << (ev->links()).at(i)))->str()+" ";
	      ERS_LOG("links:"<<linksin.c_str()<<"reported"); 
	    }
	    ERS_LOG(" received a complete event with lvl1id:"<<l1id);
	    if( !timeout) 
	      ERS_LOG(" call to getnext returns true with " <<count << 
		      " fragments");
	    if( DebugData){
	      ERS_LOG(" data dump:");
	      for (uint32_t j=0;j<length;j++) 
		ERS_LOG(j<<":"<<std::hex<<roi_data[j]);
	    }
	  }
	m_mutex.unlock();
	return true;
      }
      m_mutex.unlock();
      return false;
  }
