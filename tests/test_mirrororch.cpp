#include "gtest/gtest.h"

#define private public
#include "mirrororch.h"
#undef private

#include "consumerstatetable.h"
#include "converter.h"
#include "hiredis.h"
#include "orchdaemon.h"
#include "sai_vs.h"
#include "saiattributelist.h"
#include "saihelper.h"

void syncd_apply_view() {}

/* Global variables */
sai_object_id_t gVirtualRouterId;
sai_object_id_t gUnderlayIfId;
sai_object_id_t gSwitchId = SAI_NULL_OBJECT_ID;
MacAddress gMacAddress;
MacAddress gVxlanMacAddress;

#define DEFAULT_BATCH_SIZE 128
int gBatchSize = DEFAULT_BATCH_SIZE;

bool gSairedisRecord = true;
bool gSwssRecord = true;
bool gLogRotate = false;
ofstream gRecordOfs;
string gRecordFile;

extern CrmOrch* gCrmOrch;
extern PortsOrch* gPortsOrch;
extern RouteOrch* gRouteOrch;
extern IntfsOrch* gIntfsOrch;
extern NeighOrch* gNeighOrch;
extern FdbOrch* gFdbOrch;
extern BufferOrch* gBufferOrch;
VRFOrch* gVrfOrch;

extern sai_switch_api_t* sai_switch_api;
extern sai_port_api_t* sai_port_api;
extern sai_vlan_api_t* sai_vlan_api;
extern sai_bridge_api_t* sai_bridge_api;
extern sai_route_api_t* sai_route_api;
extern sai_router_interface_api_t* sai_router_intfs_api;
extern sai_mirror_api_t* sai_mirror_api;
extern sai_hostif_api_t* sai_hostif_api;
extern sai_neighbor_api_t* sai_neighbor_api;
extern sai_next_hop_api_t* sai_next_hop_api;
extern sai_fdb_api_t* sai_fdb_api;
extern sai_lag_api_t* sai_lag_api;

namespace nsMirrorOrchTest {

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

struct MockMirrorOrch {
    MirrorOrch* m_mirrorOrch;
    swss::DBConnector* config_db;

    MockMirrorOrch(swss::DBConnector* config_db, swss::DBConnector* state_db,
        PortsOrch* portOrch, RouteOrch* routeOrch, NeighOrch* neighOrch, FdbOrch* fdbOrch)
        : config_db(config_db)
    {
        TableConnector stateDbMirrorSession(state_db, APP_MIRROR_SESSION_TABLE_NAME);
        TableConnector confDbMirrorSession(config_db, CFG_MIRROR_SESSION_TABLE_NAME);

        m_mirrorOrch = new MirrorOrch(stateDbMirrorSession, confDbMirrorSession, portOrch, routeOrch, neighOrch, fdbOrch);
    }

    ~MockMirrorOrch()
    {
        delete m_mirrorOrch;
    }

    operator const MirrorOrch*() const
    {
        return m_mirrorOrch;
    }

    const MirrorTable& getMirrorTable()
    {
        return m_mirrorOrch->m_syncdMirrors;
    }

    void doMirrorTask(const deque<KeyOpFieldsValuesTuple>& entries)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new swss::ConsumerStateTable(config_db, CFG_MIRROR_SESSION_TABLE_NAME, 1, 1), m_mirrorOrch, CFG_MIRROR_SESSION_TABLE_NAME));

        consumerAddToSync(consumer.get(), entries);

        static_cast<Orch*>(m_mirrorOrch)->doTask(*consumer);
    }

    bool getSessionOid(const string& name, sai_object_id_t& oid)
    {
        return m_mirrorOrch->getSessionOid(name, oid);
    }
};

struct MirrorOrchTest : public TestBase {

    shared_ptr<swss::DBConnector> m_app_db;
    shared_ptr<swss::DBConnector> m_config_db;
    shared_ptr<swss::DBConnector> m_state_db;

