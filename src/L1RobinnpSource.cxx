// Othmane Rifki & R. Blair
// othmane.rifki@cern.ch

#include <deque>
#include <vector>
#include <cstdlib>

#include "eformat/util.h"

#include "eformat/eformat.h"
#include "eformat/write/eformat.h"
#include <sys/syscall.h>
#include <sys/types.h>
#include "ers/ers.h"
#include "LVL1Result.h"
#include "eformat/Issue.h"

#include "Issues.h"

#include "L1Source.h"

#include "RoIBuilder.h"


#include "DFdal/RoIBPluginRobinnp.h"
#include "RunControl/FSM/FSMCommands.h"
const bool DebugMe=false;
const bool DebugData=false;

namespace hltsv {

  /**
   * The L1RobinnpSource class encapsulates the version of the
   * L1Source class which provides a LVL1Result object 
   * from a number of LVL1 inputs fed to a CRORC  device.
   */

  class L1RobinnpSource : public L1Source {
  public:
    L1RobinnpSource(const std::vector<uint32_t>& channels);
    ~L1RobinnpSource();
    
    virtual LVL1Result* getResult() override;
    virtual void        reset(uint32_t run_number) override;
    virtual void        preset() override;
    ROS::RobinNPROIB * m_input;

  private:

    size_t m_rols;
    RoIBuilder * m_builder;
    std::vector<uint32_t> m_active_chan;
  };
}

extern "C" hltsv::L1Source *create_source(Configuration *config, const daq::df::RoIBPlugin *roib, const std::vector<std::string>& /* unused */)
{
 const daq::df::RoIBPluginRobinnp *my_config = config->cast<daq::df::RoIBPluginRobinnp>(roib);
  if(my_config == nullptr) {
    throw hltsv::ConfigFailed(ERS_HERE, "Invalid type for configuration to L1RobinnpSource");
  }
  return new hltsv::L1RobinnpSource(my_config->get_Links());
}

namespace hltsv {

  //______________________________________________________________________________
  L1RobinnpSource::L1RobinnpSource(const std::vector<uint32_t>& channels)
    : m_rols(0),
      m_input(0),
      m_builder(0)
  {

  static bool once=false;
  if(!once){
  pid_t myID=syscall(SYS_gettid);
  ERS_LOG(" L1RobinnpSource thread started with id:"<<myID);
  once = true;
  }

    m_active_chan=channels;
    m_rols=channels.size();

  }
  //____________________________________________
  L1RobinnpSource::~L1RobinnpSource()
  {
    try{
      if ( m_builder ) {
	delete m_builder;
	m_builder=0;
      } else ERS_LOG(" no builder present");
      if( m_input ) {
	m_input->abort();
	m_input->stop();
	delete m_input;
	m_input=0;
      } else ERS_LOG(" no input present");
    }
    catch(...) ERS_LOG(" failed in delete of L1RobinnpSource"<<std::endl)
  }

