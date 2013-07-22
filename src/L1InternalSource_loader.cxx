#include "L1InternalSource.h"

#include "Issues.h"

extern "C" hltsv::L1Source *create_source(Configuration *config, const daq::df::RoIBPlugin *roib, const std::vector<std::string>& /* unused */)
{
    const daq::df::RoIBPluginInternal *my_config = config->cast<daq::df::RoIBPluginInternal>(roib);
    if (my_config == nullptr) {
        throw hltsv::ConfigFailed(ERS_HERE, "Invalid type for configuration to L1InternalSource");
    }

    return new hltsv::L1InternalSource(my_config);
}

