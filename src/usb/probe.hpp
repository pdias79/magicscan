#pragma once
#include <string>
namespace magicscan {
struct ProbeResult {
    bool found = false;
    bool nova = false;
    std::string path;
    std::string error;
};
ProbeResult probe_scanner();
}