    MirrorOrchTest()
    {
        m_app_db = make_shared<swss::DBConnector>(APPL_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_config_db = make_shared<swss::DBConnector>(CONFIG_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_state_db = make_shared<swss::DBConnector>(STATE_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    }
    ~MirrorOrchTest()
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
            MirrorOrchTest::profile_get_value,
            MirrorOrchTest::profile_get_next_value
        };

        auto status = sai_api_initialize(0, (sai_service_method_table_t*)&test_services);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        sai_switch_api = const_cast<sai_switch_api_t*>(&vs_switch_api);
        sai_port_api = const_cast<sai_port_api_t*>(&vs_port_api);
        sai_vlan_api = const_cast<sai_vlan_api_t*>(&vs_vlan_api);
        sai_bridge_api = const_cast<sai_bridge_api_t*>(&vs_bridge_api);
        sai_route_api = const_cast<sai_route_api_t*>(&vs_route_api);
        sai_router_intfs_api = const_cast<sai_router_interface_api_t*>(&vs_router_interface_api);
        sai_neighbor_api = const_cast<sai_neighbor_api_t*>(&vs_neighbor_api);
        sai_next_hop_api = const_cast<sai_next_hop_api_t*>(&vs_next_hop_api);
        sai_mirror_api = const_cast<sai_mirror_api_t*>(&vs_mirror_api);
        sai_hostif_api = const_cast<sai_hostif_api_t*>(&vs_hostif_api);
        sai_fdb_api = const_cast<sai_fdb_api_t*>(&vs_fdb_api);
        sai_lag_api = const_cast<sai_lag_api_t*>(&vs_lag_api);

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

        ASSERT_EQ(gVrfOrch, nullptr);
        gVrfOrch = new VRFOrch(m_app_db.get(), APP_VRF_TABLE_NAME);

        ASSERT_EQ(gIntfsOrch, nullptr);
        gIntfsOrch = new IntfsOrch(m_app_db.get(), APP_INTF_TABLE_NAME, gVrfOrch);

        ASSERT_EQ(gNeighOrch, nullptr);
        gNeighOrch = new NeighOrch(m_app_db.get(), APP_NEIGH_TABLE_NAME, gIntfsOrch);

        ASSERT_EQ(gRouteOrch, nullptr);
        gRouteOrch = new RouteOrch(m_app_db.get(), APP_ROUTE_TABLE_NAME, gNeighOrch);

        TableConnector applDbFdb(m_app_db.get(), APP_FDB_TABLE_NAME);
        TableConnector stateDbFdb(m_state_db.get(), STATE_FDB_TABLE_NAME);

        ASSERT_EQ(gFdbOrch, nullptr);
        gFdbOrch = new FdbOrch(applDbFdb, stateDbFdb, gPortsOrch);

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
                { "Ethernet" + to_string(i), SET_COMMAND, { { "lanes", lan_map_str } } });
        }
        port_init_tuple.push_back({ "PortConfigDone", SET_COMMAND, { { "count", to_string(port_count) } } });
        port_init_tuple.push_back({ "PortInitDone", EMPTY_PREFIX, { { "", "" } } });

