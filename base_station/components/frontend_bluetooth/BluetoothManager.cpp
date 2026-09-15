#include "BluetoothManager.hpp"

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_att.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "build_timestamp.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

const char* BluetoothManager::TAG_BLUETOOTH_MANAGER = "BluetoothManager";

static BluetoothManager* instanceBT = nullptr;

BluetoothManager::BluetoothManager() {
    instanceBT = this;
    populateBuildVersionMfgData();
}

void BluetoothManager::populateBuildVersionMfgData() {
    // Company ID, little-endian (see BLEProtocol::MFG_COMPANY_ID / ble_protocol.json).
    buildVersionMfgData[0] = static_cast<uint8_t>(BLEProtocol::MFG_COMPANY_ID & 0xFF);
    buildVersionMfgData[1] = static_cast<uint8_t>((BLEProtocol::MFG_COMPANY_ID >> 8) & 0xFF);

    const auto epoch = static_cast<uint32_t>(BUILD_EPOCH_TIMESTAMP);
    buildVersionMfgData[2] = static_cast<uint8_t>(epoch & 0xFF);
    buildVersionMfgData[3] = static_cast<uint8_t>((epoch >> 8) & 0xFF);
    buildVersionMfgData[4] = static_cast<uint8_t>((epoch >> 16) & 0xFF);
    buildVersionMfgData[5] = static_cast<uint8_t>((epoch >> 24) & 0xFF);
}

void BluetoothManager::initFS() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/**
 * Formats an address in hex, e.g. "aa:bb:cc:dd:ee:ff", into `out`.
 */
static void format_addr(char* out, size_t outLen, const void* addr) {
    const auto* u8p = static_cast<const uint8_t*>(addr);
    snprintf(out, outLen, "%02x:%02x:%02x:%02x:%02x:%02x",
             u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

void
BluetoothManager::print_addr(const void* addr) {
    char buf[18];
    format_addr(buf, sizeof(buf), addr);
    ESP_LOGD(TAG_BLUETOOTH_MANAGER, "%s", buf);
}

/**
 * Logs information about a connection.
 */
void
BluetoothManager::ble_spp_server_print_conn_desc(ble_gap_conn_desc* desc) {
    char our_ota_addr[18], our_id_addr[18], peer_ota_addr[18], peer_id_addr[18];
    format_addr(our_ota_addr, sizeof(our_ota_addr), desc->our_ota_addr.val);
    format_addr(our_id_addr, sizeof(our_id_addr), desc->our_id_addr.val);
    format_addr(peer_ota_addr, sizeof(peer_ota_addr), desc->peer_ota_addr.val);
    format_addr(peer_id_addr, sizeof(peer_id_addr), desc->peer_id_addr.val);

    ESP_LOGD(TAG_BLUETOOTH_MANAGER,
             "handle=%d our_ota_addr_type=%d our_ota_addr=%s our_id_addr_type=%d our_id_addr=%s "
             "peer_ota_addr_type=%d peer_ota_addr=%s peer_id_addr_type=%d peer_id_addr=%s "
             "conn_itvl=%d conn_latency=%d supervision_timeout=%d encrypted=%d authenticated=%d bonded=%d",
             desc->conn_handle,
             desc->our_ota_addr.type, our_ota_addr,
             desc->our_id_addr.type, our_id_addr,
             desc->peer_ota_addr.type, peer_ota_addr,
             desc->peer_id_addr.type, peer_id_addr,
             desc->conn_itvl, desc->conn_latency,
             desc->supervision_timeout,
             desc->sec_state.encrypted,
             desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
void BluetoothManager::startAdvertising() {
    ble_gap_adv_params adv_params{};
    ble_hs_adv_fields fields{};
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
        BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    const char* name;
    name = ble_svc_gap_device_name();
    fields.name = (uint8_t*)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = &new_ble_svc_spp_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "error setting advertisement data; rc=%d", rc);
        return;
    }

    /* The primary advertisement packet above is already close to the 31-byte legacy ADV
     * limit (flags + tx power + service UUID + device name), so the build-version
     * manufacturer data goes in the separate scan response packet instead. Since
     * adv_params below uses undirected-connectable mode, the device is inherently
     * scannable and NimBLE answers SCAN_REQs with this data automatically. */
    ble_hs_adv_fields rsp_fields{};
    rsp_fields.mfg_data = buildVersionMfgData;
    rsp_fields.mfg_data_len = BUILD_VERSION_MFG_DATA_LEN;
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "error setting scan response data; rc=%d", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER,
                           &adv_params, ble_spp_server_gap_event, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "error enabling advertisement; rc=%d", rc);
        return;
    }
}

