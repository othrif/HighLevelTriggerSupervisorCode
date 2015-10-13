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

const bool DebugMe=false;
const bool DebugData=false;
std::string dir="/DEBUG/RoIBHistograms/";
std::set<std::string> hist_names;
const uint32_t maxBacklog=10000;
uint64_t myReads[]={0,0};
builtEv::builtEv(uint64_t l1id64) :
  m_size(0),m_count(0),m_l1id64(l1id64),m_data(0)
{
  m_start=std::chrono::high_resolution_clock::now();
  m_data=new uint32_t[maxEvWords];
  memset(m_data,0,sizeof(uint32_t)*maxEvWords);
}

builtEv::~builtEv()
{
}

void builtEv::add(uint32_t w,uint32_t *d,uint32_t link) {

  auto size = w < maxSize ? w-1: maxSize;
  const uint32_t slot=m_size;
  // move the size then copy the data
  m_size += size;
  ::memcpy(&m_data[slot], d, sizeof(uint32_t)*size);
  m_links[m_count]=link;
  m_count++;
}
void builtEv::finish() {
  m_complete=std::chrono::high_resolution_clock::now();
}
void  RoIBuilder::m_rcv_proc(uint32_t myThread)
{
  uint32_t Nwrap[]={0,0,0,0,0,0,0,0,0,0,0,0};
  uint32_t lastId[]={0,0,0,0,0,0,0,0,0,0,0,0};  
  uint32_t rolId;
  uint64_t rolCnt[]={0,0,0,0,0,0,0,0,0,0,0,0};
  uint32_t MaxSubrob=(m_active_chan.size()>6?1:0);
  uint32_t subrob=myThread;
  uint64_t el1id;
  float myFraction;
  uint64_t chanCount=0;
  const uint64_t limitFrac=1000;
  subrob=subrob%(MaxSubrob+1);
  pid_t myID=syscall(SYS_gettid);
  ERS_LOG(" Receive thread "<<myThread<<" started with id:"<<myID << 
		  " MaxSubrob="<<MaxSubrob<<" starting subrob:"<<subrob);
  for (auto i:m_active_chan ) 
    if( (i>5 && subrob == 1) || (i<6 && subrob ==0))  chanCount++;
  myFraction=(float)chanCount/(float)m_active_chan.size();
  ERS_LOG(" Thread "<<myThread<<" expects "<<myFraction<<" of the events");
  if(DebugMe ) 
	ERS_LOG(" Receive thread started");

  // loop reading out until asked to stop
  while(!m_stop) {

	if(m_module == 0 ) 
	  return;
	if( m_running && m_done.size()<maxBacklog && 
	    myReads[subrob]<
	    chanCount*(limitFrac+
		       (uint64_t)(myFraction*(float)
				  (myReads[0]+myReads[1])))) {
	  if(DebugMe) 
	    ERS_LOG(" thread "<<myThread<<" initiating getFragment("
		    <<subrob<<")"); 
	  bool newL1id=false;
	  auto fragment = m_module->getFragment(subrob);
	  m_subrobReq_hist[myThread]->Fill(subrob);
	  // check that the fragment is okay
	  if(fragment.page->virtualAddress() == 0) {
	    ERS_LOG(" thread "<<myThread<<" invalid data from RobinNP");
	    continue;
	  }
	  
	  rolId=fragment.rolId;
	  m_readLink_hist[myThread]->Fill(rolId);
	  /*
	  // next read is from the subrob with the lowest count in one of its links
	  uint64_t minVal=0xFFFFFFFFFFFFFFFF;
	  for( auto iLink:m_active_chan) if(rolCnt[iLink]<minVal){
	      minVal=rolCnt[iLink];
	      subrob=iLink>5?1:0;
	    }
	  */	  
	  // If this is from an active channel add it to a map indexed by l1id
	  if(m_active_chan.find(rolId) != m_active_chan.end())
	    { 
	      rolCnt[rolId]++;
	      myReads[subrob]++;
	      uint32_t l1id=*(fragment.page->virtualAddress()+5);
	      //check for ID wrap around
	      if( (0xFFF00000&l1id)<(0xFFF00000&lastId[rolId])) {
		ERS_LOG("thread "<<myThread<<" l1id has wrapped, this l1id:"
			<<l1id<<" previous id:"
			<<lastId[rolId] <<" link:"<<rolId);
		// exempt a wrap as the first event comes in
		if( lastId[rolId]!=0)Nwrap[rolId]++;
	      }
	      lastId[rolId]=l1id;
	      if(DebugMe ) 
		ERS_LOG("thread "<<myThread<<" Processing fragment with l1id " << l1id << " from ROL " << rolId);
	      
		  
		  // Check status element
		  if(fragment.fragmentStatus != 0 ) 
		    ERS_LOG("Fragment status " << 
			    std::hex << fragment.fragmentStatus << std::dec<<
			    " link:"<<rolId<<
			    " l1id:"<<l1id);
		  EventList::accessor m_eventsLocator;
		  el1id=(uint64_t)l1id|(uint64_t)Nwrap[rolId]<<32;
		  if (m_events.insert(m_eventsLocator, el1id)) {
		    // A new element was inserted
		    m_eventsLocator->second=new builtEv(el1id);
		    newL1id=true;
		  }
		  builtEv * & ev=m_eventsLocator->second;
		  
		  // Concatenate fragments with same l1id but different rols
		  ev->add(fragment.dataSize,fragment.page->virtualAddress(),rolId);
		  
		  // fragment is built for l1id	
		  if(ev->count()>=m_nactive) {
		    if(DebugMe) 
		      ERS_LOG("thread "<<myThread<<
			      " built lvl1id:"<<l1id);
		    // this will block until the list of done events drains
		    // below maxBacklog
		    ev->finish();
		    m_done.push(ev);
		  }
		  
		  if( newL1id) m_l1ids.push(el1id);
		  m_module->recyclePage(fragment);
		  
	    }  else {
		
	    // if data comes in for links not in use throw it away 
	    ERS_LOG("thread "<<myThread<<
		    " received data on unused link:"<<rolId);
	    m_module->recyclePage(fragment);
	    
	  }
	} else usleep(250);
  }
}

  
RoIBuilder::RoIBuilder(ROS::RobinNPROIB *module, std::vector<uint32_t> chans,uint32_t nrols)
  :m_nrols(nrols),
   m_running(false),
   m_stop(false)
{
  //the current code requires n_rcv_threads to be set to two=# subrobs
  const uint32_t n_rcv_threads=2;
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
  // set up histograms
  std::string name="Time to complete";
  m_timeComplete_hist=
    new TH1F(name.c_str(),"assembly time;microseconds;",
	     100,0,10000000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_timeComplete_hist);
  hist_names.insert(name);
  name="Time to process";
  m_timeProcess_hist=
    new TH1F(name.c_str(),"total processing time;microseconds;",100,0,10000000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_timeProcess_hist);
  hist_names.insert(name);
  name="Fragment count";
  m_nFrags_hist=new TH1F(name.c_str(),"number of fragments;;",13,-.5,12.5);
  monsvc::MonitoringService::instance().register_object(dir+name,m_nFrags_hist);
  hist_names.insert(name);
  name="Pending event count";
  m_NPending_hist=new TH1F(name.c_str(),"number of pending events;;",
			   100,0,100000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_NPending_hist);
  hist_names.insert(name);
  name="time elapsed for timeouts";
  m_timeout_hist= new TH1F(name.c_str(),"elapsed time;microseconds;",
			   100,0,1000000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_timeout_hist);
  hist_names.insert(name);
  name="link missed";
  m_missedLink_hist=new TH1F(name.c_str(),"missing links in timeouts;;",
			     12,0,12);
  monsvc::MonitoringService::instance().register_object(dir+name,m_missedLink_hist);
  hist_names.insert(name);
  // for now we spawn readout threads
  for (uint32_t i=0;i<n_rcv_threads;i++) {
    char histname[50];
    sprintf(histname,"links read by thread %d",i);
    name =std::string(histname);
    m_readLink_hist[i]=new TH1F(histname,"channels read;;",
				maxLinks,-.5,maxLinks-.5);
    monsvc::MonitoringService::instance().register_object(dir+name,
							  m_readLink_hist[i]);
    hist_names.insert(name);
    sprintf(histname,"subrobs read by thread %d",i);
    name =std::string(histname);
    m_subrobReq_hist[i]=new TH1F(histname,"subrob requests;;",2,-.5,1.5);
    monsvc::MonitoringService::instance().register_object(dir+name,
							  m_subrobReq_hist[i]);
    hist_names.insert(name);
    m_rcv_threads.push_back(std::thread(&RoIBuilder::m_rcv_proc,this,i));
  }
  name="Events complete and waiting for DAQ";
  m_backlog_hist=new TH1F(name.c_str(),"events in queue;;",110,0,11000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_backlog_hist);
}

