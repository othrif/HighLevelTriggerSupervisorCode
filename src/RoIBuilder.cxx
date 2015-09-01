// Othmane Rifki & R. Blair
// othmane.rifki@cern.ch

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
const bool TimeMe=false;
const bool DebugData=false;
const uint32_t n_rcv_threads=2;
const uint32_t maxBacklog=50;
const uint32_t minBacklog=25;
const uint32_t maxPendEv=200000;

builtEv::builtEv() :m_size(0),m_count(0),m_data(0)
{
  m_start=std::chrono::high_resolution_clock::now();
  m_data=new uint32_t[maxEvWords];
  for (uint i=0;i<maxEvWords;i++)m_data[i]=0;
}

builtEv::~builtEv()
{
}

void builtEv::add(uint32_t w,uint32_t *d,uint32_t link) {

  auto size = w < maxSize ? w-1: maxSize;

  ::memcpy(&m_data[m_size], d, sizeof(uint32_t)*size);
  
  m_size += size;
  m_count++;
  m_links.push_back(link);
}

void  RoIBuilder::m_rcv_proc()
{

  //  std::chrono::time_point<std::chrono::high_resolution_clock> thistime;
  //  std::chrono::microseconds elapsed;

  // APIC CPU
  unsigned int pa, pb, pc, pd, pbb;
  __asm__("cpuid" : "=a" (pa), "=b" (pb), "=c" (pc), "=d" (pd) : "0" (1)); 
  pbb = pb >> 24;

  uint32_t rolId;
  uint32_t rolCnt[]={0,0,0,0,0,0,0,0,0,0,0,0};
  // Process Id
  pid_t myID=syscall(SYS_gettid);
  const uint32_t myThread=(uint32_t)nThread;
  nThread++;

  uint32_t MaxSubrob=(m_active_chan.size()>6?1:0);
  uint32_t subrob=myThread;
  subrob=subrob%(MaxSubrob+1);

  ERS_LOG(" Receive thread "<<myThread<<" started with id:"<<myID << 
		  " in CPU with id:"<< pbb<<
		  " MaxSubrob="<<MaxSubrob<<" starting subrob:"<<subrob);

  if(DebugMe ) 
	ERS_LOG(" Receive thread started");

  // loop reading out until asked to stop
  builtEv * ev(0);
  uint32_t target=maxBacklog;
  uint32_t report=0;  
  while(!m_stop) {

	if(m_module == 0 ) 
	  return;
	if( m_running && m_done.size() < target ) {
	  if( m_events.size()>=maxPendEv && report <1 && myThread==0) {
	    uint32_t counts[]={0,0,0,0,0,0,0,0,0,0,0,0};
	    report++;
	    ERS_LOG( "Way too many pending fragments:"<<m_events.size());
	    for ( auto ipt=m_events.begin();ipt!=m_events.end();ipt++){
	      for( auto jjj : (*ipt).second->links()) 
		counts[jjj]++;
	    }
	    for ( auto jjj=0;jjj<12;jjj++) ERS_LOG( " link" <<jjj << " counts:"<<counts[jjj]);
	  }
	  target=maxBacklog;
	  
	  if(DebugMe) 
	    ERS_LOG(" thread "<<myThread<<" initiating getFragment("
		    <<subrob<<")"); 
	  
	  auto fragment = m_module->getFragment(subrob);
	  // check that the fragment is okay
	  if(fragment.page->virtualAddress() == 0) {
	    ERS_LOG(" thread "<<myThread<<" invalid data from RobinNP");
	    continue;
	  }
	  
	  rolId=fragment.rolId;
	  rolCnt[rolId]++;
	  // next read is from the subrob with the lowest count in one of its links
	  uint32_t minVal=0xFFFFFFFF;
	  for( auto iLink=0;iLink<m_nrols;iLink++) if(rolCnt[iLink]<minVal){
	      minVal=rolCnt[iLink];
	      subrob=iLink>5?1:0;
	    }	  
	  // If this is from an active channel add it to a map indexed by l1id
	  if(m_active_chan.find(rolId) != m_active_chan.end())
	    { 
	      
	      
	      uint32_t l1id=*(fragment.page->virtualAddress()+5);
	      
	      
	      if(DebugMe ) 
		ERS_LOG("thread "<<myThread<<" Processing fragment with l1id " << l1id << " from ROL " << rolId);
	      
		  
		  // Check status element
		  if(fragment.fragmentStatus != 0 && DebugMe ) 
		    ERS_LOG("Fragment status " << 
			    std::hex << fragment.fragmentStatus << std::dec<<
			    " link:"<<rolId<<
			    " l1id:"<<l1id);
		  
		  EventList::accessor m_eventsLocator;
		  if (m_events.insert(m_eventsLocator, l1id)) {
		    // A new element was inserted
		    ev=new builtEv;
		    m_eventsLocator->second=ev;
		  } else {
		    // The element existed already
		    ev=(m_eventsLocator->second);
		  }
		  
		  // Concatenate fragments with same l1id but different rols
		  ev->add(fragment.dataSize,fragment.page->virtualAddress(),rolId);
		  
		  // fragment is built for l1id	
		  if(ev->count()==m_active_chan.size()) {
		    if(DebugMe) 
		      ERS_LOG("thread "<<myThread<<
			      " built lvl1id:"<<l1id);
		    m_done.push(ev);
		  }
		  
		  m_module->recyclePage(fragment);
		  
	    }  else {
		
	    // if data comes in for links not in use throw it away 
	    ERS_LOG("thread "<<myThread<<
		    " received data on unused link:"<<rolId);
	    m_module->recyclePage(fragment);
	    
	  }
	} else {
	  target=minBacklog;
	  //ERS_LOG("thread "<<myThread<< " I'm in backpressure.");
	  sched_yield();
	}
	continue;
  }
}

  
RoIBuilder::RoIBuilder(ROS::RobinNPROIB *module, std::vector<uint32_t> chans,uint32_t nrols)
  :m_stop(false),
   m_running(false),
   m_nrols(nrols)
{
  // stop reading once you reach the maximum number of events
  m_done.set_capacity(maxBacklog);
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
  for (EventList::iterator i=m_events.begin();i!=m_events.end();i++) delete i->second;
}

