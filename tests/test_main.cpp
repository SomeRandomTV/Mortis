#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "m0rtis_hub.hpp"
#include "m0rtis_node.hpp"
#include "tests.hpp"

/**
 * checks that both binaries agree on the protocol version constant -
 * about as basic as a test gets, but it's what's left of the original
 * (dead, unbuildable) test_main.cpp this whole suite replaced
 */
void run_version_tests() {
    assert(std::strcmp(m0rtis::HUB_VERSION, "0.0.0") == 0);
    assert(std::strcmp(m0rtis::NODE_VERSION, "0.0.0") == 0);
}

namespace {

/**
 * the name -> function dispatch table every suite gets registered in -
 * this is what lets main() run either everything at once (no args) or
 * exactly one named suite (argv[1]), which in turn is what lets
 * tests/CMakeLists.txt register each suite as its own separate ctest
 * entry instead of ctest only ever seeing one big binary
 *
 * @return the suite table, built once on first call and reused after
 */
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

/**
 * entry point for the m0rtis_tests binary. with no args, runs every
 * suite in the table above, back to back, in one process - useful for
 * a quick manual `./m0rtis_tests` sanity check. with a suite name as
 * argv[1], runs just that one suite and nothing else - this is the mode
 * ctest actually uses, one invocation per suite, so a crash in one
 * suite doesn't take the others down with it
 *
 * @return 0 if everything requested passed, 1 for an unrecognized
 *         suite name (an assert() failure elsewhere just aborts the
 *         process instead of returning)
 */
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
