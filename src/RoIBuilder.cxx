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
  bool madenoise=true;

  pid_t myID=syscall(SYS_gettid);
  const uint32_t myThread=(uint32_t)nThread;
  nThread++;
  if(DebugMe)		
	ERS_LOG(" Receive thread "<<myThread<<" started with id:"<<myID);

  ///*
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
  while(!m_stop) {
	if(m_module == 0 ) 
	  return;

	if( m_running && m_done.size()<target ) {

	  if(madenoise) 
		ERS_LOG("thread "<<myThread<<" Done cooling");
	  madenoise=false;
	  target=maxBacklog;


	  /*
	  for(size_t chans=0; chans<maxLinks; chans++){
		int subrob = (chans<6) ? 0:1;
	  */
		if(DebugMe) 
		  ERS_LOG(" thread "<<myThread<<" initiating getFragment("
				  <<subrob<<")"); 
		
		auto fragment = m_module->getFragment(subrob);

		// check that the fragment is okay
		if(fragment.page->virtualAddress() == 0) {
		  ERS_LOG(" thread "<<myThread<<" invalid data from RobinNP");
		  continue;
		}

		// cycle through the subrobs
		///*
		subrob++;
		if(subrob>MaxSubrob) subrob=0;
		//*/


		rolId=fragment.rolId;
		// If this is from an active channel add it to a map indexed by l1id
		if(m_active_chan.find(rolId) != m_active_chan.end())
		  { 

			if(DebugMe ) 
			  ERS_LOG("thread "<<myThread<<" Processing fragment");

			uint32_t l1id=*(fragment.page->virtualAddress()+5);

			// Check status element
			if(fragment.fragmentStatus != 0) 
			  ERS_LOG("Fragment status " << 
					  std::hex << fragment.fragmentStatus << std::dec<<
					  " link:"<<rolId<<
					  " l1id:"<<l1id);

			m_mutex.lock();		  

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
			  m_done.push(l1id);
			}

			m_mutex.unlock();
			m_module->recyclePage(fragment);

		  }  else {
		
		  // if data comes in for links not in use throw it away 
		  ERS_LOG("thread "<<myThread<<
				  " received data on unused link:"<<rolId);
		  m_module->recyclePage(fragment);

		}
		/*
	  } // loop over channels
		*/
	} else {
	  if( !madenoise) {
		//	if( DebugMe ) 
		ERS_LOG("thread "<<myThread<<
				" cooling our jets, m_running="<<m_running<<
				" m_done.size()="
				<<m_done.size());

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
  delete m_events[lvl1id];
  m_events.erase(lvl1id);
  m_mutex.unlock();
}
bool RoIBuilder::getNext(uint32_t & l1id,uint32_t & count,uint32_t * & roi_data,uint32_t  & length)
{
  const std::chrono::microseconds limit(10000000);
  const uint32_t maxTotUnbuilt=3000;
  bool timeout=false;
  std::chrono::microseconds elapsed;
  std::set<uint32_t>::iterator l1id_index;
  builtEv * ev;
  count=0;
  m_mutex.lock();
  if(!m_done.empty() && m_events.size()-m_done.size() < maxTotUnbuilt) {
	l1id=m_done.front();
	m_done.pop();
	ev=((m_events.find(l1id)->second));
	count=ev->count();
  } else {
	// no completed events or too many unbuilt events so check for stale ones
	std::chrono::time_point<std::chrono::high_resolution_clock>thistime=
	  std::chrono::high_resolution_clock::now();

	for (const auto &myeventPair : m_events){
	  l1id = myeventPair.first;
	  ev = myeventPair.second;
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