        consumerAddToSync(consumer.get(), port_init_tuple);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
    }

    void TearDown() override
    {
        delete gFdbOrch;
        gFdbOrch = nullptr;
        delete gRouteOrch;
        gRouteOrch = nullptr;
        delete gNeighOrch;
        gNeighOrch = nullptr;
        delete gIntfsOrch;
        gIntfsOrch = nullptr;
        delete gVrfOrch;
        gVrfOrch = nullptr;
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
        sai_router_intfs_api = nullptr;
        sai_neighbor_api = nullptr;
        sai_next_hop_api = nullptr;
        sai_mirror_api = nullptr;
        sai_hostif_api = nullptr;
        sai_fdb_api = nullptr;
        sai_lag_api = nullptr;
    }

    shared_ptr<MockMirrorOrch> createMirrorOrch()
    {
        return make_shared<MockMirrorOrch>(m_config_db.get(), m_state_db.get(), gPortsOrch, gRouteOrch,
            gNeighOrch, gFdbOrch);
    }

    shared_ptr<SaiAttributeList> getMirrorAttributeList(sai_object_type_t objecttype, const MirrorEntry& entry)
    {
        vector<swss::FieldValueTuple> fields;

        auto port_id = sai_serialize_object_id(entry.neighborInfo.portId);

        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_TYPE", "SAI_MIRROR_SESSION_TYPE_ENHANCED_REMOTE" });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_ERSPAN_ENCAPSULATION_TYPE", "SAI_ERSPAN_ENCAPSULATION_TYPE_MIRROR_L3_GRE_TUNNEL" });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_IPHDR_VERSION", "4" });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_SRC_MAC_ADDRESS", gMacAddress.to_string() });

        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_MONITOR_PORT", port_id });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_DST_MAC_ADDRESS", entry.neighborInfo.mac.to_string() });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_SRC_IP_ADDRESS", entry.srcIp.to_string() });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_DST_IP_ADDRESS", entry.dstIp.to_string() });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_GRE_PROTOCOL_TYPE", to_string(entry.greType) });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_TOS", to_string((uint16_t)(entry.dscp << 2)) });
        fields.push_back({ "SAI_MIRROR_SESSION_ATTR_TTL", to_string(entry.ttl) });
        if (entry.queue != 0) {
            fields.push_back({ "SAI_MIRROR_SESSION_ATTR_TC", to_string(entry.queue) });
        }
        if (entry.neighborInfo.port.m_type == Port::VLAN) {
            fields.push_back({ "SAI_MIRROR_SESSION_ATTR_VLAN_HEADER_VALID", "true" });
            fields.push_back({ "SAI_MIRROR_SESSION_ATTR_VLAN_TPID", "33024" }); // 0x8100
            fields.push_back({ "SAI_MIRROR_SESSION_ATTR_VLAN_ID", to_string(entry.neighborInfo.port.m_vlan_info.vlan_id) });
            fields.push_back({ "SAI_MIRROR_SESSION_ATTR_VLAN_PRI", "0" });
            fields.push_back({ "SAI_MIRROR_SESSION_ATTR_VLAN_CFI", "0" });
        }

        return shared_ptr<SaiAttributeList>(new SaiAttributeList(objecttype, fields, false));
    }

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

        auto status = sai_mirror_api->get_mirror_session_attribute(object_id, act_attr.size(), act_attr.data());
        if (status != SAI_STATUS_SUCCESS) {
            return false;
        }

        auto b_attr_eq = AttrListEq(objecttype, act_attr, exp_attrlist);
        if (!b_attr_eq) {
            return false;
        }

        return true;
    }

    bool ValidateMirrorEntryByConfOp(const MirrorEntry& entry, const vector<swss::FieldValueTuple>& values)
    {
        for (auto it : values) {
            if (fvField(it) == "src_ip") {
                if (entry.srcIp.to_string() != fvValue(it)) {
                    return false;
                }
            } else if (fvField(it) == "dst_ip") {
                if (entry.dstIp.to_string() != fvValue(it)) {
                    return false;
                }
            } else if (fvField(it) == "gre_type") {
                if (entry.greType != to_uint<uint16_t>(fvValue(it))) {
                    return false;
                }
            } else if (fvField(it) == "dscp") {
                if (entry.dscp != to_uint<uint8_t>(fvValue(it))) {
                    return false;
                }
            } else if (fvField(it) == "ttl") {
                if (entry.ttl != to_uint<uint8_t>(fvValue(it))) {
                    return false;
                }
            } else if (fvField(it) == "queue") {
                if (entry.queue != to_uint<uint8_t>(fvValue(it))) {
                    return false;
                }
            }
        }

        return true;
    }

    void add_ip_addr(string interface, string ip)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_INTF_TABLE_NAME, 1, 1), gIntfsOrch, APP_INTF_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { interface + ":" + ip, SET_COMMAND, {} } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gIntfsOrch)->doTask(*consumer);
    }

    void add_neighbor(string interface, string ip, string mac)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_NEIGH_TABLE_NAME, 1, 1), gNeighOrch, APP_NEIGH_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { interface + ":" + ip, SET_COMMAND, { { "neigh", mac } } } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gNeighOrch)->doTask(*consumer);
    }

    void remove_neighbor(string interface, string ip)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_NEIGH_TABLE_NAME, 1, 1), gNeighOrch, APP_NEIGH_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { interface + ":" + ip, DEL_COMMAND, {} } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gNeighOrch)->doTask(*consumer);
    }

    void add_route(string prefix, string nexthop, string interface)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_ROUTE_TABLE_NAME, 1, 1), gRouteOrch, APP_ROUTE_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { prefix, SET_COMMAND, { { "nexthop", nexthop }, { "ifname", interface } } } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gRouteOrch)->doTask(*consumer);
    }

    void remove_route(string prefix)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_ROUTE_TABLE_NAME, 1, 1), gRouteOrch, APP_ROUTE_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { prefix, DEL_COMMAND, {} } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gRouteOrch)->doTask(*consumer);
    }

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

    void create_fdb(int vlan, string mac, string interface)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_FDB_TABLE_NAME, 1, 1), gPortsOrch, APP_FDB_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { "Vlan" + to_string(vlan) + ":" + mac,
                SET_COMMAND,
                {
                    { "port", interface },
                    { "type", "dynamic" },
                } } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gFdbOrch)->doTask(*consumer);
    }

    void remove_fdb(int vlan, string mac)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_FDB_TABLE_NAME, 1, 1), gPortsOrch, APP_FDB_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { "Vlan" + to_string(vlan) + ":" + mac, DEL_COMMAND, {} } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gFdbOrch)->doTask(*consumer);
    }

    void create_port_channel(string channel)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_LAG_TABLE_NAME, 1, 1), gPortsOrch, APP_LAG_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { channel, SET_COMMAND, { { "admin", "up" }, { "mtu", "9100" } } } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
    }

    void create_port_channel_member(string channel, string interface)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_LAG_MEMBER_TABLE_NAME, 1, 1), gPortsOrch, APP_LAG_MEMBER_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { channel + ":" + interface, SET_COMMAND, { { "status", "enabled" } } } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
    }
};

