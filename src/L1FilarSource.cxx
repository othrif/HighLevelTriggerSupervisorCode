//
//  $Id: L1FilarSource.cxx 47284 2008-03-14 13:36:20Z jls $
//

#include "eformat/write/ROBFragment.h"
#include "eformat/util.h"

#include "ers/ers.h"
#include "LVL1Result.h"
#include "eformat/Issue.h"

#include "rcc_error/rcc_error.h"
#include "ROSfilar/filar.h"

#include "Issues.h"
#include "L1FilarSource.h"

#include "ttcpr/CMemory.h"
#include "ttcpr/RingBuffer.h"

extern "C" hltsv::L1Source *create_source(const std::string& source, const std::vector<std::string>& /* unused */)
{
    return new hltsv::L1FilarSource(source);
}

namespace hltsv {

    const unsigned int L1HEADER = 0xb0f00000;
    const unsigned int L1TRAILER = 0xe0f00000;

    L1FilarSource::L1FilarSource(const std::string& source_type) :
        m_cmem(0),
        m_rb(0),
        m_chan(0)
    {
        // buffer pool
        m_cmem = new CMemory(FILAR_POOL_SIZE);

        m_rb = new RingBuffer(FILAR_BUFFER_COUNT,
                              FILAR_POOL_SIZE,
                              m_cmem->paddr(),
                              m_cmem->uaddr());

        // get channel number
        std::string channel = source_type;
	m_chan_mask=1;
        if (channel.length() > 5)
            {
                channel.erase(channel.begin(), channel.end()-1);
		// a bit of a kluge -> the last character is hex
		switch ((channel.c_str())[0]) {
		case 'A': m_chan_mask=10;
		  break;
		case 'B': m_chan_mask=11;
		  break;
		case 'C': m_chan_mask=12;
		  break;
		case 'D': m_chan_mask=13;
		  break;
		case 'E': m_chan_mask=14;
		  break;
		case 'F': m_chan_mask=15;
		  break;
		default:
		  m_chan_mask = atoi(channel.c_str());
		}
            }

        // Open Filar device
        unsigned int code = FILAR_Open();
        if (code) {
            throw hltsv::FilarFailed(ERS_HERE,"open");
        }
	bool chanset=false;
	// clear the m_result, m_size and m_active arrays
	for (int i_chan=0;i_chan<FILAR_MAX_LINKS;i_chan++){
	  m_active[i_chan]=( (m_chan_mask&((int)1<<i_chan)) != 0 );
	  m_result[i_chan]=0;
	  m_size[i_chan]=0;
	  if( !chanset && m_active[i_chan] ) {
	    chanset=true;
	    m_chan=i_chan;
	  }
	}
    }

    L1FilarSource::~L1FilarSource()
    {
      unsigned int code;
        // Close Filar device
      for (int i_chan=0;i_chan<FILAR_MAX_LINKS;i_chan++) {
	if( m_active[i_chan] ){
	  code = FILAR_Reset(i_chan);
	  if (code) {
	    throw hltsv::FilarFailed(ERS_HERE,"reset");
	  }
	}
      }

        code = FILAR_Close();
        if (code) {
            throw hltsv::FilarFailed(ERS_HERE,"close");
        }

        delete m_rb;
        delete m_cmem;

    }

    //  check if there are more events, 
    bool
    L1FilarSource::resultAvailable()
    {
        // Check for I/O completion
        FILAR_out_t fout;

        unsigned int code = FILAR_Flush();
        if (code) {
            throw hltsv::FilarDevException(ERS_HERE, code);
        }
	code = FILAR_PagesOut(m_chan, &fout);
	if (code) {
	  throw hltsv::FilarDevException(ERS_HERE, code);
	}

        if (fout.fragstat[0])
            {
                ERS_DEBUG(1, " Error processing L1ID " << std::hex << m_result[5]);

                // initialize next io
                m_result[m_chan] = reinterpret_cast<unsigned int*>(m_rb->reqVaddr());
                memset(m_result[m_chan], 0, m_rb->elemSize());
                FILAR_in_t fin;
                fin.nvalid = 1;      
                fin.channel = m_chan;
                fin.pciaddr[0] = reinterpret_cast<unsigned int>(m_rb->relPaddr());
                unsigned int code = FILAR_PagesIn(&fin);
                if (code) {
                    throw hltsv::FilarDevException(ERS_HERE, code);
                }
            }

        m_size[m_chan] = fout.fragsize[0];
        return (fout.nvalid != 0);
    }

