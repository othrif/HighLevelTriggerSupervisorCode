//
//  $Id: L1FilarSource.cxx 47284 2008-03-14 13:36:20Z jls $
//

#include <deque>
#include <map>
#include <vector>

#include "eformat/write/ROBFragment.h"
#include "eformat/util.h"

#include "eformat/eformat.h"
#include "eformat/write/eformat.h"

#include "ers/ers.h"
#include "LVL1Result.h"
#include "eformat/Issue.h"

#include "rcc_error/rcc_error.h"
#include "ROSfilar/filar.h"

#include "Issues.h"

#include "ROSMemoryPool/MemoryPool_CMEM.h"
#include "ROSMemoryPool/MemoryPage.h"

#include "L1Source.h"

#include "DFdal/RoIBPluginFilar.h"

namespace hltsv {

  const int FILAR_PAGE_SIZE = 0x1000;

  /**
   * The L1FilarSource class encapsulates the version of the
   * L1Source class which provides a LVL1Result object 
   * from a buffer returned by an filar device.
   */

  class L1FilarSource : public L1Source {
  public:
    L1FilarSource(const std::vector<uint32_t>& channels);
    ~L1FilarSource();
    
    virtual LVL1Result* getResult() override;
    virtual void        reset(uint32_t run_number) override;
    virtual void        preset() override;

  private:

    void startIO();
    void pollIO();
    inline void nextChan();

    ROS::MemoryPool_CMEM * m_memPool;

    size_t m_rols;
    size_t m_buffers;
    size_t m_threshold;

    std::map<u_long, ROS::MemoryPage*> m_pciaddr_to_page;

    std::vector<u_int> m_active;
    std::vector<u_int>::iterator m_activeIter;

    std::vector<std::deque<ROS::MemoryPage*>*> m_available;
    std::vector<std::deque<ROS::MemoryPage*>*>::iterator m_availableIter;

    std::vector<std::deque<std::pair<u_int,ROS::MemoryPage*>>*> m_completed;
    std::vector<std::deque<std::pair<u_int,ROS::MemoryPage*>>*>::iterator m_completedIter;

  };
}

extern "C" hltsv::L1Source *create_source(Configuration *config, const daq::df::RoIBPlugin *roib, const std::vector<std::string>& /* unused */)
{
  const daq::df::RoIBPluginFilar *my_config = config->cast<daq::df::RoIBPluginFilar>(roib);
  if(my_config == nullptr) {
    throw hltsv::ConfigFailed(ERS_HERE, "Invalid type for configuration to L1FilarSource");
  }
  return new hltsv::L1FilarSource(my_config->get_Links());
}

namespace hltsv {

  const unsigned int L1HEADER = 0xb0f00000;
  const unsigned int L1TRAILER = 0xe0f00000;
  
  //______________________________________________________________________________
  L1FilarSource::L1FilarSource(const std::vector<uint32_t>& channels)
    : m_memPool(0),
      m_rols(0),
      m_buffers(0),
      m_threshold(1)
  {

    if(channels.size() == 0) {
      throw ConfigFailed(ERS_HERE, "No Filar input channels specified");
    }

    for(auto ch : channels) {
      if((int)ch >= MAXROLS) {
	throw ConfigFailed(ERS_HERE, "Invalid Filar channel");
      }
      m_active.push_back(ch);
      m_available.push_back( new std::deque<ROS::MemoryPage*> );
      m_completed.push_back( new std::deque<std::pair<u_int,ROS::MemoryPage*>> );
      m_rols++;
    }

    // make use of all the INFIFO entries
    m_buffers = MAXINFIFO;

    // buffer pool
    try {
      m_memPool = new ROS::MemoryPool_CMEM(m_buffers*m_rols, FILAR_PAGE_SIZE);
    }
    catch (ROSException& e) {
      ers::error(e);
    }

    // Open Filar device
    unsigned int code = FILAR_Open();
    if (code) {
      throw hltsv::FilarFailed(ERS_HERE,"open");
    }
    
  }

  //______________________________________________________________________________
  L1FilarSource::~L1FilarSource()
  {
    unsigned int code;
    // Close Filar device
    for (u_int j=0; j!=m_rols; j++)
      {
	code = FILAR_Reset(m_active[j]);
	if (code) {
	  throw hltsv::FilarFailed(ERS_HERE,"reset");
	}
	// and clean up queues
	delete m_available[j];
	delete m_completed[j];
      }

    code = FILAR_Close();
    if (code) {
      throw hltsv::FilarFailed(ERS_HERE,"close");
    }

    delete m_memPool;
  }

  //______________________________________________________________________________
  // start the io process
  void
  L1FilarSource::startIO()
  {
    // wait until buffer pool waiting to be processed is almost empty
    if ((*m_completedIter)->size() > m_threshold)
      return;

    FILAR_in_t fin;
    fin.nvalid = 0;
    fin.channel = (*m_activeIter);

    // any buffers available for this channel?
    while (!(*m_availableIter)->empty())
      {
	auto ptr = (*m_availableIter)->front();
	fin.pciaddr[fin.nvalid] = ptr->physicalAddress();
	fin.nvalid++;

	(*m_availableIter)->pop_front();
      }

    unsigned int code = FILAR_PagesIn(&fin);
    if (code) {
      throw hltsv::FilarDevException(ERS_HERE, code);
    }
  }

