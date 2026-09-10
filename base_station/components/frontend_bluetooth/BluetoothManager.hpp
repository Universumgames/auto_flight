#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "host/ble_gatt.h"
#include "host/ble_uuid.h"

#include "BLETopics.hpp"
#include "FrontendPackets.hpp"

extern "C" void ble_store_config_init(void);

extern "C" {

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

}

class FrontendHandlerBlClass;

class BluetoothManager {
public:
    typedef uint8_t TopicType;

    struct Characteristic {
        TopicType topicID;
        const ble_uuid16_t* uuid;
        uint16_t val_handle;
    };
private:
    BluetoothManager();
    static const char* TAG_BLUETOOTH_MANAGER;
    friend FrontendHandlerBlClass;

    static void initFS();

    /* Advertised in the scan response's manufacturer-specific data (AD type 0xFF) so the
     * phone can see which firmware build a base station is running before connecting:
     * bytes [0:2) are the company ID (0xFFFF, the Bluetooth SIG's reserved "for testing"
     * value — this project has no assigned company ID), bytes [2:6) are
     * BUILD_EPOCH_TIMESTAMP (Unix seconds, from the generated build_timestamp.h — see
     * CMakeLists.txt/generate_build_timestamp.cmake — freshly stamped on every build) as
     * a little-endian uint32. The iOS app must mirror this exact layout when parsing. */
    static constexpr uint8_t BUILD_VERSION_MFG_DATA_LEN = 6;
    uint8_t buildVersionMfgData[BUILD_VERSION_MFG_DATA_LEN]{};
    void populateBuildVersionMfgData();

    /* C callback functions, call object instance methods */
    static void print_addr(const void *addr);
    static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg);
    static void ble_spp_server_print_conn_desc(struct ble_gap_conn_desc *desc);
    static void ble_spp_server_host_task(void *param);
    static int  ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg);
    static int  ble_svc_battery_handler(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg);

    const ble_uuid16_t new_ble_svc_spp_uuid = BLE_UUID16_INIT(CONFIG_BLUETOOTH_SERVICE_UUID);
    ble_gatt_chr_def new_ble_svc_gatt_chrs[BLETopics::ALL_NOTIFICATIONS_SIZE + 1] = {};

    /* Standard BLE Battery Service (0x180F), reporting the base station's own
     * battery via the standard Battery Level characteristic (0x2A19) so generic
     * BLE clients (not just this app) can read it. Unlike the custom SPP service
     * above, this uses plain single-byte (0-100) values with no application-level
     * framing, per the Bluetooth SIG GATT spec. */
    const ble_uuid16_t battery_svc_uuid = BLE_UUID16_INIT(0x180F);
    const ble_uuid16_t battery_level_chr_uuid = BLE_UUID16_INIT(0x2A19);
    ble_gatt_chr_def battery_svc_gatt_chrs[2] = {};
    uint16_t batteryLevelValHandle = 0;
    uint8_t batteryLevel = 0;

    ble_gatt_svc_def new_ble_svc_gatt_defs[3] = {};
    uint8_t own_addr_type{};
    bool conn_handle_subs[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1]{};

    int gatt_svr_init();

    void onResetCb(int reason);
    void onSyncCb();
    void onGattSvrRegisterCb(ble_gatt_register_ctxt *ctxt, void *arg);

    int onSPPGapEvent(ble_gap_event *event, void *arg);

    int onSVCGattHandler(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg);
    int onBatterySvcGattHandler(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg) const;

    std::vector<std::function<void(uint8_t*, int, TopicType)>> dataReceivedCallbacks;
    void callDataReceivedCallbacks(uint8_t* data, int len, uint16_t attr_handle) const {
        for (const auto& callback : dataReceivedCallbacks) {
            callback(data, len, characteristicsByHandle.at(attr_handle)->topicID);
        }
    }

    /* --- Application-level fragmentation ---
     * A single BLE notification/write is capped at (negotiated ATT MTU - 3).
     * To carry payloads larger than that (e.g. routes with many coordinates),
     * every notify/write is split into fragments, each prefixed with a 4-byte
     * little-endian header: [0:2) totalLength, [2:4) offset. The receiver
     * accumulates fragments per (conn_handle, attr_handle) until `offset +
     * chunkLen == totalLength`, then delivers the reassembled buffer.
     * The iOS app must mirror this exact framing on both directions. */
    static constexpr uint16_t FRAGMENT_HEADER_SIZE = 4;
    /* Fixed by the BLE ATT protocol: 1-byte opcode + 2-byte attribute handle. */
    static constexpr uint16_t ATT_PDU_OVERHEAD = 3;

    struct ReassemblyBuffer {
        std::vector<uint8_t> data;
        uint16_t totalLength = 0;
        uint16_t received = 0;
    };
    std::unordered_map<uint32_t, ReassemblyBuffer> writeReassemblyBuffers;

    static constexpr uint32_t reassemblyKey(const uint16_t conn_handle, const uint16_t attr_handle) {
        return (static_cast<uint32_t>(conn_handle) << 16) | attr_handle;
    }

    void handleFragmentedWrite(uint16_t conn_handle, uint16_t attr_handle, const uint8_t* buf, uint16_t len);
    void clearReassemblyBuffers(uint16_t conn_handle);
    void sendFragmented(uint16_t conn_handle, uint16_t val_handle, const uint8_t* data, uint16_t totalLength) const;

    std::vector<std::function<uint8_t*(int*, TopicType)>> dataReadCallbacks;

    /**
     * Write the first available read callback's data into the GATT access
     * context's mbuf, to be returned as the response to a read request.
     * @param ctxt The GATT access context of the read request.
     * @param attr_handle The attribute handle of the characteristic being read.
     * @return 0 on success, or a BLE_ATT_ERR_* code on failure.
     */
    int writeReadResponse(const ble_gatt_access_ctxt* ctxt, uint16_t attr_handle) const;

    void populateGattCharacteristics(const std::vector<Characteristic>& characteristics);
    void populateBatteryService();

    std::unordered_map<uint16_t, Characteristic*> characteristicsByHandle;
    std::unordered_map<TopicType, Characteristic*> characteristicsByTopic;

