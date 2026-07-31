#pragma once 

#include <string>
#include <optional>
#include <iostream>

/* This header describes the transistions between hub and Vision Node 
 *  
 *  === Hub STATES ===
 *  AWAITING_HANDSHAKE -> The hub is waiting for vnode to send a handshake 
 *      - HUB is listening for nodes to connect, the node is the one that initiates contact 
 *      - Anything other than sending the handshake will result in rejection
 *      - A NODE_SHUTDOWN is just gonna get rejected 
 *      - An IO_ERROR here would just be treated as an INVALID_HANDSHAKE 
 *
 *  ACTIVE -> The vnode is actively sending inference events and heartbeats to hub 
 *      - Anything other than those 2 are recieved - disconnect 
 *      - Malformed data is rejected and disconnect will occur (for now - data recovery? )
 *  DISCONNECTED -> Simple - das et -- now may need to add retries 
 *
 * namespace m0rtis::connection_state
 * 
 *  This is a shared library between V-Node and Hub so in the main namespace (connection_state here) will be the shared items 
 *
 *  namespace m0rtis::connection_state::vnode_state
 *      Contains all functions and shapes n shit for the V-Node state 
 *
 *  namespace m0rtis::connection_state::hub_state
 *      Same shit but for the hub state 
 */
namepsace m0rtis::connection_state {
    

    consteval const int HUB_TIMEOUT = 30;    // expect heartbeat from node, reset after receiving data from node 
    consteval const int NODE_HEARTBEAT = 5;  // send heart beat every 5 seconds after inference  

    enum class TRIGGERS {
        // ========== TO DISCONNECTED STATE ========== 
        INVALID_HANDSHAKE,      // V-Node VERSION or NODE ID are invalid 
        PROTOCOL_VIOLATION,     // V-Node sent data not valid in the current state 
        HANDSHAKE_TIMEOUT,      // V-Node did not send 
        HEARTBEAT_TIMEOUT,      // Hub did not receive V-Node  
        IO_ERROR,               // Malformed data recv or bad read  
        TCP_FAILURE,            // In-flight TCP connection failure  
        
        // =========== TO NEXT VALID STATE =========
        HANDSHAKE_VALID,        // Good Handshake
        INFERENCE_EVENT,        // Inference Event captured by the I.R.I.S.
        HEARTBEAT,              // Heartbeat sent by V-Node to Hub (Alive notification)
        NODE_SHUTDOWN           // Node is letting the hub disconnect is happening 
    }; 

    enum class CONNECTION_STATES {
        // ========== HUB States ==========
        AWAITING_HANDSHAKE,     // Hub is waiting on Handshake from V-Node


        // ========== Node States ==========
        CONNECTING,             // V-Node is waint for acceptance after handshake
        HANDSHAKE_SENT,         // 

        // ========== Shared States ==========
        ACTIVE,                 // connection between V-Node and Hub is on going 
        DISCONNECTED,           // V-Node and Hub are no longer communicating 
    

    };

    struct Transition {
        CONNECTION_STATES from;
        TRIGGERS trigger;
        CONNECTION_STATES next;
    };

    static constexpr Transition hub_t_table[] = {
        // during q0 
        { CONNECTION_STATES::AWAITING_HANDSHAKE, TRIGGERS::HANDSHAKE_VALID, CONNECTION_STATES::ACTIVE },
        { CONNECTION_STATES::AWAITING_HANDSHAKE, TRIGGERS::INVALID_HANDSHAKE, CONNECTION_STATES::DISCONNECTED },
        { CONNECTION_STATES::AWAITING_HANDSHAKE, TRIGGERS::PROTOCOL_VIOLATION, CONNECTION_STATES::DISCONNECTED },
        { CONNECTION_STATES::AWAITING_HANDSHAKE, TRIGGERS::HANDSHAKE_TIMEOUT, CONNECTION_STATES::DISCONNECTED },
        // during q1 
        { CONNECTION_STATES::ACTIVE, TRIGGERS::INFERENCE_EVENT, CONNECTION_STATES::ACTIVE },
        { CONNECTION_STATES::ACTIVE, TRIGGERS::HEARTBEAT, CONNECTION_STATES::ACTIVE },
        { CONNECTION_STATES::ACTIVE, TRIGGERS::HEARTBEAT_TIMEOUT, CONNECTION_STATES::DISCONNECTED },
        { CONNECTION_STATES::ACTIVE, TRIGGERS::IO_ERROR, CONNECTION_STATES::DISCONNECTED },
        { CONNECTION_STATES::ACTIVE, TRIGGERS::PROTOCOL_VIOLATION, CONNECTION_STATES::DISCONNECTED },
        { CONNECTION_STATES::ACTIVE, TRIGGERS::NODE_SHUTDOWN, CONNECTION_STATES::DISCONNECTED }
    };

    static constexpr Transition vnode_t_table[] = {
        // TODO: GET NODE STATE DOWN 

    };

    namespace vnode_state {
        // node connection state will go here  
        inline constexpr int connect_to_hub(connection_state::Transistion &t) {
            std::cout << "Connecting to hub ... \n";
            return 0;
        } 
    }

    namespace hub_state {
        // hub connection state will go here 
        inline constexpr int accept_vnode(connection_state::Transition &t) {
            std::cout << "Accepting V-Node ... \n";
            return 0;

        }
    }



}   // namepsace m0rtis::connection_state