  //______________________________________________________________________________
  //  check if there are more events, 
  void
  L1FilarSource::pollIO()
  {
    // Check for I/O completion
    FILAR_out_t fout;

    unsigned int code = FILAR_Flush();
    if (code) {
      throw hltsv::FilarDevException(ERS_HERE, code);
    }

    code = FILAR_PagesOut(*m_activeIter, &fout);
    if (code) {
      throw hltsv::FilarDevException(ERS_HERE, code);
    }

    for (u_int i=0; i<fout.nvalid; i++)
      {
	auto page(m_pciaddr_to_page[fout.pciaddr[i]]);
	if (fout.fragstat[i])
	  {
	    // handle error
	    ERS_DEBUG(1, " Error processing L1ID " << std::hex
		      << reinterpret_cast<const uint32_t*>(page->address())[5]);

	    // discard result, make page available again;
	    (*m_availableIter)->push_back(page);
	  }
	else
	  // queue result for l1Result processing
	  (*m_completedIter)->push_back(std::make_pair(fout.fragsize[i], page));
      }
  }

  //______________________________________________________________________________
  inline void
  L1FilarSource::nextChan()
  {
    if (m_active.size() <= 1)
      return;

    // Update iterators for channel queues
    m_activeIter++;
    if (m_activeIter == m_active.end())
      {
	m_activeIter = m_active.begin();
	m_availableIter = m_available.begin();
	m_completedIter = m_completed.begin();
      }
    else
      {    
	m_availableIter++;
	m_completedIter++;
      }
  }
  
  //______________________________________________________________________________
  LVL1Result* L1FilarSource::getResult()
  {
    if ((*m_completedIter)->empty())
      pollIO();

    // if still no results, try next channel
    if ((*m_completedIter)->empty())
      {
	nextChan();
	startIO();
	return nullptr;
      }

    LVL1Result* l1Result = nullptr;

    auto ptr((*m_completedIter)->front());
    auto result = reinterpret_cast<const uint32_t*>((ptr.second)->address());
    auto size = ptr.first;

    try {
      // locate the ROD fragments
      const uint32_t* rod[MAXLVL1RODS];
      uint32_t        rodsize[MAXLVL1RODS];

      uint32_t num_frags = eformat::find_rods(result, size, rod, rodsize, MAXLVL1RODS);
      uint32_t size_word = 0;
      uint32_t lvl1_id = 0;

      // create the ROB fragments out of ROD fragments
      eformat::write::ROBFragment* writers[MAXLVL1RODS];
      for (size_t i = 0; i < num_frags; ++i) {
	writers[i] = new eformat::write::ROBFragment(const_cast<uint32_t*>(rod[i]), rodsize[i]);

	//update ROB header
	writers[i]->rob_source_id(writers[i]->rod_source_id());
	// m_len[i] = writers[i]->size_word();
	size_word += writers[i]->size_word();

	lvl1_id = writers[i]->rod_lvl1_id();
      }

      // make one single buffer out of the whole data
      uint32_t *event = new uint32_t[size_word];
      uint32_t current = 0;
      for (size_t i = 0; i< num_frags; ++i) {
	eformat::write::copy(*writers[i]->bind(), &event[current], writers[i]->size_word());
	current += writers[i]->size_word();
	delete writers[i];
      }

      l1Result = new LVL1Result( lvl1_id, event, size_word);

      ERS_DEBUG(3, "Created LVL1Result with l1id: " << l1Result->l1_id()
		<< " and size " << size );
    } catch (eformat::Issue &e) {
      ers::error(e); 
    }
        
    // initialize next io
    (*m_availableIter)->push_back(ptr.second);
    (*m_completedIter)->pop_front();

    nextChan();
    startIO();
        
    return l1Result;
  }

  //______________________________________________________________________________
  void
  L1FilarSource::reset(uint32_t /* run_number */)
  {
    // reset device

    // The upper bits of the channel are interpreted
    // as a card number. 
    for (u_int j=0; j!=m_rols; j++)
      {
	unsigned int code = FILAR_Reset(m_active[j]);
	if (code) {
	  throw hltsv::FilarDevException(ERS_HERE, code);
	}
      }

    // reset buffer pools
    m_memPool->reset();
    m_pciaddr_to_page.clear();

    for (u_int j=0; j!=m_rols; j++)
      {
	u_int code = FILAR_LinkReset(m_active[j]);
	if (code) {
	  throw hltsv::FilarDevException(ERS_HERE, code);
	}

	m_available[j]->clear();
	m_completed[j]->clear();

	for (u_int i=0; i!=m_buffers; i++)
	  {
	    ROS::MemoryPage* page = m_memPool->getPage();
	    m_available[j]->push_back(page);
	    m_pciaddr_to_page[page->physicalAddress()] = page;
	  }
      }

    m_activeIter = m_active.begin();
    m_availableIter = m_available.begin();
    m_completedIter = m_completed.begin();

    startIO();

  }

  //______________________________________________________________________________
  void
  L1FilarSource::preset()
  {
    FILAR_config_t fconfig;
 
    // configure device
    for(int c = 0; c < MAXROLS; c++)
      fconfig.enable[c] = 0; 

    for (u_int j=0; j!=m_rols; j++)
      fconfig.enable[m_active[j]] = 1;

    fconfig.psize = 4;
    fconfig.bswap = 0;
    fconfig.wswap = 0;
    fconfig.scw = L1HEADER;
    fconfig.ecw = L1TRAILER;
    fconfig.interrupt = 1;

    unsigned int code = FILAR_Init(&fconfig);
    if (code) {
      throw hltsv::FilarDevException(ERS_HERE, code);
    } else {
      // reset device
      // reset(0);
    }
  }
}
