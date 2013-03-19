#include "config/Configuration.h"

#include "cmdl/cmdargs.h"

#include "RunController/ConfigurationBridge.h"
#include "RunController/Controllable.h"
#include "RunController/ItemCtrl.h"

class ROSApplication : public daq::rc::Controllable {
public:
    ROSApplication(std::string& name)
        : daq::rc::Controllable(name), m_running(false)
    {
    }
    ~ROSApplication()
    {
    }

    virtual void initialize(std::string & ) override
    {
    }

    virtual void configure(std::string& ) override
    {
        Configuration *config = daq::rc::ConfigurationBridge::instance()->getConfiguration();

        // get DFParameters from partition object, get data networks
    }

    virtual void unconfigure(std::string &) override
    {
    }
    
    virtual void connect(std::string& ) override
    {
    }

    virtual void stopL2SV(std::string &) override
    {
        m_running = false;
    }

    virtual void prepareForRun(std::string& ) override
    {
        m_running = true;
    }
  
  
  
private:
    bool m_running;
    
};

int main(int argc, char *argv[])
{
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
  
    daq::rc::ItemCtrl control(new ROSApplication(name), interactive, parent);
    control.run();
}
