
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
        bool restarted = false;

        for(int i = 1; i < argc; i++) {
            if(strcmp(argv[i], "--restart") == 0) {
                restarted = true;
                ERS_LOG("--restarted");
            }
        }

        IPCCore::init(argc,argv);
        daq::rc::CmdLineParser cmdline(argc, argv);

        daq::rc::ItemCtrl control(cmdline, std::make_shared<hltsv::Activity>(restarted));
        control.init();
        control.run();

    } catch(ers::Issue& ex) {
        ers::fatal(ex);
        exit(EXIT_FAILURE);
    }


}