int BluetoothManager::ble_spp_server_gap_event(ble_gap_event* event, void* arg) {
    return instanceBT->onSPPGapEvent(event, arg);
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * ble_spp_server uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  ble_spp_server.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
int BluetoothManager::onSPPGapEvent(ble_gap_event* event, void* arg) {
    ble_gap_conn_desc desc{};
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        ESP_LOGI(TAG_BLUETOOTH_MANAGER, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            ble_spp_server_print_conn_desc(&desc);
        }
        if (event->connect.status != 0 || CONFIG_BT_NIMBLE_MAX_CONNECTIONS > 1) {
            /* Connection failed or if multiple connection allowed; resume advertising. */
            startAdvertising();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG_BLUETOOTH_MANAGER, "disconnect; reason=%d", event->disconnect.reason);
        ble_spp_server_print_conn_desc(&event->disconnect.conn);

        if (event->disconnect.conn.conn_handle <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
            conn_handle_subs[event->disconnect.conn.conn_handle] = false;
        }
        clearReassemblyBuffers(event->disconnect.conn.conn_handle);

        /* Connection terminated; resume advertising. */
        startAdvertising();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        ESP_LOGD(TAG_BLUETOOTH_MANAGER, "connection updated; status=%d", event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        ble_spp_server_print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGD(TAG_BLUETOOTH_MANAGER, "advertise complete; reason=%d", event->adv_complete.reason);
        startAdvertising();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGD(TAG_BLUETOOTH_MANAGER, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle,
                 event->mtu.channel_id,
                 event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG_BLUETOOTH_MANAGER, "subscribe event; conn_handle=%d attr_handle=%d "
                 "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle,
                 event->subscribe.reason,
                 event->subscribe.prev_notify,
                 event->subscribe.cur_notify,
                 event->subscribe.prev_indicate,
                 event->subscribe.cur_indicate);
        if (event->subscribe.conn_handle <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
            conn_handle_subs[event->subscribe.conn_handle] = true;
        }
        return 0;

    default:
        return 0;
    }
}

void BluetoothManager::onResetCb(int reason) {
    ESP_LOGE(TAG_BLUETOOTH_MANAGER, "Resetting state; reason=%d", reason);
}

void BluetoothManager::onSyncCb() {
    int rc;

    /* ble_gatts_add_svcs() (called from gatt_svr_init(), during init()) only queues
     * the service definitions — NimBLE doesn't actually register attributes and
     * assign real handles (via the val_handle pointers, see populateGattCharacteristics())
     * until ble_gatts_start(), which the host stack calls asynchronously from its own
     * task before triggering this sync callback. So this is the first point at which
     * every characteristic's val_handle is guaranteed to hold its real attribute handle. */
    for (const auto& [topic, characteristic] : characteristicsByTopic) {
        characteristicsByHandle[characteristic->val_handle] = characteristic;
    }

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "error determining address type; rc=%d", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, nullptr);

    char addr_str[18];
    format_addr(addr_str, sizeof(addr_str), addr_val);
    ESP_LOGI(TAG_BLUETOOTH_MANAGER, "Device Address: %s", addr_str);
    /* Begin advertising. */
    startAdvertising();
}

