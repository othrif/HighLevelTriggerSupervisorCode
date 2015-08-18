// Othmane Rifki
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
const bool DebugData=false;
const uint32_t n_rcv_threads=1;
const uint32_t maxBacklog=50;
const uint32_t minBacklog=25;
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
  uint32_t rolId;

  pid_t myID=syscall(SYS_gettid);
  const uint32_t myThread=(uint32_t)nThread;
  nThread++;

  ///*
  uint32_t nreq=0;
  uint32_t MaxSubrob=(m_active_chan.size()>6?1:0);
  uint32_t subrob=myThread;
  subrob=subrob%(MaxSubrob+1);
  ERS_LOG(" Receive thread "<<myThread<<" started with id:"<<myID<<
		  " MaxSubrob="<<MaxSubrob<<" starting subrob:"<<subrob);
  //*/

  if(DebugMe ) ERS_LOG(" Receive thread started");
  // loop readng out until asked to stop
  builtEv * ev(0);
  uint32_t target=maxBacklog;
  // if multithread start at different subrobs
  auto chanVal=m_active_chan.find(subrob*6);
  while(!m_stop) {
	if(m_module == 0 ) 
	  return;
	if( m_running && m_done.size()<target ) {
	  target=maxBacklog;
	  if(DebugMe) 
	    ERS_LOG(" thread "<<myThread<<" initiating getFragment("
		    <<subrob<<")"); 
	  if(chanVal==m_active_chan.end())chanVal=m_active_chan.begin();
	  subrob=(*chanVal>5?1:0);
	  auto fragment = m_module->getFragment(subrob);
	  chanVal++;
	  // sample subrobs according to link occupancy
	  // check that the fragment is okay
	  if(fragment.page->virtualAddress() == 0) {
	    ERS_LOG(" thread "<<myThread<<" invalid data from RobinNP");
	    continue;
	  }
	  rolId=fragment.rolId;
	  // If this is from an active channel add it to a map indexed by l1id
	  if(m_active_chan.find(rolId) != m_active_chan.end())
		  { 
		    
		    if(DebugMe ) 
		      ERS_LOG("thread "<<myThread<<" Processing fragment");
		    
		    uint32_t l1id=*(fragment.page->virtualAddress()+5);
		    
		    // Check status element
		    if(fragment.fragmentStatus != 0 && DebugMe ) 
		      ERS_LOG("Fragment status " << 
			      std::hex << fragment.fragmentStatus << std::dec<<
			      " link:"<<rolId<<
			      " l1id:"<<l1id);
		    m_evmutex.lock();		  
		    m_eventsLocator=m_events.find(l1id);
		    
		    if( m_eventsLocator == m_events.end()  ) 
		      {
			if(DebugMe) 
			  ERS_LOG("thread "<<myThread<<
				  " starting to build lvl1id:"<<l1id);
			ev=new builtEv;
			m_events.insert(std::pair<uint32_t,builtEv *>(l1id,ev));
		      } else { 
			  ev=(m_events.find(l1id)->second);
		    }
		    
		    // Concatenate fragments with same l1id but different rols
		    ev->add(fragment.dataSize,fragment.page->virtualAddress(),rolId);
			
		    // fragment is built for l1id	
		    if(ev->count()==m_active_chan.size()) {
		      if(DebugMe) 
			ERS_LOG("thread "<<myThread<<
				" built lvl1id:"<<l1id);
		      m_mutex.lock();
		      m_done.push(ev);
		      m_mutex.unlock();
		    }
		    
		    m_evmutex.unlock();
		    m_module->recyclePage(fragment);
		    
		  }  else {
	    
	    // if data comes in for links not in use throw it away 
	    ERS_LOG("thread "<<myThread<<
		    " received data on unused link:"<<rolId);
	    m_module->recyclePage(fragment);
	    
	  }
	} else {
	  target=minBacklog;
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
  // if this event is a timeout the ev mutex is already locked do not lock
  if(m_events[lvl1id]->count() == m_nactive)  m_evmutex.lock();
  delete m_events[lvl1id];
  m_events.erase(lvl1id);
  m_evmutex.unlock();
}
bool RoIBuilder::getNext(uint32_t & l1id,uint32_t & count,uint32_t * & roi_data,uint32_t  & length)
{
  std::chrono::time_point<std::chrono::high_resolution_clock> thistime;
  const std::chrono::microseconds limit(10000000);
  const uint32_t maxTot=1000;
  const uint32_t maxCheck=10;
  bool timeout=false;
  static uint32_t ncheck=0;
  std::chrono::microseconds elapsed;
  std::vector<uint32_t> linkList;
  builtEv * ev;
  count=0;
  m_mutex.lock();
  if(!m_done.empty() &&  (ncheck++ < maxTot || ncheck > maxTot+maxCheck)) {
    ev=m_done.front();
    m_done.pop();
    m_mutex.unlock();
    l1id=ev->data()[5];
    count=ev->count();
    linkList=ev->links();
    // if we already checked 10 wait for maxTot more events before checking
    if(ncheck>=maxTot) ncheck=0;
  } else {
    m_mutex.unlock();  
    if( ncheck>maxTot ){
      // check for stale entries every so often
      thistime=
	std::chrono::high_resolution_clock::now();
      m_evmutex.lock();
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
	// do not unlock timed out events until they are processed    
	m_evmutex.unlock();
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
