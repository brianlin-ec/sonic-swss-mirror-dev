#include "ut_helper.h"

#include "orchdaemon.h"

// void syncd_apply_view() {}

// /* Global variables */
// sai_object_id_t gVirtualRouterId;
// sai_object_id_t gUnderlayIfId;
// sai_object_id_t gSwitchId = SAI_NULL_OBJECT_ID;
// MacAddress gMacAddress;
// MacAddress gVxlanMacAddress;

// #define DEFAULT_BATCH_SIZE 128
// int gBatchSize = DEFAULT_BATCH_SIZE;

// bool gSairedisRecord = true;
// bool gSwssRecord = true;
// bool gLogRotate = false;
// ofstream gRecordOfs;
// string gRecordFile;

extern sai_object_id_t gSwitchId;

extern PortsOrch* gPortsOrch;
extern CrmOrch* gCrmOrch;
extern BufferOrch* gBufferOrch;
// VRFOrch* gVrfOrch;

extern sai_switch_api_t* sai_switch_api;
extern sai_port_api_t* sai_port_api;
extern sai_vlan_api_t* sai_vlan_api;
extern sai_bridge_api_t* sai_bridge_api;
extern sai_route_api_t* sai_route_api;
extern sai_hostif_api_t* sai_hostif_api;
extern sai_fdb_api_t* sai_fdb_api;

namespace nsFdbOrchTest {

using namespace std;

size_t consumerAddToSync(Consumer* consumer, const deque<KeyOpFieldsValuesTuple>& entries)
{
    /* Nothing popped */
    if (entries.empty()) {
        return 0;
    }

    for (auto& entry : entries) {
        string key = kfvKey(entry);
        string op = kfvOp(entry);

        /* If a new task comes or if a DEL task comes, we directly put it into getConsumerTable().m_toSync map */
        if (consumer->m_toSync.find(key) == consumer->m_toSync.end() || op == DEL_COMMAND) {
            consumer->m_toSync[key] = entry;
        }
        /* If an old task is still there, we combine the old task with new task */
        else {
            KeyOpFieldsValuesTuple existing_data = consumer->m_toSync[key];

            auto new_values = kfvFieldsValues(entry);
            auto existing_values = kfvFieldsValues(existing_data);

            for (auto it : new_values) {
                string field = fvField(it);
                string value = fvValue(it);

                auto iu = existing_values.begin();
                while (iu != existing_values.end()) {
                    string ofield = fvField(*iu);
                    if (field == ofield)
                        iu = existing_values.erase(iu);
                    else
                        iu++;
                }
                existing_values.push_back(FieldValueTuple(field, value));
            }
            consumer->m_toSync[key] = KeyOpFieldsValuesTuple(key, op, existing_values);
        }
    }
    return entries.size();
}

struct TestBase : public ::testing::Test {

    bool AttrListEq(sai_object_type_t objecttype, const vector<sai_attribute_t>& act_attr_list, SaiAttributeList& exp_attr_list)
    {
        if (act_attr_list.size() != exp_attr_list.get_attr_count()) {
            return false;
        }

        auto l = exp_attr_list.get_attr_list();
        for (int i = 0; i < exp_attr_list.get_attr_count(); ++i) {
            sai_attr_id_t id = exp_attr_list.get_attr_list()[i].id;
            auto meta = sai_metadata_get_attr_metadata(objecttype, id);

            assert(meta != nullptr);

            char act_buf[0x4000];
            char exp_buf[0x4000];

            auto act_len = sai_serialize_attribute_value(act_buf, meta, &act_attr_list[i].value);
            auto exp_len = sai_serialize_attribute_value(exp_buf, meta, &exp_attr_list.get_attr_list()[i].value);

            assert(act_len < sizeof(act_buf));
            assert(exp_len < sizeof(exp_buf));

            if (act_len != exp_len) {
                cerr << "AttrListEq failed\n";
                cerr << "Actual:   " << act_buf << "\n";
                cerr << "Expected: " << exp_buf << "\n";
                return false;
            }

            if (strcmp(act_buf, exp_buf) != 0) {
                cerr << "AttrListEq failed\n";
                cerr << "Actual:   " << act_buf << "\n";
                cerr << "Expected: " << exp_buf << "\n";
                return false;
            }
        }

        return true;
    }
};

struct MockFdbOrch {
    FdbOrch* m_fdbOrch;
    swss::DBConnector* app_db;

