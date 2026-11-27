#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

// Lets WASM code build a custom BLE GATT peripheral at runtime instead of the
// static BT_GATT_SERVICE_DEFINE tables Zephyr normally expects, since the set
// of services isn't known at compile time.
namespace ble {

constexpr int MAX_SERVICES = 4;
constexpr int MAX_CHARACTERISTICS_PER_SERVICE = 6;
constexpr int MAX_CHARACTERISTICS =
    MAX_SERVICES * MAX_CHARACTERISTICS_PER_SERVICE;
constexpr int MAX_VALUE_LEN = 64;
// primary service decl + (characteristic decl + value + ccc) per characteristic
constexpr int MAX_ATTRS_PER_SERVICE = 1 + MAX_CHARACTERISTICS_PER_SERVICE * 3;

struct Characteristic {
    bt_uuid_128 uuid;
    bt_gatt_chrc decl;
    bt_gatt_ccc_managed_user_data ccc;
    uint8_t value[MAX_VALUE_LEN];
    uint16_t value_len;
    bt_gatt_attr *value_attr;
    bool dirty;
};

struct Service {
    bt_uuid_128 uuid;
    bt_gatt_attr attrs[MAX_ATTRS_PER_SERVICE];
    bt_gatt_service service;
    bool registered;
};

inline Service services[MAX_SERVICES];
inline int service_count = 0;

inline Characteristic characteristics[MAX_CHARACTERISTICS];
inline int characteristic_count = 0;

inline const bt_uuid_16 uuid_gatt_primary =
    BT_UUID_INIT_16(BT_UUID_GATT_PRIMARY_VAL);
inline const bt_uuid_16 uuid_gatt_chrc = BT_UUID_INIT_16(BT_UUID_GATT_CHRC_VAL);
inline const bt_uuid_16 uuid_gatt_ccc = BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL);

// WASM passes UUID bytes in standard (most significant byte first) order,
// Zephyr stores 128-bit UUIDs least significant byte first.
inline void bytes_to_uuid(const uint8_t *bytes, bt_uuid_128 *uuid) {
    uuid->uuid.type = BT_UUID_TYPE_128;
    for (int i = 0; i < 16; i++) {
        uuid->val[i] = bytes[15 - i];
    }
}

inline ssize_t characteristic_gatt_read(bt_conn *conn, const bt_gatt_attr *attr,
                                        void *buf, uint16_t len,
                                        uint16_t offset) {
    const auto *chr = static_cast<const Characteristic *>(attr->user_data);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, chr->value,
                             chr->value_len);
}

inline ssize_t characteristic_gatt_write(bt_conn *conn,
                                         const bt_gatt_attr *attr,
                                         const void *buf, uint16_t len,
                                         uint16_t offset, uint8_t flags) {
    auto *chr = static_cast<Characteristic *>(attr->user_data);
    if (offset + len > MAX_VALUE_LEN) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    memcpy(chr->value + offset, buf, len);
    chr->value_len = offset + len;
    chr->dirty = true;
    return len;
}

inline int service_create(const uint8_t uuid[16]) {
    if (service_count >= MAX_SERVICES) {
        return -1;
    }
    Service *svc = &services[service_count];
    bytes_to_uuid(uuid, &svc->uuid);
    svc->registered = false;
    svc->service.attrs = svc->attrs;
    svc->service.attr_count = 0;
    return service_count++;
}