void RoIBuilder::release(uint32_t lvl1id)
{
  if(DebugMe) ERS_LOG(" erasing all traces of lvl1id:"<<lvl1id);
  EventList::accessor m_eventsLocator;
  if(m_events.find(m_eventsLocator,lvl1id)) {
	delete m_eventsLocator->second;
	m_events.erase(m_eventsLocator);
  }
}
bool RoIBuilder::getNext(uint32_t & l1id,uint32_t & count,uint32_t * & roi_data,uint32_t  & length)
{
  std::chrono::time_point<std::chrono::high_resolution_clock> thistime;
  const std::chrono::microseconds limit(100000);
  const uint32_t maxTot=1000000;
  const uint32_t maxCheck=10;
  bool timeout=false;
  static uint32_t ncheck=0;
  std::chrono::microseconds elapsed;
  std::vector<uint32_t> linkList;
  builtEv * ev;
  count=0;
  if(!m_done.empty() && (ncheck++ < maxTot || ncheck > maxTot+maxCheck)){
    if( m_done.try_pop(ev)) {
      l1id=ev->data()[5];
      count=ev->count();
      linkList=ev->links();
    }
    // if we already checked 10 wait for maxTot more events before checking
    if(ncheck>=maxTot) ncheck=0;
  } else {
    if( ncheck>maxTot ){
      // check for stale entries every so often
      thistime=
	std::chrono::high_resolution_clock::now();
      //      m_evmutex.lock();
      for (const auto &myeventPair : m_events){
	l1id = myeventPair.first;
	ev = myeventPair.second;
	elapsed=std::chrono::duration_cast<std::chrono::microseconds>
	  (thistime-ev->m_start);
	
	if(elapsed>limit && ev->count()<m_nactive) {
	  timeout=true;
	  count=ev->count();
	  linkList=ev->links();
	  break;
	}
      }
      if(!timeout){
	ncheck=0;
      }
    }
  }
  if( count == 0 ) {
    // no timeouts and no events
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
		for (uint32_t i=0;i<linkList.size();i++) 
		  linksin+=
			static_cast<std::ostringstream*>( &(std::ostringstream() << linkList.at(i)))->str()+" ";
		ERS_LOG("links:"<<linksin.c_str()<<"reported"); 
	  }
	  if(DebugMe) 
		{
		  if(!timeout) {
			for (uint32_t i=0;i<linkList.size();i++) 
			  linksin+=
				static_cast<std::ostringstream*>( &(std::ostringstream() << linkList.at(i)))->str()+" ";
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
	  return true;
	}
  return false;
}

