//
// Created by Tom Arlt on 22.10.26.
//

#include "CameraCommunication.hpp"

#include <cstring>
#ifdef NATIVE_BUILD

namespace CameraCommunication{
    struct ReceivedPacket {
        uint32_t length;
        uint8_t data[1 + sizeof(ScanRequest)];
    };

}
#endif