inline int characteristic_create(int service_index, const uint8_t uuid[16],
                                 uint8_t properties, uint16_t permissions) {
    if (service_index < 0 || service_index >= service_count) {
        return -1;
    }
    Service *svc = &services[service_index];
    bool needs_ccc = properties & (BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_INDICATE);
    size_t needed =
        (svc->service.attr_count == 0 ? 1 : 0) + 2 + (needs_ccc ? 1 : 0);
    if (svc->registered || characteristic_count >= MAX_CHARACTERISTICS ||
        svc->service.attr_count + needed > MAX_ATTRS_PER_SERVICE) {
        return -1;
    }

    Characteristic *chr = &characteristics[characteristic_count];
    bytes_to_uuid(uuid, &chr->uuid);
    chr->decl = {
        .uuid = &chr->uuid.uuid, .value_handle = 0U, .properties = properties};
    chr->ccc = {.cfg = {},
                .cfg_changed = nullptr,
                .cfg_write = nullptr,
                .cfg_match = nullptr};
    chr->value_len = 0;
    chr->dirty = false;

    if (svc->service.attr_count == 0) {
        svc->attrs[svc->service.attr_count++] = {
            .uuid = &uuid_gatt_primary.uuid,
            .read = bt_gatt_attr_read_service,
            .write = nullptr,
            .user_data = &svc->uuid,
            .handle = 0,
            .perm = BT_GATT_PERM_READ,
        };
    }

    svc->attrs[svc->service.attr_count++] = {
        .uuid = &uuid_gatt_chrc.uuid,
        .read = bt_gatt_attr_read_chrc,
        .write = nullptr,
        .user_data = &chr->decl,
        .handle = 0,
        .perm = BT_GATT_PERM_READ,
    };
    chr->value_attr = &svc->attrs[svc->service.attr_count];
    svc->attrs[svc->service.attr_count++] = {
        .uuid = &chr->uuid.uuid,
        .read = characteristic_gatt_read,
        .write = characteristic_gatt_write,
        .user_data = chr,
        .handle = 0,
        .perm = permissions,
    };

    if (needs_ccc) {
        svc->attrs[svc->service.attr_count++] = {
            .uuid = &uuid_gatt_ccc.uuid,
            .read = bt_gatt_attr_read_ccc,
            .write = bt_gatt_attr_write_ccc,
            .user_data = &chr->ccc,
            .handle = 0,
            .perm = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        };
    }

    return characteristic_count++;
}

inline int characteristic_write(int index, const uint8_t *data, uint16_t len) {
    if (index < 0 || index >= characteristic_count) {
        return -1;
    }
    Characteristic *chr = &characteristics[index];
    if (len > MAX_VALUE_LEN) {
        len = MAX_VALUE_LEN;
    }
    memcpy(chr->value, data, len);
    chr->value_len = len;

    if (chr->decl.properties & (BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_INDICATE)) {
        // No subscribers is not an error for the WASM caller, ignore it.
        bt_gatt_notify(nullptr, chr->value_attr, chr->value, chr->value_len);
    }
    return 0;
}

inline int characteristic_read(int index, uint8_t *buf, uint16_t max_len) {
    if (index < 0 || index >= characteristic_count) {
        return -1;
    }
    Characteristic *chr = &characteristics[index];
    uint16_t len = chr->value_len < max_len ? chr->value_len : max_len;
    memcpy(buf, chr->value, len);
    chr->dirty = false;
    return len;
}

inline int characteristic_available(int index) {
    if (index < 0 || index >= characteristic_count) {
        return -1;
    }
    return characteristics[index].dirty ? 1 : 0;
}

inline int enable(const char *name) {
    int err = bt_enable(nullptr);
    if (err) {
        printf("Bluetooth init failed (err %d)\n", err);
        return err;
    }
    return bt_set_name(name);
}

// Advertising stops as soon as a central connects and Zephyr doesn't resume
// it automatically, so we track connection state so that you can poll if we
// are still advertising from wasm, if not you can call advertise_start() again.
inline int connection_count = 0;

inline void connected_cb(bt_conn *conn, uint8_t err) {
    if (!err) {
        connection_count++;
    }
}

inline void disconnected_cb(bt_conn *conn, uint8_t reason) {
    connection_count--;
}

BT_CONN_CB_DEFINE(callbacks) = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};

inline bool connected() { return connection_count > 0; }

// Advertisement data: flags, device name and every registered service's UUID.
inline bt_data ad[2 + MAX_SERVICES];

inline int advertise_start() {
    static uint8_t flags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
    int ad_count = 0;
    ad[ad_count++] = BT_DATA(BT_DATA_FLAGS, &flags, sizeof(flags));

    const char *name = bt_get_name();
    ad[ad_count++] = BT_DATA(BT_DATA_NAME_COMPLETE, name,
                             static_cast<uint8_t>(strlen(name)));

    for (int i = 0; i < service_count; i++) {
        Service *svc = &services[i];
        if (!svc->registered) {
            int err = bt_gatt_service_register(&svc->service);
            if (err) {
                printf("Failed to register BLE service %d (err %d)\n", i, err);
                return err;
            }
            svc->registered = true;
        }
        ad[ad_count++] = BT_DATA(BT_DATA_UUID128_ALL, svc->uuid.val, 16);
    }

    return bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ad_count, nullptr, 0);
}

inline int advertise_stop() { return bt_le_adv_stop(); }

}  // namespace ble