map<string, string> MirrorOrchTest::gProfileMap;
map<string, string>::iterator MirrorOrchTest::gProfileIter = MirrorOrchTest::gProfileMap.begin();

TEST_F(MirrorOrchTest, Create_And_Delete_Session)
{
    auto orch = createMirrorOrch();

    string session_name = "mirror_session_1";
    auto mirror_cfg = deque<KeyOpFieldsValuesTuple>(
        { { session_name,
            SET_COMMAND,
            {
                { "src_ip", "1.1.1.1" },
                { "dst_ip", "2.2.2.2" },
                { "gre_type", "0x6558" },
                { "dscp", "8" },
                { "ttl", "100" },
                { "queue", "0" },
            } } });

    orch->doMirrorTask(mirror_cfg);

    const auto& mirror_table = orch->getMirrorTable();
    auto it = mirror_table.find(session_name);
    ASSERT_NE(it, mirror_table.end()); // session exist
    const auto& mirror_entry = it->second;
    ASSERT_EQ(mirror_entry.status, false); // session inactive

    ASSERT_EQ(ValidateMirrorEntryByConfOp(mirror_entry, kfvFieldsValues(mirror_cfg.front())), true);

    auto del_cfg = deque<KeyOpFieldsValuesTuple>(
        { { session_name,
            DEL_COMMAND,
            {} } });
    orch->doMirrorTask(del_cfg);

    ASSERT_EQ(mirror_table.find(session_name), mirror_table.end()); // session not exist
}

