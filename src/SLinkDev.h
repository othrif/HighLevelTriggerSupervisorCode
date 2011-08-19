//  SLinkDev.h
//
// $Id: SLinkDev.h 27483 2006-06-23 14:26:21Z jls $

#ifndef __SLINKDEV_H__
#define __SLINKDEV_H__

#include "ttcpr/PCIDevice.h"
#include "ttcpr/s5933.h"


typedef enum {
  SLINK_FREE,
  SLINK_WAITING_FOR_SYNC,
  SLINK_REJECT,
  SLINK_RECEIVING,
  SLINK_SENDING,
  SLINK_NEED_OF_NEW_PAGE,
  SLINK_FINISHED,

  /* Be careful, inserts new codes before this one */
  SLINK_INSERT_NEW_STATUS_CODES_BEFORE_THIS_ONE
} SLINK_status_types;

const unsigned int SLINK_CONTROL_WORD = 0;
const unsigned int SLINK_DATA_WORD = 1;

const int SLINK_POOL_SIZE = 0x10000;

class CMemory;
class RingBuffer;


class SLinkDev : public PCIDevice {

 public:
    // Constructor/Destructor
  SLinkDev(const int& buffers,
	   const std::string& name,
	   const int& in,
	   const unsigned int& vid,
	   const unsigned int& did,
	   const unsigned int& cmd);
  virtual ~SLinkDev();

    // Functions
    virtual void control_words( unsigned long start,
                                unsigned long stop );

    virtual void* io_buffer();
    virtual int  io_size() const;
    virtual void io_init(const int& size) = 0;
    virtual bool io_status() = 0;
    virtual void reset(void) = 0;

 protected:
    // Variables
    volatile s5933_regs*  m_regs;

    CMemory*              m_cmem;
    RingBuffer*           m_rb;
    int                   m_size;

    int                   m_count;
    int                   m_state;
    unsigned long         m_start_word;
    unsigned long         m_stop_word;
 
};

#endif // __SLINKDEV_H__
