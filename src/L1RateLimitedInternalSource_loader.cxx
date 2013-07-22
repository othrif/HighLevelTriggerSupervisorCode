#include "L1RateLimitedInternalSource.h"

#include "Issues.h"

extern "C" hltsv::L1Source *create_source(Configuration *config, const daq::df::RoIBPlugin *roib, const std::vector<std::string>& /* unused */)
{
    const daq::df::RoIBPluginRateLimitedInternal *my_config = config->cast<daq::df::RoIBPluginRateLimitedInternal>(roib);
    if(my_config == nullptr) {
        throw hltsv::ConfigFailed(ERS_HERE, "Invalid type for configuration to L1InternalRateLimitedSource");
    }
    return new hltsv::L1RateLimitedInternalSource(my_config);
}