void BluetoothManager::ble_spp_server_host_task(void* param) {
    ESP_LOGI(TAG_BLUETOOTH_MANAGER, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

int BluetoothManager::ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt* ctxt,
                                           void* arg) {
    return instanceBT->onSVCGattHandler(conn_handle, attr_handle, ctxt, arg);
}

/**
 * Logs a received data buffer as a string if it is null-terminated (with no
 * embedded null bytes before the end), otherwise logs the whole buffer as hex.
 */
static void log_received_data(const char* tag, uint16_t conn_handle, uint16_t attr_handle, const uint8_t* buf,
                              uint16_t len) {
    const bool is_null_terminated_string = len > 0 && buf[len - 1] == '\0' &&
        memchr(buf, '\0', len - 1) == nullptr;

    if (is_null_terminated_string) {
        ESP_LOGI(tag, "Data received in write event,conn_handle = %x,attr_handle = %x, data = %s",
                 conn_handle, attr_handle, reinterpret_cast<const char *>(buf));
        return;
    }

    auto* hex = static_cast<char*>(malloc(static_cast<size_t>(len) * 3 + 1));
    if (hex == nullptr) {
        ESP_LOGI(
            tag,
            "Data received in write event,conn_handle = %x,attr_handle = %x, data = <hex log alloc failed, len=%u>",
            conn_handle, attr_handle, len);
        return;
    }

    for (uint16_t i = 0; i < len; ++i) {
        snprintf(hex + i * 3, 4, "%02x ", buf[i]);
    }
    hex[len > 0 ? len * 3 - 1 : 0] = '\0';

    ESP_LOGI(tag, "Data received in write event,conn_handle = %x,attr_handle = %x, data (hex) = %s",
             conn_handle, attr_handle, hex);
    free(hex);
}

/* Callback function for custom service */
int BluetoothManager::onSVCGattHandler(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt* ctxt,
                                       void* arg) {
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Reads carry no data of their own - they're just a "send me a fresh
         * notification now" trigger. ctxt->om is left empty, so this is a
         * zero-length ack; the real value follows shortly via a notification
         * on this same characteristic, same as the periodic 1s broadcast. */
        ESP_LOGD(TAG_BLUETOOTH_MANAGER, "read request; requesting out-of-cycle notify");
        callReadTriggerCallbacks(characteristicsByHandle.at(attr_handle)->topicID);
        return 0;

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len < FRAGMENT_HEADER_SIZE) {
            ESP_LOGE(TAG_BLUETOOTH_MANAGER, "write fragment too small (%u bytes, need at least %u for header)",
                     om_len, FRAGMENT_HEADER_SIZE);
            break;
        }

        auto* buf = static_cast<uint8_t*>(malloc(om_len));
        if (buf == nullptr) {
            ESP_LOGE(TAG_BLUETOOTH_MANAGER, "malloc failed for BLE write data, size=%u", om_len);
            break;
        }

        uint16_t out_len = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, om_len, &out_len);
        if (rc == 0) {
            handleFragmentedWrite(conn_handle, attr_handle, buf, out_len);
        }
        else {
            ESP_LOGE(TAG_BLUETOOTH_MANAGER, "ble_hs_mbuf_to_flat failed; rc=%d", rc);
        }

        free(buf);
        break;
    }

    default:
        ESP_LOGD(TAG_BLUETOOTH_MANAGER, "Default Callback");
        break;
    }
    return 0;
}


int BluetoothManager::ble_svc_battery_handler(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt* ctxt,
                                              void* arg) {
    return instanceBT->onBatterySvcGattHandler(conn_handle, attr_handle, ctxt, arg);
}

/* Callback for the standard Battery Service's Battery Level characteristic (read-only; no write flag is set). */
int BluetoothManager::onBatterySvcGattHandler(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt* ctxt,
                                              void* arg) const {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const int rc = os_mbuf_append(ctxt->om, &batteryLevel, sizeof(batteryLevel));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* Define new custom service */

void BluetoothManager::onGattSvrRegisterCb(ble_gatt_register_ctxt* ctxt, void* arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG_BLUETOOTH_MANAGER, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG_BLUETOOTH_MANAGER, "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle,
                 ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG_BLUETOOTH_MANAGER, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int BluetoothManager::gatt_svr_init() {
    int rc = 0;
#if CONFIG_BT_NIMBLE_GAP_SERVICE
    ble_svc_gap_init();
#endif
#if MYNEWT_VAL(BLE_GATTS)
    ble_svc_gatt_init();
#endif
    rc = ble_gatts_count_cfg(new_ble_svc_gatt_defs);

    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(new_ble_svc_gatt_defs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

void BluetoothManager::init(const std::vector<Characteristic>& characteristics) {
    initFS();

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "Failed to initialize NimBLE port");
        return;
    }

    /* Initialize connection_handle array */
    for (int i = 0; i <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        conn_handle_subs[i] = false;
    }

    populateGattCharacteristics(characteristics);
    populateBatteryService();

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = [](int reason) { instanceBT->onResetCb(reason); };
    ble_hs_cfg.sync_cb = []() { instanceBT->onSyncCb(); };
    ble_hs_cfg.gatts_register_cb = [](ble_gatt_register_ctxt* ctxt, void* arg) {
        instanceBT->onGattSvrRegisterCb(ctxt, arg);
    };
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

#if MYNEWT_VAL(BLE_GATTS)
    int rc;
    /* Register custom service */
    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set(CONFIG_BLUETOOTH_DEVICE_NAME);
    assert(rc == 0);
    ESP_LOGI(TAG_BLUETOOTH_MANAGER, "Device name set to: %s", CONFIG_BLUETOOTH_DEVICE_NAME);
#endif

    /* XXX Need to have template for store */
    ble_store_config_init();

    /* Advertising is started once the host reports sync (see onSyncCb) —
     * own_addr_type isn't valid until then, so it must not be started here. */
    nimble_port_freertos_init(ble_spp_server_host_task);
}

void BluetoothManager::notify(TopicType topic, uint8_t* data, int len) {
    const auto it = characteristicsByTopic.find(topic);
    if (it == characteristicsByTopic.end()) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "notify: unknown topic %d", topic);
        return;
    }
    if (len < 0 || len > UINT16_MAX) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "notify: payload too large (%d bytes, max %d)", len, UINT16_MAX);
        return;
    }
    const uint16_t val_handle = it->second->val_handle;
    const auto totalLength = static_cast<uint16_t>(len);

    for (int i = 0; i <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        /* Check if client has subscribed to notifications */
        if (conn_handle_subs[i]) {
            sendFragmented(static_cast<uint16_t>(i), val_handle, data, totalLength);
        }
    }
}