RoIBuilder::~RoIBuilder()
{
  m_running=false;
  m_stop=true;
  for (uint32_t i=0;i<m_rcv_threads.size();i++) m_rcv_threads[i].join();
  if(DebugMe) ERS_LOG(" receive threads and their data cleaned up");
  for (EventList::iterator i=m_events.begin();i!=m_events.end();i++) delete i->second;
  for (auto name:hist_names)
  monsvc::MonitoringService::instance().remove_object(dir+name);

}

void RoIBuilder::release(uint64_t lvl1id)
{
  if(DebugMe) ERS_LOG(" erasing all traces of lvl1id:"<<lvl1id);
  EventList::accessor m_eventsLocator;
  if(m_events.find(m_eventsLocator,lvl1id)) {
	delete m_eventsLocator->second;
	m_events.erase(m_eventsLocator);
  } else {
    ERS_LOG(" could not find event:"<<lvl1id<<" for release");
  }
}
bool RoIBuilder::getNext(uint32_t & l1id,uint32_t & count,uint32_t * & roi_data,uint32_t  & length, uint64_t & el1id)
{
  std::chrono::time_point<std::chrono::high_resolution_clock> thistime;
  const std::chrono::microseconds limit(30000000);
  const uint32_t maxTot=1000000;
  const uint32_t maxCheck=10;
  bool timeout=false;
  static uint32_t ncheck=0;
  static uint64_t lastId=0;
  std::chrono::microseconds elapsed;
  const uint32_t * linkList;
  builtEv * evdone;
  uint64_t l1id_timedOut=0;
  std::chrono::time_point<std::chrono::high_resolution_clock> time;
  count=0;
  m_backlog_hist->Fill(m_done.size());
  // if we already checked 10 wait for maxTot more events before checking
  if(++ncheck>maxTot+maxCheck) ncheck=0;
  if( ncheck < maxTot){
    if( m_done.try_pop(evdone)) {
      length=evdone->size();
      count=evdone->count();
      roi_data=evdone->data();
      l1id=roi_data[5];
      el1id=evdone->l1id64();
      if(DebugMe) ERS_LOG(" lvl1id:"<<l1id<<" has "<<count<<" fragments"<<
			  " total size is "<< length << " in words");
      if( lastId<el1id ) lastId=el1id;
      time=evdone->start();
      thistime=evdone->complete();
      // the time for the event to be fully collected
      elapsed=std::chrono::duration_cast<std::chrono::microseconds>
	(thistime-time);
      m_timeComplete_hist->Fill(elapsed.count());
      thistime=
	std::chrono::high_resolution_clock::now();
      // the time for the event to be fully processed
      elapsed=std::chrono::duration_cast<std::chrono::microseconds>
	(thistime-time);
      m_timeProcess_hist->Fill(elapsed.count());
      m_nFrags_hist->Fill(count);
      return true;
    } else return false;
  } else {
    // check for stale entries every so often
    thistime=
      std::chrono::high_resolution_clock::now();
    std::queue<uint64_t>  restore;
    m_NPending_hist->Fill(m_events.size());
    while (m_l1ids.try_pop(el1id) ) {
      // don't consider an event timed out unless it is prior to
      // a completed event since long term buffering occurs in the
      // robinnp
      if( el1id < lastId) {
	EventList::const_accessor m_eventsLocator;
	if(m_events.find(m_eventsLocator,el1id)) {
	  if( !timeout ) {
	    builtEv * const & ev=(m_eventsLocator)->second;
	    time=ev->start();
	    elapsed=std::chrono::duration_cast<std::chrono::microseconds>
	      (thistime-time);
	    // check that this has timed out & has at least one link & less then
	    //all links
	    if((elapsed>limit && ev->count()<m_nactive && ev->count()>0)
	       && !timeout) {
	      timeout=true;
	      l1id_timedOut=el1id;
	    }
	  }
	  // save the order and all but the timedout l1id for restoring
	  if( el1id != l1id_timedOut || !timeout ) restore.push(el1id);
	}
      } else restore.push(el1id);
    }
    // put all but the ones cleared from the event list back
    while (!restore.empty()){
      m_l1ids.push(restore.front());
      restore.pop();
    }
  }
  // update the count in case a fragment came in on a timeout
  EventList::accessor m_eventsLocator;
  if(timeout && m_events.find(m_eventsLocator,l1id_timedOut)) {
    builtEv * & ev=(m_eventsLocator)->second;
    length=ev->size();
    count=ev->count();
    // make sure not to report a complete event here
    if( count >= m_nactive ){
      if(DebugMe) ERS_LOG(" event completed while processing timeout");
      return false;
    }
    roi_data=ev->data();
    l1id=roi_data[5];
    linkList=ev->links();
    time=ev->start();
    elapsed=std::chrono::duration_cast<std::chrono::microseconds>
      (thistime-time);
    ERS_LOG(" event with lvl1id:"<<l1id<<" timed out "<<
	    elapsed.count());
    ERS_LOG(" call to getnext returns true with " <<count << 
	    " fragments");
    m_nFrags_hist->Fill(count);
    m_timeout_hist->Fill(elapsed.count());
    std::set<uint32_t> missingLinks=m_active_chan;
    for (uint32_t k=0;k<count;k++) 
      {
	auto i=linkList[k];
	auto loc=missingLinks.find(i);
	if(loc != missingLinks.end() ) missingLinks.erase(loc);
      }
    for( auto i:missingLinks) m_missedLink_hist->Fill(i);
    std::string linksin;
    for (uint32_t i=0;i<count;i++) 
      linksin+=
	static_cast<std::ostringstream*>( &(std::ostringstream() << linkList[i]))->str()+" ";
    ERS_LOG("links:"<<linksin.c_str()<<"reported"); 
    if( DebugData){
      ERS_LOG(" data dump:");
      for (uint32_t j=0;j<length;j++) 
	ERS_LOG(j<<":"<<std::hex<<roi_data[j]);
    }
    return true;
  } else return false;
}

