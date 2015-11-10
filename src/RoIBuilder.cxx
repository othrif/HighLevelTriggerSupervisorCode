// Othmane Rifki, R. Blair, J. Love
// othmane.rifki@cern.ch, jeremy.love@cern.ch

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

builtEv::builtEv(uint64_t l1id64) :
  m_size(0),m_count(0),m_l1id64(l1id64),m_data(0)
{
  m_start=tbb::tick_count::now();
  m_data=new uint32_t[maxEvWords];
  memset(m_data,0,sizeof(uint32_t)*maxEvWords);
}

builtEv::~builtEv()
{
}

void builtEv::add(uint32_t w,uint32_t *d,uint32_t link,ROS::ROIBOutputElement &fragment) {
  auto size = w < maxSize ? w-1: maxSize;
  const uint32_t slot=m_size;
  // move the size then copy the data
  m_size += size;
  ::memcpy(&m_data[slot], d, sizeof(uint32_t)*size);
  m_links[m_count]=link;
  pages.push_back(fragment);
  m_count++;
}
void builtEv::finish() {
  m_complete=tbb::tick_count::now();
}
void  RoIBuilder::m_rcv_proc(uint32_t myThread)
{
  uint32_t Nwrap[]={0,0,0,0,0,0,0,0,0,0,0,0};
  uint32_t lastId[]={0,0,0,0,0,0,0,0,0,0,0,0};  
  uint32_t rolId;
  uint32_t MaxSubrob=(m_active_chan.size()>6?1:0);
  uint32_t subrob=myThread;
  uint64_t el1id;

  subrob=subrob%(MaxSubrob+1);

  pid_t myID=syscall(SYS_gettid);
  ERS_LOG(" Receive thread "<<myThread<<" started with id:"<<myID << 
		  " MaxSubrob="<<MaxSubrob<<" starting subrob:"<<subrob);

  if(DebugMe ) 
	ERS_LOG(" Receive thread started");

  // loop reading out until asked to stop
  while(!m_stop) {
    
    if(m_module == 0 ) 
      return;

    // the below is needed to avoid hanging on unconfig
    if( m_done.size()>=maxBacklog ) continue; 

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
    
    // If this is from an active channel add it to a map indexed by l1id
    if(m_active_chan.find(rolId) != m_active_chan.end())
      { 
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
	ev->add(fragment.dataSize,fragment.page->virtualAddress(),rolId, fragment);

	//...// 
	//...//WITH NEW TIME OUT SCHEME NO LONGER NECESSARY HANDLE BUILT EVENTS IN READ-IN THREAD
	//...//
	/*
	// fragment is built for l1id	
	if(ev->count()>=m_nactive) {
	  if(DebugMe) 
	    ERS_LOG("thread "<<myThread<<
		    " built lvl1id:"<<l1id);
	  // this will block until the list of done events drains
	  // below maxBacklog
	  ev->finish();
	  m_done.push(ev);
	  tbb::concurrent_vector<ROS::ROIBOutputElement> evPages= ev->associatedPages();
	  tbb::concurrent_vector<ROS::ROIBOutputElement>::iterator ipages= evPages.begin();
	  
	  for(; ipages!= evPages.end();++ipages){
	    m_module->recyclePage(*ipages);
	  }
	}//END BUILT EVENT HANDLING.
	*/

	if(newL1id) {
	  std::lock_guard<std::mutex> lock(m_result_mutex[subrob]); 
	  m_result_lists[subrob].push_back(el1id);
	}
      }  else {
      
      // if data comes in for links not in use throw it away 
      ERS_LOG("thread "<<myThread<<
	      " received data on unused link:"<<rolId);
      m_module->recyclePage(fragment);
      std::cout<<"I have a non-active channel recycling page!"<<std::endl;
    }
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
  std::string name="time_Build"; //"Time to complete";
  m_timeComplete_hist=
    new TH1D(name.c_str(),"assembly time;microseconds;",
	     100,0,60000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_timeComplete_hist);
  hist_names.insert(name);
  name="time_Process"; //"Time to process";
  m_timeProcess_hist=
    new TH1D(name.c_str(),"total processing time;microseconds;",100,0,60000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_timeProcess_hist);
  hist_names.insert(name);
  name="fragment_Count"; //"Fragment count";
  m_nFrags_hist=new TH1D(name.c_str(),"number of fragments;;",13,-.5,12.5);
  monsvc::MonitoringService::instance().register_object(dir+name,m_nFrags_hist);
  hist_names.insert(name);
  name="Pending_RNP"; //"Pending event count";
  m_NPending_hist=new TH1D(name.c_str(),"number of pending events;;",
			   100,0,4000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_NPending_hist);
  hist_names.insert(name);
  name="timeout"; //"time elapsed for timeouts";
  m_timeout_hist= new TH1D(name.c_str(),"elapsed time;microseconds;",
			   100,0,1000000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_timeout_hist);
  hist_names.insert(name);
  name="link_Missed"; //"link missed";
  m_missedLink_hist=new TH1D(name.c_str(),"missing links in timeouts;;",
			     12,0,12);
  monsvc::MonitoringService::instance().register_object(dir+name,m_missedLink_hist);
  hist_names.insert(name);
  // for now we spawn readout threads
  for (uint32_t i=0;i<n_rcv_threads;i++) {
    char histname[50];
    sprintf(histname,"links_read_thread%d",i); //"links read by thread %d"
    name =std::string(histname);
    m_readLink_hist[i]=new TH1D(histname,"channels read;;",
				maxLinks,-.5,maxLinks-.5);
    monsvc::MonitoringService::instance().register_object(dir+name,
							  m_readLink_hist[i]);
    hist_names.insert(name);
    sprintf(histname,"subrobs_read_thread%d",i); //"subrobs read by thread %d"
    name =std::string(histname);
    m_subrobReq_hist[i]=new TH1D(histname,"subrob requests;;",2,-.5,1.5);
    monsvc::MonitoringService::instance().register_object(dir+name,
							  m_subrobReq_hist[i]);
    hist_names.insert(name);
    m_rcv_threads.push_back(std::thread(&RoIBuilder::m_rcv_proc,this,i));
  }
  
  // spawn result checking thread
  m_result_thread =  std::thread(&RoIBuilder::check_results,this);
  
  name="Built_DAQ";//"Events complete and waiting for DAQ";
  m_backlog_hist=new TH1D(name.c_str(),"events in queue;;",110,0,10000);
  monsvc::MonitoringService::instance().register_object(dir+name,m_backlog_hist);
}

RoIBuilder::~RoIBuilder()
{
  m_running=false;
  m_stop=true;
  for (uint32_t i=0;i<m_rcv_threads.size();i++) m_rcv_threads[i].join();
  m_result_thread.join();
  if(DebugMe) ERS_LOG("Result and receive threads and their data cleaned up");
  for (EventList::iterator i=m_events.begin();i!=m_events.end();i++)
    delete i->second;
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
  tbb::tick_count time;
  tbb::tick_count thistime;
  tbb::tick_count::interval_t elapsed;
  
  m_backlog_hist->Fill(m_done.size());
  m_backlog = m_done.size();
  builtEv * evdone;
  if(m_done.try_pop(evdone)){
    length=evdone->size();
    count=evdone->count();
    roi_data=evdone->data();
    l1id=roi_data[5];
    el1id=evdone->l1id64();
    time=evdone->start();
    thistime=evdone->complete();
    // the time for the event to be fully collected
    elapsed=(thistime-time);
    m_timeComplete_hist->Fill(elapsed.seconds()*sectomicro);
    m_time_Build = elapsed.seconds()*sectomicro;
    thistime=tbb::tick_count::now();
    // the time for the event to be fully processed
    elapsed=(thistime-time);
    m_timeProcess_hist->Fill(elapsed.seconds()*sectomicro);
    m_time_Process = elapsed.seconds()*sectomicro;
    m_nFrags_hist->Fill(count);
    return true;
  }
  else
    return false;
  
}

void RoIBuilder::check_results( )
{
  
  pid_t myID=syscall(SYS_gettid);
  ERS_LOG(" Result handling thread started with id:"<<myID );

  
  tbb::tick_count time;
  tbb::tick_count thistime;
  tbb::tick_count::interval_t elapsed;

  const double limit = 1.0; // in seconds

  while(!m_stop){
    m_NPending_hist->Fill(m_events.size());
    m_NPending = m_events.size();
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // result checking every 10 ms
    // check results 

    for(size_t i = 0; i < NUMBER_OF_SUBROBS; i++) {
      
      auto & lst = m_result_lists[i];      
      while(!lst.empty()) {
	std::unique_lock<std::mutex> lock(m_result_mutex[i]);          
	
	auto el1id = lst.front();
	
	EventList::const_accessor m_eventsLocator;
	if(m_events.find(m_eventsLocator,el1id)) {
	  
	  builtEv * const & ev=(m_eventsLocator)->second;
	  time=ev->start();
	  thistime=tbb::tick_count::now();
	  elapsed=(thistime-time);
	  
	  // check that this has timed out & has at least one link & less then all links
	  if(ev->count() == m_nactive || //built
	     (elapsed.seconds() > limit ) //Or timed out events must be handled.
	     ){

	    // the below is needed to avoid hanging on unconfig
	    if( m_done.size()>=maxBacklog ) break;     

	    //move to built events queue
	    ev->finish();
	    m_done.push(ev);
	    lst.pop_front();
	    
	    if(elapsed.seconds() > limit && ev->count() < m_nactive){
	      m_timeout_hist->Fill(elapsed.seconds()*sectomicro);
	      m_timeout = elapsed.seconds()*sectomicro;
	    }
	    
	    tbb::concurrent_vector<ROS::ROIBOutputElement> evPages= ev->associatedPages();
	    tbb::concurrent_vector<ROS::ROIBOutputElement>::iterator ipages= evPages.begin();
	    
	    for(; ipages!= evPages.end();++ipages){
	      m_module->recyclePage(*ipages);
	    }
	  }
	  else
	    break; //no results to check.
	}
	else {
	  //entry not found
	  lst.pop_front();
	  continue;
	} 			
      }
    }
  }
}

void RoIBuilder::getISInfo(hltsv::HLTSV * info)
{

  // RoIB Statistics
  info->RoIB_Pending_RNP = m_NPending;
  info->RoIB_Pending_DAQ = m_backlog;
  info->RoIB_time_Build = m_time_Build;
  info->RoIB_time_Process = m_time_Process;
  info->RoIB_timeout = m_timeout;

  // RobinNP Statistics
  info->RNP_Most_Recent_ID.resize(maxLINKS);
  info->RNP_Free_pages.resize(maxLINKS);
  info->RNP_Used_pages.resize(maxLINKS);
  info->RNP_Most_Recent_ID.resize(maxLINKS);
  info->RNP_XOFF_state.resize(maxLINKS);
  info->RNP_XOFF_per.resize(maxLINKS);
  info->RNP_XOFF_count.resize(maxLINKS);
  info->RNP_Down_stat.resize(maxLINKS);
  info->RNP_bufferFull.resize(maxLINKS);
  info->RNP_numberOfLdowns.resize(maxLINKS);

  for(int i=0;i<maxLINKS;i++){
    m_rolStats = m_module->getRolStatistics(i);
    info->RNP_Most_Recent_ID[i] = m_rolStats->m_mostRecentId;
    info->RNP_Free_pages[i] = m_rolStats->m_pagesFree;
    info->RNP_Used_pages[i] = m_rolStats->m_pagesInUse;
    info->RNP_Most_Recent_ID[i] = m_rolStats->m_mostRecentId;
    info->RNP_XOFF_state[i] = m_rolStats->m_rolXoffStat;
    info->RNP_XOFF_per[i] = m_rolStats->m_xoffpercentage;
    info->RNP_XOFF_count[i] = m_rolStats->m_xoffcount;
    info->RNP_Down_stat[i] = m_rolStats->m_rolDownStat;
    info->RNP_bufferFull[i] = m_rolStats->m_bufferFull;
    info->RNP_numberOfLdowns[i] = m_rolStats->m_ldowncount;
  }
}