void BluetoothManager::notifyBatteryLevel(const uint8_t percentage) {
    batteryLevel = percentage;
    if (batteryLevelValHandle == 0) return; // not yet registered (ble_gatts_start() hasn't run)

    for (int i = 0; i <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        if (!conn_handle_subs[i]) continue;

        /* No application-level framing here (unlike sendFragmented) - a single byte
         * never needs fragmenting, and generic BAS clients expect the raw value. */
        os_mbuf* txom = ble_hs_mbuf_from_flat(&batteryLevel, sizeof(batteryLevel));
        if (txom == nullptr) {
            ESP_LOGE(TAG_BLUETOOTH_MANAGER, "notifyBatteryLevel: failed to allocate mbuf");
            return;
        }
        const int rc = ble_gatts_notify_custom(static_cast<uint16_t>(i), batteryLevelValHandle, txom);
        if (rc != 0) {
            ESP_LOGE(TAG_BLUETOOTH_MANAGER, "notifyBatteryLevel: error sending notification rc=%d", rc);
        }
    }
}

/**
 * Split `data` into MTU-sized fragments, each prefixed with a 4-byte
 * [totalLength, offset] header (see the framing comment in the header file),
 * and send them as separate notifications. Sends a single (possibly
 * header-only) fragment when totalLength is 0.
 */
void BluetoothManager::sendFragmented(const uint16_t conn_handle, const uint16_t val_handle, const uint8_t* data,
                                      const uint16_t totalLength) const {
    const uint16_t attMtu = ble_att_mtu(conn_handle);
    const uint16_t usableMtu = attMtu > 0 ? attMtu : BLE_ATT_MTU_DFLT;
    if (usableMtu <= ATT_PDU_OVERHEAD + FRAGMENT_HEADER_SIZE) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "notify: MTU too small to fragment (mtu=%d)", usableMtu);
        return;
    }
    const uint16_t chunkSize = usableMtu - ATT_PDU_OVERHEAD - FRAGMENT_HEADER_SIZE;

    uint16_t offset = 0;
    do {
        const uint16_t thisChunkLen = std::min<uint16_t>(chunkSize, totalLength - offset);

        std::vector<uint8_t> frame(FRAGMENT_HEADER_SIZE + thisChunkLen);
        frame[0] = static_cast<uint8_t>(totalLength & 0xFF);
        frame[1] = static_cast<uint8_t>((totalLength >> 8) & 0xFF);
        frame[2] = static_cast<uint8_t>(offset & 0xFF);
        frame[3] = static_cast<uint8_t>((offset >> 8) & 0xFF);
        if (thisChunkLen > 0) {
            memcpy(frame.data() + FRAGMENT_HEADER_SIZE, data + offset, thisChunkLen);
        }

        os_mbuf* txom = ble_hs_mbuf_from_flat(frame.data(), static_cast<uint16_t>(frame.size()));
        if (txom == nullptr) {
            ESP_LOGE(TAG_BLUETOOTH_MANAGER, "notify: failed to allocate mbuf for fragment (offset=%u len=%u)",
                     offset, thisChunkLen);
            return;
        }
        const int rc = ble_gatts_notify_custom(conn_handle, val_handle, txom);
        if (rc != 0) {
            ESP_LOGE(TAG_BLUETOOTH_MANAGER, "notify: error sending fragment rc=%d (offset=%u len=%u)", rc, offset,
                     thisChunkLen);
            return;
        }

        offset += thisChunkLen;
    }
    while (offset < totalLength);

    ESP_LOGD(TAG_BLUETOOTH_MANAGER, "Notification sent successfully (%u bytes, %u-byte chunks)", totalLength,
             chunkSize);
}