TEST_F(MirrorOrchTest, Activate_And_Deactivate_Session)
{
    auto orch = createMirrorOrch();

    string session_name = "mirror_session_1";
    string dst_ip = "2.2.2.2";
    auto mirror_cfg = deque<KeyOpFieldsValuesTuple>(
        { { session_name,
            SET_COMMAND,
            {
                { "src_ip", "1.1.1.1" },
                { "dst_ip", dst_ip },
                { "gre_type", "0x6558" },
                { "dscp", "8" },
                { "ttl", "100" },
                { "queue", "1" },
            } } });

    orch->doMirrorTask(mirror_cfg);

    const auto& mirror_table = orch->getMirrorTable();
    auto it = mirror_table.find(session_name);
    ASSERT_NE(it, mirror_table.end()); // session exist
    const auto& mirror_entry = it->second;
    ASSERT_EQ(mirror_entry.status, false); // session inactive

    string exp_port = "Ethernet0";
    string exp_mac = "00:01:02:03:04:05";

    // add ip
    add_ip_addr(exp_port, "192.168.1.1/24");

    // add neighbor to activate session
    add_neighbor(exp_port, dst_ip, exp_mac);

    ASSERT_EQ(mirror_entry.status, true); // session active

    Port p;
    ASSERT_TRUE(gPortsOrch->getPort(exp_port, p));
    ASSERT_EQ(mirror_entry.neighborInfo.portId, p.m_port_id);
    ASSERT_EQ(mirror_entry.neighborInfo.mac.to_string(), exp_mac);
    ASSERT_TRUE(ValidateMirrorEntryByConfOp(mirror_entry, kfvFieldsValues(mirror_cfg.front())));

    sai_object_id_t session_oid;
    ASSERT_TRUE(orch->getSessionOid(session_name, session_oid));

    const sai_object_type_t objecttype = SAI_OBJECT_TYPE_MIRROR_SESSION;
    auto exp_attrlist = getMirrorAttributeList(objecttype, mirror_entry);
    ASSERT_TRUE(Validate(objecttype, session_oid, *exp_attrlist.get()));

    // remove neighbor to deactivate session
    remove_neighbor(exp_port, dst_ip);

    ASSERT_EQ(mirror_entry.status, false); // session inactive
}

TEST_F(MirrorOrchTest, Activate_And_Deactivate_Session_2)
{
    auto orch = createMirrorOrch();

    string session_name = "mirror_session_1";
    string dst_ip = "2.2.2.2";
    auto mirror_cfg = deque<KeyOpFieldsValuesTuple>(
        { { session_name,
            SET_COMMAND,
            {
                { "src_ip", "1.1.1.1" },
                { "dst_ip", dst_ip },
                { "gre_type", "0x6558" },
                { "dscp", "8" },
                { "ttl", "100" },
                { "queue", "1" },
            } } });

    orch->doMirrorTask(mirror_cfg);

    const auto& mirror_table = orch->getMirrorTable();
    auto it = mirror_table.find(session_name);
    ASSERT_NE(it, mirror_table.end()); // session exist
    const auto& mirror_entry = it->second;
    ASSERT_EQ(mirror_entry.status, false); // session inactive

    string exp_port = "Ethernet0";
    string exp_mac = "00:01:02:03:04:05";
    string exp_nexthop = "10.0.0.1";

    // add ip
    add_ip_addr(exp_port, "10.0.0.0/31");

    // add neighbor
    add_neighbor(exp_port, exp_nexthop, exp_mac);

    // add route to activate session
    add_route(dst_ip, exp_nexthop, exp_port);

    ASSERT_EQ(mirror_entry.status, true); // session active

    Port p;
    ASSERT_TRUE(gPortsOrch->getPort(exp_port, p));
    ASSERT_EQ(mirror_entry.neighborInfo.portId, p.m_port_id);
    ASSERT_EQ(mirror_entry.neighborInfo.mac.to_string(), exp_mac);
    ASSERT_TRUE(ValidateMirrorEntryByConfOp(mirror_entry, kfvFieldsValues(mirror_cfg.front())));

    sai_object_id_t session_oid;
    ASSERT_TRUE(orch->getSessionOid(session_name, session_oid));

    const sai_object_type_t objecttype = SAI_OBJECT_TYPE_MIRROR_SESSION;
    auto exp_attrlist = getMirrorAttributeList(objecttype, mirror_entry);
    ASSERT_TRUE(Validate(objecttype, session_oid, *exp_attrlist.get()));

    // remove route to deactivate session
    remove_route(dst_ip);

    ASSERT_EQ(mirror_entry.status, false); // session inactive
}

