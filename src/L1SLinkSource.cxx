
#include "dcmessages/LVL1Result.h"

#include "SSPDev.h"

#include "L1SLinkSource.h"

extern "C" hltsv::L1Source *create_source(const std::string&, Configuration& )
{
    return new hltsv::L1SLinkSource();
}

namespace hltsv {

    const int SIZE_BUFFER = 4096;
    const int BUFFERS = 16;

    const int L1HEADER = 0xb0f00000;
    const int L1TRAILER = 0xe0f00000;

    const int SIZE_HEADER = 9;  // Version 2.4
    const int SIZE_TRAILER = 3;


    L1SLinkSource::L1SLinkSource() :
        m_ssp(0),
        m_result(0)
    {
        m_ssp = new SSPDev(BUFFERS);

        // set expected control words
        m_ssp->control_words(L1HEADER, L1TRAILER);

        // store user buffer address
        m_result = reinterpret_cast<unsigned int*>(m_ssp->io_buffer());
    }

    L1SLinkSource::~L1SLinkSource()
    {
        // Close SLink device
        delete m_ssp;
    }

    dcmessages::LVL1Result*
    L1SLinkSource::getResult()
    {
        if(!m_ssp->io_status()) return 0;

        dcmessages::LVL1Result* l1Result =
            new dcmessages::LVL1Result( m_result, m_ssp->io_size()/sizeof(uint32_t) );

        // initialize next io
        m_result = reinterpret_cast<unsigned int*>(m_ssp->io_buffer());
        m_ssp->io_init(SIZE_BUFFER);

        return l1Result;
    }

    void
    L1SLinkSource::reset()
    {
        // reset device
        m_ssp->reset();

        // store user buffer address
        m_result = reinterpret_cast<unsigned int*>(m_ssp->io_buffer());

        // Initiate I/O
        m_ssp->io_init(SIZE_BUFFER);

    }
}