/**
 * Accumulate one fragment of a write into the reassembly buffer for
 * (conn_handle, attr_handle), delivering the complete payload to the
 * registered write callbacks once every byte has arrived.
 */
void BluetoothManager::handleFragmentedWrite(const uint16_t conn_handle, const uint16_t attr_handle,
                                             const uint8_t* buf, const uint16_t len) {
    if (len < FRAGMENT_HEADER_SIZE) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "write: fragment too small (%u bytes)", len);
        return;
    }
    const uint16_t totalLength = buf[0] | (buf[1] << 8);
    const uint16_t offset = buf[2] | (buf[3] << 8);
    const uint16_t chunkLen = len - FRAGMENT_HEADER_SIZE;
    const uint32_t key = reassemblyKey(conn_handle, attr_handle);

    if (static_cast<uint32_t>(offset) + chunkLen > totalLength) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER,
                 "write: malformed fragment (offset=%u chunkLen=%u total=%u); dropping in-progress message",
                 offset, chunkLen, totalLength);
        writeReassemblyBuffers.erase(key);
        return;
    }

    ReassemblyBuffer& state = writeReassemblyBuffers[key];
    if (offset == 0) {
        state.data.assign(totalLength, 0);
        state.totalLength = totalLength;
        state.received = 0;
    }
    else if (state.totalLength != totalLength || offset > state.data.size()) {
        ESP_LOGE(TAG_BLUETOOTH_MANAGER, "write: fragment doesn't match in-progress message; dropping");
        writeReassemblyBuffers.erase(key);
        return;
    }

    if (chunkLen > 0) {
        memcpy(state.data.data() + offset, buf + FRAGMENT_HEADER_SIZE, chunkLen);
    }
    state.received += chunkLen;

    if (state.received >= state.totalLength) {
        callDataReceivedCallbacks(state.data.data(), state.totalLength, attr_handle);
        log_received_data(TAG_BLUETOOTH_MANAGER, conn_handle, attr_handle, state.data.data(), state.totalLength);
        writeReassemblyBuffers.erase(key);
    }
}

void BluetoothManager::clearReassemblyBuffers(const uint16_t conn_handle) {
    for (auto it = writeReassemblyBuffers.begin(); it != writeReassemblyBuffers.end();) {
        if ((it->first >> 16) == conn_handle) {
            it = writeReassemblyBuffers.erase(it);
        }
        else {
            ++it;
        }
    }
}

void BluetoothManager::populateGattCharacteristics(const std::vector<Characteristic>& characteristics) {
    //new_ble_svc_gatt_chrs = (ble_gatt_chr_def*)calloc(BLETopics::ALL_NOTIFICATIONS_SIZE + 1, sizeof(ble_gatt_chr_def));
    for (int i = 0; i < characteristics.size(); i++) {
        auto* characteristic = new Characteristic(characteristics[i]);
        new_ble_svc_gatt_chrs[i] = {
            /* Support SPP service */
            .uuid = &BLETopics::getUUIDForTopic(BLETopics::ALL_NOTIFICATIONS[i])->u,
            .access_cb = ble_svc_gatt_handler,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &characteristic->val_handle,
        };
        characteristicsByTopic[characteristic->topicID] = characteristic;
        /* val_handle is only a placeholder (0) here — NimBLE doesn't assign the real
         * attribute handle (via the pointer above) until ble_gatts_start() runs, which
         * happens asynchronously from the host task well after this function returns.
         * characteristicsByHandle is indexed once that's guaranteed to have happened,
         * in onSyncCb(). */
    }
    //new_ble_svc_gatt_defs = (ble_gatt_svc_def*)calloc(2, sizeof(ble_gatt_svc_def));
    new_ble_svc_gatt_defs[0] =
    {
        /*** Service: SPP */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &new_ble_svc_spp_uuid.u,
        .characteristics = new_ble_svc_gatt_chrs
    };
}

void BluetoothManager::populateBatteryService() {
    battery_svc_gatt_chrs[0] = {
        .uuid = &battery_level_chr_uuid.u,
        .access_cb = ble_svc_battery_handler,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &batteryLevelValHandle,
    };
    new_ble_svc_gatt_defs[1] =
    {
        /*** Service: Battery (standard BAS) */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &battery_svc_uuid.u,
        .characteristics = battery_svc_gatt_chrs
    };
}