TEST_F(MirrorOrchTest, MirrorToVlan)
{
    auto orch = createMirrorOrch();

    string session_name = "mirror_session_1";
    string dst_ip = "2.2.2.2";
    auto mirror_cfg = deque<KeyOpFieldsValuesTuple>(
        { { session_name,
            SET_COMMAND,
            {
                { "src_ip", "1.1.1.1" },
                { "dst_ip", dst_ip },
                { "gre_type", "0x6558" },
                { "dscp", "8" },
                { "ttl", "100" },
                { "queue", "1" },
            } } });

    orch->doMirrorTask(mirror_cfg);

    const auto& mirror_table = orch->getMirrorTable();
    auto it = mirror_table.find(session_name);
    ASSERT_NE(it, mirror_table.end()); // session exist
    const auto& mirror_entry = it->second;
    ASSERT_EQ(mirror_entry.status, false); // session inactive

    string exp_port = "Ethernet0";
    string exp_mac = "00:01:02:03:04:05";
    int exp_vlan = 6;

    // create vlan
    string vlan_str = create_vlan(exp_vlan);

    // add vlan member
    add_vlan_member(exp_vlan, exp_port);

    // add ip
    add_ip_addr(vlan_str, "192.168.1.1/24");

    // add neighbor
    add_neighbor(vlan_str, dst_ip, exp_mac);

    // add fdb to activate session
    create_fdb(exp_vlan, exp_mac, exp_port);

    ASSERT_EQ(mirror_entry.status, true); // session active

    Port p;
    ASSERT_TRUE(gPortsOrch->getPort(exp_port, p));
    ASSERT_EQ(mirror_entry.neighborInfo.portId, p.m_port_id);
    ASSERT_EQ(mirror_entry.neighborInfo.mac.to_string(), exp_mac);
    ASSERT_TRUE(ValidateMirrorEntryByConfOp(mirror_entry, kfvFieldsValues(mirror_cfg.front())));

    sai_object_id_t session_oid;
    ASSERT_TRUE(orch->getSessionOid(session_name, session_oid));

    const sai_object_type_t objecttype = SAI_OBJECT_TYPE_MIRROR_SESSION;
    auto exp_attrlist = getMirrorAttributeList(objecttype, mirror_entry);
    ASSERT_TRUE(Validate(objecttype, session_oid, *exp_attrlist.get()));

    // remove fdb to deactivate session
    remove_fdb(exp_vlan, exp_mac);

    ASSERT_EQ(mirror_entry.status, false); // session inactive
}

TEST_F(MirrorOrchTest, MirrorToLAG)
{
    auto orch = createMirrorOrch();

    string session_name = "mirror_session_1";
    string dst_ip = "2.2.2.2";
    auto mirror_cfg = deque<KeyOpFieldsValuesTuple>(
        { { session_name,
            SET_COMMAND,
            {
                { "src_ip", "1.1.1.1" },
                { "dst_ip", dst_ip },
                { "gre_type", "0x6558" },
                { "dscp", "8" },
                { "ttl", "100" },
                { "queue", "1" },
            } } });

    orch->doMirrorTask(mirror_cfg);

    const auto& mirror_table = orch->getMirrorTable();
    auto it = mirror_table.find(session_name);
    ASSERT_NE(it, mirror_table.end()); // session exist
    const auto& mirror_entry = it->second;
    ASSERT_EQ(mirror_entry.status, false); // session inactive

    string exp_port = "Ethernet0";
    string exp_mac = "00:01:02:03:04:05";
    string exp_lag = "PortChannel0";

    // create port channel
    create_port_channel(exp_lag);

    // create port channel member
    create_port_channel_member(exp_lag, exp_port);

    // add ip
    add_ip_addr(exp_lag, "192.168.1.1/24");

    // add neighbor to activate session
    add_neighbor(exp_lag, dst_ip, exp_mac);

    ASSERT_EQ(mirror_entry.status, true); // session active

    Port p;
    ASSERT_TRUE(gPortsOrch->getPort(exp_port, p));
    ASSERT_EQ(mirror_entry.neighborInfo.portId, p.m_port_id);
    ASSERT_EQ(mirror_entry.neighborInfo.mac.to_string(), exp_mac);
    ASSERT_TRUE(ValidateMirrorEntryByConfOp(mirror_entry, kfvFieldsValues(mirror_cfg.front())));

    sai_object_id_t session_oid;
    ASSERT_TRUE(orch->getSessionOid(session_name, session_oid));

    const sai_object_type_t objecttype = SAI_OBJECT_TYPE_MIRROR_SESSION;
    auto exp_attrlist = getMirrorAttributeList(objecttype, mirror_entry);
    ASSERT_TRUE(Validate(objecttype, session_oid, *exp_attrlist.get()));

    // remove neighbor to deactivate session
    remove_neighbor(exp_lag, dst_ip);

    ASSERT_EQ(mirror_entry.status, false); // session inactive
}

}
