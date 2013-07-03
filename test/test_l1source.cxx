
#include <stdlib.h>
#include <string>
#include <vector>
#include <memory>
#include <array>

#include "config/Configuration.h"
#include "DFdal/RoIBPlugin.h"

#include "dynlibs/DynamicLibrary.h"

#include "../src/L1Source.h"
#include "../src/LVL1Result.h"

#include "eformat/util.h"
#include "eformat/ROBFragmentNoTemplates.h"

#include <iostream>

#include "ipc/core.h"


int main(int argc, char *argv[])
{
    using namespace std;

    if(argc < 3) {
        cerr << "Usage: " << argv[0] << " oks-database UID [ files ]\n"
             << "  where UID is the object ID of the RoIBPlugin" << std::endl;
        exit(EXIT_FAILURE);
    }

    IPCCore::init(argc, argv);

    // will throw on error
    Configuration config(string("oksconfig:") + argv[1]);

    string uid = argv[2];

    argc -= 3;
    argv += 3;

    vector<string> files;
    while(argc > 0) {
        files.push_back(argv[0]);
        argc--;
        argv++;
    }

    
    const daq::df::RoIBPlugin *roib = config.get<daq::df::RoIBPlugin>(uid);
    if(roib == nullptr) {
        cerr << "Object not found: " << uid << std::endl;
        exit(EXIT_FAILURE);
    }

    vector<unique_ptr<DynamicLibrary>> libs;

    try {

        for(auto& lib : roib->get_Libraries()) {
            libs.emplace_back(new DynamicLibrary(lib));
        }

        if(hltsv::L1Source::creator_t maker = libs.back()->function<hltsv::L1Source::creator_t>("create_source")) {
            unique_ptr<hltsv::L1Source> source(maker(&config, roib, files));

            std::cout << "Created new L1Source" << std::endl;

            // once per configuration
            source->preset();

            // once per run ?
            source->reset(1);

            for(int i = 0; i < 1000; ) {
                if(hltsv::LVL1Result *event = source->getResult()) {
                    std::cout << "Got event: "
                              << "L1ID: " << event->l1_id() << std::endl
                              << "Size: " << event->event_data_size() << " bytes" << std::endl;

                    unique_ptr<uint32_t[]> data(new uint32_t[event->event_data_size()]);
                    event->copy(data.get());

                    std::array<const uint32_t*,12> robs;

                    size_t num_robs = eformat::find_fragments(eformat::ROB,
                                                              data.get(), 
                                                              event->event_data_size()/sizeof(uint32_t),
                                                              &robs[0], robs.size());
                    
                    std::cout << "ROBs: " << num_robs << std::endl;
                    
                    for(size_t i = 0; i < num_robs; i++) {
                        eformat::read::ROBFragment fragment(robs[i]);
                        std::cout << "ROB[" << i << "] source id: " << hex << fragment.rob_source_id() << dec << std::endl
                                  << "ROB[" << i << "] size word: " << fragment.rod_fragment_size_word() << std::endl
                                  << "ROD[" << i << "] run number: " << fragment.rod_run_no() << std::endl
                                  << "ROD[" << i << "] lvl1 id: " << fragment.rod_lvl1_id() << std::endl
                                  << "ROD[" << i << "] bc id: " << fragment.rod_bc_id() << std::endl
                            ;
                    }
                    std::cout << std::endl;
                    i++;
                }
            }

        } else {
            std::cerr << "create_source function not found in loaded library";
            exit(EXIT_FAILURE);
        }
    } catch(ers::Issue& ex) {
        std::cerr << ex << std::endl;
        exit(EXIT_FAILURE);
    } catch(std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    return 0;

}