  //______________________________________________________________________________
  LVL1Result* L1RobinnpSource::getResult()
  {


  // Thread and CPU Ids
	unsigned int pa, pb, pc, pd, pbb;
  __asm__("cpuid" : "=a" (pa), "=b" (pb), "=c" (pc), "=d" (pd) : "0" (1));
  pbb = pb >> 24;
  static bool once=false;
  if(!once){
  pid_t myID=syscall(SYS_gettid);
  ERS_LOG(" getResult thread started with id:"<<myID << " in CPU with id:"
		  << pbb );
  once = true;
  }



    LVL1Result* l1Result = nullptr;
    // lvl1_id is obvious
    // count is the number of fragments
    // roi_data is a pointer to the concatinated fragments
    // length is the uint32_t length of fragments
    uint32_t lvl1_id, count=0, length=0;
    uint32_t * roi_data;
    if(m_builder==0) {
      ERS_LOG("no builder present");
      return nullptr;
    }
    if (m_builder->getNext(lvl1_id,count,roi_data,length)){
      if(DebugMe) ERS_LOG("processing event:"<<lvl1_id<<" fragment count:"<<
			  count);
      if (count==m_rols ){
	try {
	  // make copies for processing so the input areas can be released
	  l1Result = new LVL1Result(roi_data,length);
	  m_builder->release(lvl1_id);
	} 
	catch(...) {
	  ERS_LOG(" failed to create LVL1Result from this data");
	  ERS_LOG(" data dump:"<<" lvl1_id:"<<lvl1_id);
	  for(uint32_t j=0;j<length;j++) ERS_LOG(j<<":"<<std::hex<<roi_data[j]);
	  l1Result=nullptr;
	}
      } else {
	// here we have timeouts and maybe other potential screwups
	if( count >0 )
	  {
	    ERS_LOG("found only "<<count<<" RoI links for this event, lvl1id:"<<
		    lvl1_id);
	    try {
	      l1Result = new LVL1Result(roi_data,length);
	      m_builder->release(lvl1_id);
	      if(DebugData) {
		ERS_LOG(" data dump:"<<" lvl1_id:"<<lvl1_id);
		for(uint32_t j=0;j<length;j++) 
		  ERS_LOG(j<<":"<<std::hex<<roi_data[j]);
	      }
	    }
	    catch(...) {
	      ERS_LOG(" failed to create LVL1Result from this data, LVL1ID:"<<
		      lvl1_id);
	      ERS_LOG(" data dump:"<<" lvl1_id:"<<lvl1_id);
	      for(uint32_t j=0;j<length;j++) 
		ERS_LOG(std::dec<<j<<":"<<std::hex<<roi_data[j]<<" "<<std::dec);
	      l1Result=nullptr;
	    }
	  } else {
	  try{
	    l1Result=nullptr;
	    m_builder->release(lvl1_id);
	    delete roi_data;
	    ERS_LOG(" failed to find any links from this data, lvl1id:"<<lvl1_id);
	    if(DebugData) { 
	      ERS_LOG(" data dump:"<<" lvl1_id:"<<lvl1_id);
	      // this is a serious error - likely the below is broken
	      for(uint32_t j=0;j<length;j++) 
		ERS_LOG(std::dec<<j<<":"<<std::hex<<roi_data[j]<<" "<<std::dec);
	    }
	  }
	  catch(...){
	    ERS_LOG(" no links from lvl1id:"<<lvl1_id);
	  }
	}
      }
    } else {
      l1Result=nullptr;
    }
    return l1Result;
  }
  
  //___________________________________________________________________________
  void   L1RobinnpSource::preset()
  {
    if(m_active_chan.size() == 0) {
      throw ConfigFailed(ERS_HERE, "No RobinNP input channels specified");
    }
    
    for(auto ch : m_active_chan) {
      if((uint)ch >= maxLinks) {
	throw ConfigFailed(ERS_HERE, "Invalid RobinNP channel");
      }
    }
    uint32_t m_maxchan=0;
    for ( auto i:m_active_chan ) m_maxchan=i>m_maxchan?i:m_maxchan;
    if(DebugMe) ERS_LOG(" running externally with "<<m_maxchan+1<<
			" channels");
    if ( m_builder ) {
      delete m_builder;
      m_builder=0;
    } else ERS_LOG(" no builder present");
    if( m_input ) {
      m_input->abort();
      m_input->stop();
      delete m_input;
      m_input=0;
    } else ERS_LOG(" no input present");
    m_input=new ROS::RobinNPROIB(0,m_maxchan+1,false,0);
    m_builder=new RoIBuilder(m_input,m_active_chan,m_maxchan+1);
  }
  //______________________________________________________________________________
  void
    L1RobinnpSource::reset(uint32_t /* run number */)
  {
    // configure (with protection against doing this twice)
    if( !m_builder->m_running) m_input->start();
    m_builder->m_running=true;
  }
}
