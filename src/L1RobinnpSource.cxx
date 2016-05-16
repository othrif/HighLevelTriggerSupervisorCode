
// R. Blair, J. Love, & O. Rifki
//jeremy.love@cern.ch

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

#include "dal/Component.h"
#include "dal/Partition.h"
#include "dal/Segment.h"
#include "DFdal/DFParameters.h"
#include "DFdal/DataFile.h"
#include "DFdal/RoIBConfiguration.h"
#include "DFdal/RoIBInputBoard.h"
#include "DFdal/RoIBInputChannel.h"
#include "rc/RunParams.h"
#include "dal/Partition.h"
#include "dal/MasterTrigger.h"
#include "DFdal/DFParameters.h"
#include "RunControl/Common/OnlineServices.h"

#include "Issues.h"

#include "L1Source.h"

#include "RoIBuilder.h"
#include "HLTSV.h"

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
    L1RobinnpSource(const std::vector<uint32_t>& channels, uint32_t timeout, uint64_t sleep, double fraction, bool isEmu);
    ~L1RobinnpSource();
    
    virtual LVL1Result* getResult() override;
    virtual void        reset(uint32_t run_number) override;
    virtual void        preset() override;
    ROS::RobinNPROIB * m_input;
	virtual void getMonitoringInfo(HLTSV *info) override;

  private:

    size_t m_rols;
    float m_timeout;
    uint64_t m_sleep;
    double m_fraction;
    bool m_emulated;
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

  std::vector<uint32_t> links;
  Configuration& conf=daq::rc::OnlineServices::instance().getConfiguration();
  // Load configuration parameters
  // start with roib inputs
  const daq::core::Partition & partition = 
    daq::rc::OnlineServices::instance().getPartition();
  const daq::core::Segment & segment = 
    daq::rc::OnlineServices::instance().getSegment();
  std::vector<const daq::core::ResourceBase*> rb = segment.get_Resources();
  if( rb.size() == 0 ) {
    ERS_LOG(" no resources found to explore for RoIBInputs");
  }
  else for (std::vector<const daq::core::ResourceBase*>::iterator it = 
              rb.begin(); it != rb.end(); it++) {
      const daq::core::ResourceSet* rit = conf.cast<daq::core::ResourceSet>(*it);
      if (rit && !rit->disabled(partition)) {
        // Inputs from L1Source
        try {
          //input loop
          if (rit->UID() == "RoIBInput") {
            std::vector<const daq::core::ResourceBase*> inputChannels =
              rit->get_Contains();
            for (std::vector<const daq::core::ResourceBase*>::iterator ic = inputChannels.begin();
                 ic != inputChannels.end(); ic++) {
              const daq::df::RoIBInputChannel* ric = conf.cast<daq::df::RoIBInputChannel>(*ic);
              if (ric && !ric->disabled(partition)) {
                //check if the L1source available and enabled
                const daq::core::ResourceBase* rb = ric->get_L1Source();
                if (rb && !rb->disabled(partition)) {
		  ERS_LOG(" Adding link "<<ric->get_ChannelID()<<
			  " from RoIBInput");
		  links.push_back(ric->get_ChannelID());
		}
              }
            }
          }
        } catch (daq::config::Exception& ex) {
          ers::error(ex);
        }
      }
    }
  /*//add any specific additional ones from the config
  for (auto i=0;i<(int32_t)(my_config->get_Links()).size();i++)  {
    ERS_LOG(" Adding link "<<my_config->get_Links().at(i));
    links.push_back((my_config->get_Links()).at(i));
    }*/
  // remove any repeats and order them
  std::sort(links.begin(),links.end());
  auto last=std::unique(links.begin(),links.end());
  auto oldend=links.end();
  links.erase(last,oldend);
  ERS_LOG(" total of "<<links.size()<<" unique links");

  return new hltsv::L1RobinnpSource(links, my_config->get_Timeout(), my_config->get_SleepTime(), my_config->get_SleepFraction(), my_config->get_DataEmulation());
}

namespace hltsv {

  //______________________________________________________________________________
  L1RobinnpSource::L1RobinnpSource(const std::vector<uint32_t>& channels, uint32_t timeout, uint64_t sleep, double fraction, bool isEmu)
    :m_input(0),
     m_rols(0),
     m_builder(0)
  {
	pid_t myID=syscall(SYS_gettid);
	ERS_LOG(" L1RobinnpSource thread started with id:"<<myID);
	m_active_chan=channels;
	m_rols=channels.size();
	m_sleep=sleep;
	m_fraction=fraction;
	m_emulated=isEmu;
	m_timeout=timeout;
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
    catch(...) {
	  ERS_LOG(" failed in delete of L1RobinnpSource"<<std::endl);
    }
  }

  //______________________________________________________________________________
  LVL1Result* L1RobinnpSource::getResult()
  {

    /*
	  static bool once=false;
	  if(!once){
	  pid_t myID=syscall(SYS_gettid);
	  ERS_LOG(" getResult thread started with id:"<<myID);
	  once = true;
	  }
    */

    LVL1Result* l1Result = nullptr;
    // lvl1_id is obvious
    // count is the number of fragments
    // roi_data is a pointer to the concatenated fragments
    // length is the uint32_t length of fragments
    uint32_t lvl1_id, count=0, length=0;
    uint32_t * roi_data;
    uint64_t elvl1id;
    if(m_builder==0) {
      ERS_LOG("no builder present");
      return nullptr;
    }
    if (m_builder->getNext(lvl1_id,count,roi_data,length,elvl1id)){
      if(DebugMe) ERS_LOG("processing event:"<<lvl1_id<<" fragment count:"<<
						  count);
      if (count==m_rols ){
		try {
		  // make copies for processing so the input areas can be released
		  l1Result = new LVL1Result(roi_data,length);
		  m_builder->release(elvl1id);
		} 
		catch(...) {
		  hltsv::L1ResultFail err(ERS_HERE);
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
		      m_builder->release(elvl1id);
		      if(DebugData) {
			ERS_LOG(" data dump:"<<" lvl1_id:"<<lvl1_id);
			for(uint32_t j=0;j<length;j++) 
			  ERS_LOG(j<<":"<<std::hex<<roi_data[j]);
		      }
		    }
		    catch(...) {
		      hltsv::L1ResultFail err(ERS_HERE);
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
			m_builder->release(elvl1id);
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

    uint32_t m_maxchan=m_active_chan.size();
    uint32_t linkMask = 0;
    for(auto ch : m_active_chan) {
      linkMask |= (1 << ch);
    }
    ERS_LOG("Mask of active channels: " << linkMask);
    
    if(DebugMe) ERS_LOG(" running externally with "<<m_maxchan<<
			" channels");
    //m_input=new ROS::RobinNPROIB(0,linkMask,true,0);//,256,4000);
    m_input=new ROS::RobinNPROIB(0,linkMask,m_emulated,0);
    
    m_builder=new RoIBuilder(m_input,m_active_chan);
    m_builder->setTimeout(m_timeout);
    m_builder->setSleep(m_sleep);
    m_builder->setFraction(m_fraction);
  }
  //______________________________________________________________________________
  void
  L1RobinnpSource::reset(uint32_t /* run number */)
  {
    // configure (with protection against doing this twice)
    if( !m_builder->m_running) m_input->start();
    m_builder->m_running=true;
  }
  //______________________________________________________________________________
  void L1RobinnpSource::getMonitoringInfo(HLTSV *info)
  {
	m_builder->getISInfo(info);
  }



}
