
#include "ipc/core.h"

#include "RunControl/Common/Controllable.h"
#include "RunControl/Common/OnlineServices.h"
#include "RunControl/Common/CmdLineParser.h"
#include "RunControl/ItemCtrl/ItemCtrl.h"

#include "Issues.h"
#include "Activity.h"

#include <memory>

int main(int argc, char** argv)
{
    try{
        IPCCore::init(argc,argv);
        daq::rc::CmdLineParser cmdline(argc, argv);
        daq::rc::ItemCtrl control(cmdline, std::make_shared<hltsv::Activity>());
        control.run();

    } catch(ers::Issue& ex) {
        ers::fatal(ex);
        exit(EXIT_FAILURE);
    }


}
