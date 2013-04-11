#include "config/Configuration.h"
#include "dal/Partition.h"
#include "DFdal/DFParameters.h"

#include "msg/Port.h"
#include "msgconf/MessageConfiguration.h"
#include "msginput/MessageHeader.h"

#include "cmdl/cmdargs.h"

#include "dcmessages/LVL1Result.h"
#include "RunController/ConfigurationBridge.h"
#include "RunController/Controllable.h"
#include "RunController/ItemCtrl.h"

#include "queues/ProtectedQueue.h"

#include "ipc/core.h"
#include "ers/ers.h"

#include "asyncmsg/NameService.h"

#include "DCMMessages.h"
#include "HLTSVSession.h"


class DCMActivity : public daq::rc::Controllable {
public:
  DCMActivity(std::string& name)
    : daq::rc::Controllable(name),
      m_work(m_dcm_io_service), m_running(false)
  {
  }

  ~DCMActivity()
  {
  }

  virtual void initialize(std::string & )
  {
  }

  virtual void configure(std::string& )
  {
    Configuration *conf = daq::rc::ConfigurationBridge::instance()->getConfiguration();

    const daq::core::Partition *partition = daq::rc::ConfigurationBridge::instance()->getPartition();
    IPCPartition part(partition->UID());
    const daq::df::DFParameters *dfparams = conf->cast<daq::df::DFParameters>(partition->get_DataFlowParameters());
    m_testns = new daq::asyncmsg::NameService(part, dfparams->get_DefaultDataNetworks());
    
  }

  virtual void unconfigure(std::string &)
  {
    m_msgconf.unconfigure();
    m_ports.clear();
    delete m_testns;
  }


  virtual void connect(std::string& )
  {
    
    // Read the HLTSV port using the name Service
    std::vector<boost::asio::ip::tcp::endpoint> hltsv_eps = m_testns->lookup("HLTSV");
    ERS_LOG("Found: " << hltsv_eps.size() << " endpoints");
    ERS_LOG("Found the port: " << hltsv_eps[0].port() );

    // Start ASIO Client
    ERS_LOG(" *** Start io_service ***");
    auto func = [&] () {
      ERS_LOG(" *** Run io_service ***");
      m_dcm_io_service.run(); 
      ERS_LOG(" *** io_service End ***");
    };
    boost::thread service_thread(func); 
    ERS_LOG(" *** installed service thread *** ");
    // create the session to talk to the HLTSV    
    m_session = std::make_shared<hltsv::HLTSVSession>(m_dcm_io_service);
    ERS_LOG(" *** created HLTSVSession *** ");
    m_session->asyncOpen("HLTSV", hltsv_eps[0]);
    
    //    std::vector<uint32_t> l1ids;
    //l1ids.push_back(3);
    //std::unique_ptr<const hltsv::RequestMessage> test_msg(new hltsv::RequestMessage(1,l1idss));
    //m_sessions[0]->onSend(test_msg);
    // Stop test

//     MessagePassing::Buffer announce(128);

//     MessageInput::MessageHeader header(0x1234, 0, MessagePassing::Port::self(),
// 				       MessageInput::MessageHeader::SIZE + sizeof(uint32_t));
                        
//     MessagePassing::Buffer::iterator it = announce.begin();
//     it << header << 8; // hard-coded number of "worker" processses

//     announce.size(it);
    
//     m_ports.front()->send(&announce, true);

  }
  virtual void stopL2SV(std::string &)
  {
    m_running = false;
  }

  void execute()
  {
    ERS_LOG(" *** enter execute() *** ");
    
//     using namespace MessagePassing;
//     using namespace MessageInput;
    
//     Buffer reply(128);
    
//     ERS_LOG(" *** entering run run loop (m_running = " << m_running << ") *** ");
//     while(m_running) {
      
//       if(Buffer *buf = Port::receive(100000)) {
	
// 	MessageHeader input(buf);
// 	if(!input.valid()) {
// 	  delete buf;
// 	  continue;
// 	}
	
// 	dcmessages::LVL1Result l1result(buf);
        
// 	delete buf;
	
// 	// ERS_LOG("Got event " << l1result.l1ID());
	
// 	MessageHeader header(0x8765U,
// 			     0, 
// 			     Port::self(), 
// 			     MessageHeader::SIZE + sizeof(uint32_t));
                
// 	Buffer::iterator it = reply.begin();
// 	it << header << l1result.l1ID();
	
// 	reply.size(it);
	
// 	if(Port *port = Port::find(input.source())) {
// 	  port->send(&reply, true);
// 	} else {
// 	  ERS_LOG("Invalid source node ID: " << input.source());
// 	}
	
// 	// ERS_LOG("Sent reply " << l1result.l1ID());
        
//       }
//     }
  }

    virtual void prepareForRun(std::string& )
  {
    ERS_LOG(" *** enter prepareForRun *** ");
    m_running = true;
    execute();
  }
  
  


  //*************************
  
private:
  MessageConfiguration   m_msgconf;
  daq::asyncmsg::NameService *m_testns;
  std::list<MessagePassing::Port*> m_ports;
  boost::asio::io_service m_dcm_io_service;
  boost::asio::io_service::work m_work;
  std::shared_ptr<hltsv::HLTSVSession> m_session;

  bool m_running;
  ProtectedQueue<dcmessages::LVL1Result*> m_queue;
  std::vector<boost::thread*>      m_handlers;
  tbb::atomic<uint32_t>            m_outstanding;
  uint32_t                         m_cores;
  
  uint32_t                         m_l2_processing;
  uint32_t                         m_l2_accept;
  uint32_t                         m_event_building;
  uint32_t                         m_event_filter;
};

int main(int argc, char *argv[])
{

  try{
    IPCCore::init(argc,argv);
  } catch(daq::ipc::Exception& ex) {
    ers::fatal(ex);
    exit(EXIT_FAILURE);
  }

  std::string name;
  std::string parent;
  bool interactive;
  
  CmdArgStr     app('N',"name", "name", "application name", CmdArg::isOPT);
  CmdArgStr     uniqueId('n',"uniquename", "uniquename", "unique application id", CmdArg::isREQ);
  CmdArgBool    iMode('i',"iMode", "turn on interactive mode", CmdArg::isOPT);
  CmdArgStr     segname('s',"segname", "segname", "segment name", CmdArg::isOPT);
  CmdArgStr     parentname('P',"parentname", "parentname", "parent name", CmdArg::isREQ);

  
  segname = "";
  parentname = "";
  
  CmdLine       cmd(*argv, &app, &uniqueId, &iMode, &segname, &parentname, NULL);
  CmdArgvIter   argv_iter(--argc, (const char * const *) ++argv);

  
  unsigned int status = cmd.parse(argv_iter);
  if (status) {
    cmd.error() << argv[0] << ": parsing errors occurred!" << std::endl ;
    exit(EXIT_FAILURE);
  }
  name = uniqueId;
  interactive = iMode;
  parent = parentname;
  
  daq::rc::ItemCtrl control(new DCMActivity(name), interactive, parent);

  control.run();

}