    LVL1Result* L1FilarSource::getResult()
    {
        if(!resultAvailable()) return 0;

        FILAR_in_t fin;

        LVL1Result* l1Result = 0;

        try {
	  //            l1Result = new LVL1Result( m_result[m_chan], m_size[m_chan] );
                        // locate the ROD fragments
            const uint32_t* rod[MAXLVL1RODS];
            uint32_t        rodsize[MAXLVL1RODS];

            uint32_t num_frags = eformat::find_rods(m_result[m_chan], m_size[m_chan], rod, rodsize, MAXLVL1RODS);
            uint32_t size_word = 0;

            // create the ROB fragments out of ROD fragments
            eformat::write::ROBFragment* writers[MAXLVL1RODS];
            for (size_t i = 0; i < num_frags; ++i) {
                writers[i] = 
                    new eformat::write::ROBFragment(const_cast<uint32_t*>(rod[i]), rodsize[i]);

                //update ROB header
                writers[i]->rob_source_id(writers[i]->rod_source_id());
                // m_len[i] = writers[i]->size_word();
                size_word += writers[i]->size_word();
            }

            // make one single buffer out of the whole data
            uint32_t *event = new uint32_t[size_word];
            uint32_t current = 0;
            for (size_t i = 0; i< num_frags; ++i) {
                eformat::write::copy(*writers[i]->bind(), &event[current], writers[i]->size_word());
                current += writers[i]->size_word();
                delete writers[i];
            }

            l1Result = new LVL1Result( 0, event, size_word);
            

            ERS_DEBUG(3, "Created LVL1Result with l1id: " << l1Result->l1ID()
                      << " and size " << m_size[m_chan] );
        } catch (eformat::Issue &e) {
            ers::error(e); 
        }
        
        // initialize next io
        m_result[m_chan] = reinterpret_cast<unsigned int*>(m_rb->reqVaddr());
        memset(m_result[m_chan], 0, m_rb->elemSize());

        fin.nvalid = 1;      
        fin.channel = m_chan;
        fin.pciaddr[0] = reinterpret_cast<unsigned int>(m_rb->relPaddr());
        unsigned int code = FILAR_PagesIn(&fin);
        if (code) {
            throw hltsv::FilarDevException(ERS_HERE, code);
        }
	// move on to the next active channel
	for (int i=0;i<FILAR_MAX_LINKS;i++) {
	  m_chan++;
	  m_chan=m_chan%FILAR_MAX_LINKS;
	  if( m_active[m_chan] ) break;
	}
        
        return l1Result;
    }

    void
    L1FilarSource::reset()
    {
        FILAR_in_t fin;
	for (int c=0;c<FILAR_MAX_LINKS;c++) if(m_active[c]){
	  // reset device
	  unsigned int code = FILAR_Reset(c);
	  if (code) {
            throw hltsv::FilarDevException(ERS_HERE, code);
	  }

	  code = FILAR_LinkReset(c);
	  if (code) {
            throw hltsv::FilarDevException(ERS_HERE, code);
	  }

	  // store user buffer address
	  m_result[c] = reinterpret_cast<unsigned int*>(m_rb->reqVaddr());
	  memset(m_result[c], 0, m_rb->elemSize());
	  
	  fin.nvalid = 1;      
	  fin.channel = c;
	  fin.pciaddr[0] = reinterpret_cast<unsigned int>(m_rb->relPaddr());
	  code = FILAR_PagesIn(&fin);
	  if (code) {
            throw hltsv::FilarDevException(ERS_HERE,  code);
	  }
	}
    }

    void
    L1FilarSource::preset()
    {
        FILAR_config_t fconfig;
 
        // reset device
        for(int c = 0; c < MAXROLS; c++)
	  fconfig.enable[c] = m_active[c]?1:0; 

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
            reset();
        }

    }
}