    MockFdbOrch(swss::DBConnector* app_db, swss::DBConnector* state_db, PortsOrch* portOrch)
        : app_db(app_db)
    {
        TableConnector applDbFdb(app_db, APP_FDB_TABLE_NAME);
        TableConnector stateDbFdb(state_db, STATE_FDB_TABLE_NAME);

        m_fdbOrch = new FdbOrch(applDbFdb, stateDbFdb, gPortsOrch);
    }

    ~MockFdbOrch()
    {
        delete m_fdbOrch;
    }

    operator const FdbOrch*() const
    {
        return m_fdbOrch;
    }

    void doFdbTask(const deque<KeyOpFieldsValuesTuple>& entries)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new swss::ConsumerStateTable(app_db, APP_FDB_TABLE_NAME, 1, 1), m_fdbOrch, APP_FDB_TABLE_NAME));

        consumerAddToSync(consumer.get(), entries);

        static_cast<Orch*>(m_fdbOrch)->doTask(*consumer);
    }
};

struct FdbOrchTest : public TestBase {

    shared_ptr<swss::DBConnector> m_app_db;
    shared_ptr<swss::DBConnector> m_config_db;
    shared_ptr<swss::DBConnector> m_state_db;

    FdbOrchTest()
    {
        m_app_db = make_shared<swss::DBConnector>(APPL_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_config_db = make_shared<swss::DBConnector>(CONFIG_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_state_db = make_shared<swss::DBConnector>(STATE_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    }
    ~FdbOrchTest()
    {
    }

    static map<string, string> gProfileMap;
    static map<string, string>::iterator gProfileIter;

    static const char* profile_get_value(
        sai_switch_profile_id_t profile_id,
        const char* variable)
    {
        map<string, string>::const_iterator it = gProfileMap.find(variable);
        if (it == gProfileMap.end()) {
            return NULL;
        }

        return it->second.c_str();
    }

    static int profile_get_next_value(
        sai_switch_profile_id_t profile_id,
        const char** variable,
        const char** value)
    {
        if (value == NULL) {
            gProfileIter = gProfileMap.begin();
            return 0;
        }

        if (variable == NULL) {
            return -1;
        }

        if (gProfileIter == gProfileMap.end()) {
            return -1;
        }

        *variable = gProfileIter->first.c_str();
        *value = gProfileIter->second.c_str();

        gProfileIter++;

        return 0;
    }

    void SetUp() override
    {
        gProfileMap.emplace("SAI_VS_SWITCH_TYPE", "SAI_VS_SWITCH_TYPE_BCM56850");
        gProfileMap.emplace("KV_DEVICE_MAC_ADDRESS", "20:03:04:05:06:00");

        sai_service_method_table_t test_services = {
            FdbOrchTest::profile_get_value,
            FdbOrchTest::profile_get_next_value
        };

        auto status = sai_api_initialize(0, (sai_service_method_table_t*)&test_services);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

#if WITH_SAI == LIBVS
        sai_switch_api = const_cast<sai_switch_api_t*>(&vs_switch_api);
        sai_port_api = const_cast<sai_port_api_t*>(&vs_port_api);
        sai_vlan_api = const_cast<sai_vlan_api_t*>(&vs_vlan_api);
        sai_bridge_api = const_cast<sai_bridge_api_t*>(&vs_bridge_api);
        sai_route_api = const_cast<sai_route_api_t*>(&vs_route_api);
        sai_hostif_api = const_cast<sai_hostif_api_t*>(&vs_hostif_api);
        sai_fdb_api = const_cast<sai_fdb_api_t*>(&vs_fdb_api);
#endif

        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr.value.booldata = true;
        status = sai_switch_api->create_switch(&gSwitchId, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // Get switch source MAC address
        attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
        status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
        gMacAddress = attr.value.mac;

        // Get the default virtual router ID
        attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
        status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
        gVirtualRouterId = attr.value.oid;

        const int portsorch_base_pri = 40;

        vector<table_name_with_pri_t> ports_tables = {
            { APP_PORT_TABLE_NAME, portsorch_base_pri + 5 },
            { APP_VLAN_TABLE_NAME, portsorch_base_pri + 2 },
            { APP_VLAN_MEMBER_TABLE_NAME, portsorch_base_pri },
            { APP_LAG_TABLE_NAME, portsorch_base_pri + 4 },
            { APP_LAG_MEMBER_TABLE_NAME, portsorch_base_pri }
        };

        ASSERT_EQ(gPortsOrch, nullptr);
        gPortsOrch = new PortsOrch(m_app_db.get(), ports_tables);

        ASSERT_EQ(gCrmOrch, nullptr);
        gCrmOrch = new CrmOrch(m_config_db.get(), CFG_CRM_TABLE_NAME);

        vector<string> buffer_tables = {
            CFG_BUFFER_POOL_TABLE_NAME,
            CFG_BUFFER_PROFILE_TABLE_NAME,
            CFG_BUFFER_QUEUE_TABLE_NAME,
            CFG_BUFFER_PG_TABLE_NAME,
            CFG_BUFFER_PORT_INGRESS_PROFILE_LIST_NAME,
            CFG_BUFFER_PORT_EGRESS_PROFILE_LIST_NAME
        };

        ASSERT_EQ(gBufferOrch, nullptr);
        gBufferOrch = new BufferOrch(m_config_db.get(), buffer_tables);

        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_PORT_TABLE_NAME, 1, 1), gPortsOrch, APP_PORT_TABLE_NAME));

        /* Get port number */
        attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
        status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
        auto port_count = attr.value.u32;

        /* Get port list */
        vector<sai_object_id_t> port_list;
        port_list.resize(port_count);
        attr.id = SAI_SWITCH_ATTR_PORT_LIST;
        attr.value.objlist.count = (uint32_t)port_list.size();
        attr.value.objlist.list = port_list.data();
        status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        deque<KeyOpFieldsValuesTuple> port_init_tuple;
        for (auto i = 0; i < port_count; i++) {
            string lan_map_str = "";
            sai_uint32_t lanes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
            attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
            attr.value.u32list.count = 8;
            attr.value.u32list.list = lanes;
            status = sai_port_api->get_port_attribute(port_list[i], 1, &attr);
            ASSERT_EQ(status, SAI_STATUS_SUCCESS);

            for (auto j = 0; j < attr.value.u32list.count; j++) {
                if (j != 0)
                    lan_map_str += ",";
                lan_map_str += to_string(attr.value.u32list.list[j]);
            }

            port_init_tuple.push_back(
                { "Ethernet" + to_string(i), SET_COMMAND, { { "lanes", lan_map_str }, { "mtu", "9100" } } });
        }
        port_init_tuple.push_back({ "PortConfigDone", SET_COMMAND, { { "count", to_string(port_count) } } });

        consumerAddToSync(consumer.get(), port_init_tuple);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);

        auto setData = deque<KeyOpFieldsValuesTuple>({ { "PortInitDone", EMPTY_PREFIX, { { "", "" } } } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
    }

    void TearDown() override
    {
        delete gPortsOrch;
        gPortsOrch = nullptr;
        delete gCrmOrch;
        gCrmOrch = nullptr;
        delete gBufferOrch;
        gBufferOrch = nullptr;

        auto status = sai_switch_api->remove_switch(gSwitchId);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
        gSwitchId = 0;

        sai_api_uninitialize();

        sai_switch_api = nullptr;
        sai_port_api = nullptr;
        sai_vlan_api = nullptr;
        sai_bridge_api = nullptr;
        sai_route_api = nullptr;
        sai_hostif_api = nullptr;
        sai_fdb_api = nullptr;
    }

    shared_ptr<MockFdbOrch> createFdbOrch()
    {
        return make_shared<MockFdbOrch>(m_app_db.get(), m_state_db.get(), gPortsOrch);
    }

    // shared_ptr<SaiAttributeList> getMirrorAttributeList(sai_object_type_t objecttype, const MirrorEntry& entry)
    // {
    //     vector<swss::FieldValueTuple> fields;

    //     return shared_ptr<SaiAttributeList>(new SaiAttributeList(objecttype, fields, false));
    // }

    bool Validate(sai_object_type_t objecttype, sai_object_id_t object_id, SaiAttributeList& exp_attrlist)
    {
        vector<sai_attribute_t> act_attr;

        for (int i = 0; i < exp_attrlist.get_attr_count(); ++i) {
            const auto attr = exp_attrlist.get_attr_list()[i];
            auto meta = sai_metadata_get_attr_metadata(objecttype, attr.id);

            if (meta == nullptr) {
                return false;
            }

            sai_attribute_t new_attr = { 0 };
            new_attr.id = attr.id;

            act_attr.emplace_back(new_attr);
        }

        // auto status = sai_fdb_api->get_fdb_entry_attribute(object_id, act_attr.size(), act_attr.data());
        // if (status != SAI_STATUS_SUCCESS) {
        //     return false;
        // }

        auto b_attr_eq = AttrListEq(objecttype, act_attr, exp_attrlist);
        if (!b_attr_eq) {
            return false;
        }

        return true;
    }

    // bool ValidateMirrorEntryByConfOp(const MirrorEntry& entry, const vector<swss::FieldValueTuple>& values)
    // {
    //     for (auto it : values) {
    //         if (fvField(it) == "src_ip") {
    //             if (entry.srcIp.to_string() != fvValue(it)) {
    //                 return false;
    //             }
    //         } else if (fvField(it) == "dst_ip") {
    //             if (entry.dstIp.to_string() != fvValue(it)) {
    //                 return false;
    //             }
    //         } else if (fvField(it) == "gre_type") {
    //             if (entry.greType != to_uint<uint16_t>(fvValue(it))) {
    //                 return false;
    //             }
    //         } else if (fvField(it) == "dscp") {
    //             if (entry.dscp != to_uint<uint8_t>(fvValue(it))) {
    //                 return false;
    //             }
    //         } else if (fvField(it) == "ttl") {
    //             if (entry.ttl != to_uint<uint8_t>(fvValue(it))) {
    //                 return false;
    //             }
    //         } else if (fvField(it) == "queue") {
    //             if (entry.queue != to_uint<uint8_t>(fvValue(it))) {
    //                 return false;
    //             }
    //         }
    //     }

    //     return true;
    // }

    string create_vlan(int vlan)
    {
        string vlan_key = "Vlan" + to_string(vlan);
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_VLAN_TABLE_NAME, 1, 1), gPortsOrch, APP_VLAN_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { vlan_key, SET_COMMAND, {} } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
        return vlan_key;
    }

    void add_vlan_member(int vlan, string interface)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_VLAN_MEMBER_TABLE_NAME, 1, 1), gPortsOrch, APP_VLAN_MEMBER_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { "Vlan" + to_string(vlan) + ":" + interface, SET_COMMAND, { { "tagging_mode", "untagged" } } } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
    }

    // void create_port_channel(string channel)
    // {
    //     auto consumer = unique_ptr<Consumer>(new Consumer(
    //         new ConsumerStateTable(m_app_db.get(), APP_LAG_TABLE_NAME, 1, 1), gPortsOrch, APP_LAG_TABLE_NAME));
    //     auto setData = deque<KeyOpFieldsValuesTuple>(
    //         { { channel, SET_COMMAND, { { "admin", "up" }, { "mtu", "9100" } } } });
    //     consumerAddToSync(consumer.get(), setData);
    //     static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
    // }

    // void create_port_channel_member(string channel, string interface)
    // {
    //     auto consumer = unique_ptr<Consumer>(new Consumer(
    //         new ConsumerStateTable(m_app_db.get(), APP_LAG_MEMBER_TABLE_NAME, 1, 1), gPortsOrch, APP_LAG_MEMBER_TABLE_NAME));
    //     auto setData = deque<KeyOpFieldsValuesTuple>(
    //         { { channel + ":" + interface, SET_COMMAND, { { "status", "enabled" } } } });
    //     consumerAddToSync(consumer.get(), setData);
    //     static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
    // }
};

