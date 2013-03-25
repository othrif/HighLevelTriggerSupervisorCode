
#include <stdlib.h>
#include <string>

#include "dynlibs/DynamicLibrary.h"

#include "../src/L1Source.h"
#include "../src/LVL1Result.h"

#include <iostream>

int main(int argc, char *argv[])
{
    std::string source_type("internal");
    std::string file("");

    if(argc > 1) {
        source_type = argv[1];
    }


    if(argc > 2) {
        file = argv[2];
    }

    std::cout << "Using source type: " << source_type << '\n'
              << "Using file: " << file << std::endl;

    std::string libname("libsvl1");
    libname += source_type;

    try {

        DynamicLibrary lib(libname);

        std::vector<std::string> files(1, file);

        if(hltsv::L1Source::creator_t maker = lib.function<hltsv::L1Source::creator_t>("create_source")) {
            hltsv::L1Source *source = maker(source_type, files);

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
