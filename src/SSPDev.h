//  SSPDev.h
//
// $Id: SSPDev.h 27483 2006-06-23 14:26:21Z jls $

#ifndef __SSPDEV_H__
#define __SSPDEV_H__

#include "SLinkDev.h"

const std::string SSP_NAME("SSP1");        
const unsigned int SSP_PCI_VID  = 0x10dc;
const unsigned int SSP_PCI_DID  = 0x0011;
const unsigned int SSP_PCI_CMD = 0x0146;

const unsigned int SSP_FIFO_RESET = 0x01000006;
const unsigned int SSP_TYPE_MASK = 0x03000000;
const unsigned int SSP_CONTROL_MASK = 0x01000000;
const unsigned int SSP_DATA_MASK = 0x03000000;
const unsigned int SSP_CONTROL = 0x01000007;
const unsigned int SSP_DATA = 0x03000007;


class SSPDev : public SLinkDev {

 public:
  // Constructor/Destructor
  SSPDev(const int& buffers = 1,
	   const int& in = 1,
           const std::string& name = SSP_NAME,
           const unsigned int& vid = SSP_PCI_VID,
           const unsigned int& did = SSP_PCI_DID,
           const unsigned int& cmd = SSP_PCI_CMD);
  ~SSPDev();

  // Functions
  void reset(void);
  void io_init(const int&size);
  bool io_status();

  long read_one();
  int word_type()
    { return m_type_of_word; }

 protected:
  // Variables
  int m_type_of_word;
  unsigned int m_expected;

};

#endif // __SSPDEV_H__
