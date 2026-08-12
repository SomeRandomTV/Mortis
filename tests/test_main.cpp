#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "m0rtis_hub.hpp"
#include "m0rtis_node.hpp"
#include "tests.hpp"

void run_version_tests() {
    assert(std::strcmp(m0rtis::HUB_VERSION, "0.0.0") == 0);
    assert(std::strcmp(m0rtis::NODE_VERSION, "0.0.0") == 0);
}

namespace {

const std::vector<std::pair<std::string, void (*)()>> &suites() {
    static const std::vector<std::pair<std::string, void (*)()>> table = {
        {"version", run_version_tests},
        {"envelope", run_envelope_tests},
        {"message_type", run_message_type_tests},
        {"event_type", run_event_type_tests},
        {"framing", run_framing_tests},
        {"connection_state", run_connection_state_tests},
        {"fixture", run_fixture_tests},
        {"fixture_reader", run_fixture_reader_tests},
    };
    return table;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc > 1) {
        const std::string requested = argv[1];
        for (const auto &[name, run] : suites()) {
            if (name == requested) {
                run();
                std::cout << name << " passed." << std::endl;
                return 0;
            }
        }
        std::cerr << "Unknown test suite: " << requested << std::endl;
        return 1;
    }

    for (const auto &[name, run] : suites()) {
        run();
    }
    std::cout << "All tests passed." << std::endl;
    return 0;
}
