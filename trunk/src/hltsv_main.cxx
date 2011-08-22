
#include <string>

#include "dal/util.h"
#include "dal/Partition.h"
#include "sysmon/Resource.h"
#include "sysmonapps/ISThread.h"
#include "ac/parse_cmd.h"
#include "ac/AppControl.h"
#include "histmon/OHThread.h"

#include "Issues.h"
#include "Activity.h"


int
main(int argc, char** argv)
{
    cmdargs cmdLineArgs = parse_cmd("HLTSV Application", argc, argv);

    AppControl* ac = AppControl::get_instance(cmdLineArgs);
	
    hltsv::Activity hltsva;
    ISThread is_thread(AppControl::get_instance()->getNodeID());
    histmon::OHThread oh_thread(AppControl::get_instance()->getNodeID());

    try {

        if (ac->registerActivity(hltsva) != 0) {
            ers::fatal(hltsv::InitialisationFailure(ERS_HERE,
                                                   "Failed to register with AppControl"));
            daq::threads::Thread::sleep(500); //Make sure ers message is sent ??
            _exit(EXIT_FAILURE);
        }

        // register monitor threads and start AppControl
        if (ac->registerActivity(is_thread) != 0) {
            ers::error(hltsv::InitialisationFailure(
                                                   ERS_HERE, 
                                                   "Failed to register is_thread with AppControl"));
        }

        if (ac->registerActivity(oh_thread) != 0) {
            ers::error(hltsv::InitialisationFailure(
                                                   ERS_HERE,
                                                   "Failed to register oh_thread with AppControl"));
        }

        ac->run();

    } catch (ers::Issue& ex) {
        ers::fatal(ex);
        daq::threads::Thread::sleep(500); //Make sure ers message is sent ??
        _exit(EXIT_FAILURE);
    } catch (std::exception& ex) {
        ers::fatal(hltsv::InitialisationFailure(ERS_HERE, ex.what()));
        daq::threads::Thread::sleep(500); //Make sure ers message is sent ??
        _exit(EXIT_FAILURE);
    }

    /* Check the RC status to handle an exit from unexpected state,
     * i.e., not INITIAL.
     */
    daq::rc::States s;
    daq::rc::States::T_State state = ac->getState();
  
    if (state != daq::rc::States::INITIAL) {
        ERS_LOG("Use _exit to exit application as RC state after "
                << "exit from AppControl::run() is " << s.toString(state)
                << ". Expected RC state is INITIAL.");
        switch (state) {
        case daq::rc::States::ROIBSTOPPED:
        case daq::rc::States::RUNNING:
            { 
                std::ostringstream msg;
                Resource::get_all_resources(msg);
                ERS_LOG("" << cmdLineArgs.uniqueid
                        << ": Monitoring information  at unexpected exit  "
                        << ": \n" << msg.str().c_str());
            } 
            break;
        default:
            break;
        }
        daq::threads::Thread::sleep(500); //Make sure LOG message is sent ??
        _exit(EXIT_FAILURE);
    } else {
        ERS_LOG("Normal exit from AppControl::run() with RC status: " 
                << s.toString(state));
    }
    ac->unregisterActivity(&is_thread);
    ac->unregisterActivity(&hltsva);
    ac->unregisterActivity(&oh_thread);
    delete ac;
}