map<string, string> FdbOrchTest::gProfileMap;
map<string, string>::iterator FdbOrchTest::gProfileIter = FdbOrchTest::gProfileMap.begin();

TEST_F(FdbOrchTest, foo)
{
    auto orch = createFdbOrch();

    string exp_port = "Ethernet0";
    string exp_mac = "00:01:02:03:04:05";
    int exp_vlan = 1;

    // create vlan
    string vlan_str = create_vlan(exp_vlan);

    // add vlan member
    add_vlan_member(exp_vlan, exp_port);

    auto setData = deque<KeyOpFieldsValuesTuple>(
        { { "Vlan" + to_string(exp_vlan) + ":" + exp_mac,
            SET_COMMAND,
            {
                { "port", exp_port },
                { "type", "dynamic" },
            } } });

    orch->doFdbTask(setData);

    Port p;
    ASSERT_TRUE(gPortsOrch->getVlanByVlanId(exp_vlan, p));

    MacAddress mac(exp_mac);
    sai_fdb_entry_t entry;
    entry.switch_id = gSwitchId;
    memcpy(entry.mac_address, mac.getMac(), sizeof(sai_mac_t));
    entry.bv_id = p.m_vlan_info.vlan_oid;

    sai_attribute_t attr;
    attr.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;

    sai_status_t status = sai_fdb_api->get_fdb_entry_attribute(&entry, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    ASSERT_TRUE(gPortsOrch->getPortByBridgePortId(attr.value.oid, p));
    ASSERT_EQ(p.m_alias, exp_port);
}

}
