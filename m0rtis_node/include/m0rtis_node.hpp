#pragma once

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "m0rtis_proto/envelope.hpp"

#ifndef FIXTURE_INFERENCE_EVENTS_PATH
#error "FIXTURE_INFERENCE_EVENTS_PATH must be defined by the build (see m0rtis_node/CMakeLists.txt)"
#endif

namespace m0rtis {

    //  Protocol Version
    inline constexpr const char* NODE_VERSION = "0.0.0";


    /* ========== DO NOT TOUCH UNTIL I.R.I.S. integration ========== */
    // Placeholder event source until real I.R.I.S. integration exists.
    // Reads test_files/inference_events.json, keeps only inference_event
    // entries, and hands them back one at a time, wrapping to the start
    // once exhausted.
    inline Envelope next_fixture_inference_event() {
        static const std::vector<Envelope> events = [] {
            std::ifstream in(FIXTURE_INFERENCE_EVENTS_PATH);
            if (!in.is_open()) {
                throw std::runtime_error(
                    "next_fixture_inference_event: could not open " FIXTURE_INFERENCE_EVENTS_PATH);
            }

            std::vector<Envelope> loaded;
            std::string line;
            while (std::getline(in, line)) {
                if (line.empty()) {
                    continue;
                }
                Envelope env = nlohmann::json::parse(line).get<Envelope>();
                if (env.type == MessageType::InferenceEvent) {
                    loaded.push_back(std::move(env));
                }
            }
            if (loaded.empty()) {
                throw std::runtime_error(
                    "next_fixture_inference_event: no inference_event entries found in fixture");
            }
            return loaded;
        }();


        static size_t index = 0;
        const Envelope &next = events[index];
        index = (index + 1) % events.size();
        return next;
    }

    /* ========== END-OF-DO-NOT-TOUCH ==========*/

    class MortisNode {
        
        public:

            const char *_HOST;
            unsigned _PORT;
            uint8_t VNODE_ID;
            
            MortisNode(const char *HOST, unsigned PORT, uint8_t ID) : _HOST(HOST), _PORT(PORT), VNODE_ID(ID) {}

            int connect_hub();      // build envelope send to hub 
            void event_loop();      // runs indefinately sending inference events + heartbeats 
            int node_shutdown();

        private:
            int sock;

    };

}   // namespace m0rtis
