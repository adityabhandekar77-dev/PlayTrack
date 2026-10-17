
#ifndef APP_DISCOVERY_H
#define APP_DISCOVERY_H

#include "tracking_engine.h"
#include <filesystem>
#include <vector>

std::vector<Application> discoverApplications(
    const std::vector<std::filesystem::path>& folders
);

void mergeDiscoveredApplications(
    std::vector<Application>& existing,
    const std::vector<Application>& discovered
);

#endif
