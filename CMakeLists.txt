
tdaq_package()

tdaq_generate_dal(schema/hltsv.schema.xml
  INCLUDE_DIRECTORIES dal monsvc DFConfiguration
  NAMESPACE daq::df
  INCLUDE DFdal
  CPP_OUTPUT dal_srcs)

tdaq_add_library(daq-hltsv-dal DAL ${dal_srcs}
  LINK_LIBRARIES monsvc-dal daq-df-dal daq-core-dal)

tdaq_add_library(hltsv
  src/L1Source.cxx 
  src/Activity.cxx 
  src/EventScheduler.cxx 
  src/DCMSession.cxx 
  src/ROSClear.cxx 
  src/UnicastROSClear.cxx 
  src/MulticastROSClear.cxx 
  src/ROSSession.cxx 
  src/HLTSVServer.cxx 
  src/LVL1Result.cxx 
  src/UpdateMessage.cxx 
  src/ProcessMessage.cxx 
  src/ClearMessage.cxx
  LINK_LIBRARIES TTCInfo_ISINFO trgCommander rc_ItemCtrl tdaq-common::ers TBB tdaq-common::eformat_write tdaq-common::eformat daq-hltsv-dal monsvcpub monsvc-dal asyncmsg dynlibs)

tdaq_add_executable(hltsv_main
  src/hltsv_main.cxx
  LINK_LIBRARIES hltsv)

tdaq_add_library(svl1internal MODULE
  src/L1InternalSource.cxx
  LINK_LIBRARIES trgCommander tdaq-common::ers daq-hltsv-dal)

tdaq_add_library(svl1preloaded MODULE
  src/L1PreloadedSource.cxx
  LINK_LIBRARIES trgCommander tdaq-common::ers tdaq-common::eformat_write tdaq-common::eformat tdaq-common::CTPfragment tdaq-common::DataReader daq-hltsv-dal)

tdaq_add_library(svl1filar MODULE
  src/L1FilarSource.cxx
  LINK_LIBRARIES trgCommander tdaq-common::eformat_write tdaq-common::eformat filar ROSMemoryPool daq-hltsv-dal)

tdaq_add_library(svl1ttc2lan MODULE
  src/L1TTC2LANSource.cxx
  LINK_LIBRARIES trgCommander tdaq-common::eformat_write tdaq-common::eformat asyncmsg daq-hltsv-dal)