public:

    void init(const std::vector<Characteristic>& characteristics);

    void startAdvertising();

    /**
     * Notify connected clients with the given data.
     * @param topic The topic to notify clients about.
     * @param data Pointer to the data to be sent.
     * @param len Length of the data to be sent.
     */
    void notify(TopicType topic, uint8_t * data, int len);

    /**
     * Update and notify connected clients of the base station's own battery level via
     * the standard BLE Battery Service (0x180F / 0x2A19 Battery Level characteristic).
     * @param percentage Battery charge, 0-100.
     */
    void notifyBatteryLevel(uint8_t percentage);

    /**
     * Add a callback function that will be called when data is written to the Bluetooth SPP service.
     * @param callback A function that takes a pointer to a uint8_t array, an int (the length of the data), and a TopicType (the topic of the data) and returns void.
     */
    void addDataWriteCallback(const std::function<void(uint8_t*, int, TopicType)>& callback) {
        dataReceivedCallbacks.push_back(callback);
    }

    void addDataWriteCallback(const TopicType topic, const std::function<void(uint8_t*, int, TopicType)>& callback) {
        dataReceivedCallbacks.emplace_back([callback, topic](uint8_t* data, int len, const TopicType receivedTopic) {
            if (receivedTopic == topic) {
                callback(data, len, receivedTopic);
            }
        });
    }

    /**
     * Add a callback function that will be called when data is read from the Bluetooth SPP service.
     * @param callback A function that takes a pointer to an int (to store the length of the data) and
     *  returns a pointer to a uint8_t array containing the data to be sent.
     *  The caller is responsible for freeing the returned data after use.
     *  The first callback that returns non-null data will be used, and the rest will be ignored.
     */
    void addDataReadCallback(const std::function<uint8_t*(int*, TopicType)>& callback) {
        dataReadCallbacks.push_back(callback);
    }

    /**
     * Add a callback function that will be called when data is read from the Bluetooth SPP service for a specific topic.
     * @param topic The topic for which the callback should be invoked.
     * @param callback A function that takes a pointer to an int (to store the length of the data) and
     *  returns a pointer to a uint8_t array containing the data to be sent.
     *  The caller is responsible for freeing the returned data after use.
     *  The first callback that returns non-null data will be used, and the rest will be ignored.
     */
    void addDataReadCallback(const TopicType topic, const std::function<uint8_t*(int*, TopicType)>& callback) {
        dataReadCallbacks.emplace_back([callback, topic](int* len, const TopicType requestedTopic) -> uint8_t* {
            if (requestedTopic == topic) {
                return callback(len, requestedTopic);
            }
            return nullptr;
        });
    }
};