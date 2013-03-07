
#include <string>

#include "dal/util.h"
#include "dal/Partition.h"
#include "Issues.h"
#include "Activity.h"
#include "cmdl/cmdargs.h"

#include "RunController/Controllable.h"
#include "RunController/ConfigurationBridge.h"
#include "RunController/ItemCtrl.h"

int main(int argc, char** argv)
{
    try{
        IPCCore::init(argc,argv);
    } catch(daq::ipc::Exception& ex) {
        ers::fatal(ex);
        exit(EXIT_FAILURE);
    }

    //parse_args(argc,argv,name,parent,interactive);
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

    std::string name;
    std::string parent;
    bool interactive = iMode;

    name = uniqueId;
    parent = parentname;
  
    daq::rc::ItemCtrl control(new hltsv::Activity(name),interactive,parent);
    control.run();

    /* Check the RC status to handle an exit from unexpected state,
     * i.e., not INITIAL.
     */
    daq::rc::States s;
    daq::rc::States::T_State state = control.getState();

    if (state != daq::rc::States::INITIAL) {
        ERS_LOG("Exit from unexpected run control state: " << s.toString(state));
        _exit(EXIT_FAILURE);
    } else {
        ERS_LOG("Normal exit");
    }
}
