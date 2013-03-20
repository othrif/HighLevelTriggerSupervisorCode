
#include "../src/L1Source.h"

#include <stdlib.h>
#include <string>

#include "dynlibs/DynamicLibrary.h"
#include "config/Configuration.h"
#include "../src/LVL1Result.h"

#include <iostream>

int main(int argc, char *argv[])
{
    std::string source_type("internal");
    std::string db("oksconfig:test_source.data.xml");

    if(argc > 2) {
        db = argv[2];
    }

    if(argc > 1) {
        source_type = argv[1];
    }

    std::cout << "Using source type: " << source_type << std::endl;

    std::string libname("libsvl1");
    libname += source_type;

    try {

        DynamicLibrary lib(libname);

        Configuration config(db);

        if(getenv("TDAQ_PARTITION") == 0) {
            putenv("TDAQ_PARTITION=test_source");
        }

        if(hltsv::L1Source::creator_t maker = lib.function<hltsv::L1Source::creator_t>("create_source")) {
            hltsv::L1Source *source = maker(source_type, config);

            std::cout << "Created new L1Source" << std::endl;

            // once per configuration
            source->preset();

            // once per run ?
            source->reset();

            for(int i = 0; i < 1000; i++) {
                if(hltsv::LVL1Result *event = source->getResult()) {
                    std::cout << "Got event: "
                              << "L1ID: " << event->l1_id() << std::endl;
                    
                    
                } else {
                    std::cout << "No more events after " << i << " have been read" << std::endl;
                    break;
                }
            }

            delete source;

            lib.release();

        } else {
            std::cerr << "create_source function not found in loaded library";
            exit(1);
        }
    } catch(ers::Issue& ex) {
        std::cerr << ex << std::endl;
    }


}
