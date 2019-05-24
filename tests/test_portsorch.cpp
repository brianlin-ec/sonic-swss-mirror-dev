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

#define PORT_SPEED_TYPE  5
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
extern sai_hostif_api_t* sai_hostif_api;
extern sai_neighbor_api_t* sai_neighbor_api;


namespace nsPortsOrchTest {

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

struct PortsOrchTest : public TestBase {

    shared_ptr<swss::DBConnector> m_app_db;
    shared_ptr<swss::DBConnector> m_config_db;
    shared_ptr<swss::DBConnector> m_state_db;

    PortsOrchTest()
    {
        m_app_db = make_shared<swss::DBConnector>(APPL_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_config_db = make_shared<swss::DBConnector>(CONFIG_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_state_db = make_shared<swss::DBConnector>(STATE_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    }
    ~PortsOrchTest()
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
            PortsOrchTest::profile_get_value,
            PortsOrchTest::profile_get_next_value
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
        sai_hostif_api = const_cast<sai_hostif_api_t*>(&vs_hostif_api);        
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
                { "Ethernet" + to_string(i), 
                    SET_COMMAND, 
                    { { "lanes", lan_map_str },
                      { "speed", "1000" },
                      { "mtu", "6000" },
                      { "admin_status", "down" },
                    } 
                }
            );
            // std::cout << "Ethernet" << to_string(i)<<endl;;
        }
        port_init_tuple.push_back({ "PortConfigDone", SET_COMMAND, { { "count", to_string(port_count) } } });
        port_init_tuple.push_back({ "PortInitDone", EMPTY_PREFIX, { { "", "" } } });

        consumerAddToSync(consumer.get(), port_init_tuple);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
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
        sai_hostif_api = nullptr;        
    }
    
    bool Validate(Port* p, uint32_t exp_value, sai_attr_id_t attr_id)
    {
        sai_attribute_t attr;                   
        attr.id = attr_id;
        auto status = sai_port_api->get_port_attribute(p->m_port_id, 1, &attr);
        if (status != SAI_STATUS_SUCCESS) {
            return false;
        }
        /* mtu + 14 + 4 + 4 = 22 bytes */
        /* sizeof(struct ether_header) + FCS_LEN + VLAN_TAG_LEN */
        if(attr_id ==SAI_PORT_ATTR_MTU)
            exp_value +=22;
                         
        if (exp_value != attr.value.u32) {
            return false;
        }
        return true;        
    }    
    
    bool ValidateAdminStatus(Port* p, bool exp_value, sai_attr_id_t attr_id)
    {
        sai_attribute_t attr;                   
        attr.id = attr_id;
        auto status = sai_port_api->get_port_attribute(p->m_port_id, 1, &attr);
        if (status != SAI_STATUS_SUCCESS) {
            return false;
        }
        if (exp_value != attr.value.booldata ) {
            return false;
        }
        return true;        
    }    
    
    void create_port_interface(string interface)
    {
        auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_LAG_TABLE_NAME, 1, 1), gPortsOrch, APP_LAG_TABLE_NAME));
        auto setData = deque<KeyOpFieldsValuesTuple>(
            { { interface, SET_COMMAND, { { "admin", "up" }, { "mtu", "6000" } , { "speed", "1000" }} } });
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);        
    }
};

map<string, string> PortsOrchTest::gProfileMap;
map<string, string>::iterator PortsOrchTest::gProfileIter = PortsOrchTest::gProfileMap.begin();


TEST_F(PortsOrchTest, PortMTUTest)
{
    Port p;
    string exp_port = "Ethernet1";
    uint32_t exp_mtu = 9100;
    sai_attribute_t attr;
    auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_PORT_TABLE_NAME, 1, 1), gPortsOrch, APP_PORT_TABLE_NAME));
       
    deque<KeyOpFieldsValuesTuple> setData;        

    setData.push_back(
        { "Ethernet1", 
            SET_COMMAND, 
            { { "lanes", "25,26,27,28" },
            { "mtu", to_string(exp_mtu) }, 
            } 
        });
        
    consumerAddToSync(consumer.get(), setData);
    static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
                
    ASSERT_TRUE(gPortsOrch->getPort(exp_port, p));  
    ASSERT_TRUE(Validate(&p, exp_mtu, SAI_PORT_ATTR_MTU));          
}

TEST_F(PortsOrchTest, PortSpeedTest)
{
    Port p;
    string exp_port = "Ethernet1";
    uint32_t exp_speed = 10000;
    uint32_t speed_list[PORT_SPEED_TYPE] = {10000, 25000, 40000, 50000, 100000};
    sai_attribute_t attr;
    auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_PORT_TABLE_NAME, 1, 1), gPortsOrch, APP_PORT_TABLE_NAME));
        
    deque<KeyOpFieldsValuesTuple> setData;
    for (auto jj = 0; jj < PORT_SPEED_TYPE; jj++) {
        exp_speed = speed_list[jj];   
        setData.push_back(
        { "Ethernet1", 
            SET_COMMAND, 
            { { "lanes", "25,26,27,28" },
            { "speed", to_string(exp_speed)}, 
            } 
        });                                   
        
        consumerAddToSync(consumer.get(), setData);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);               
                
        ASSERT_TRUE(gPortsOrch->getPort(exp_port, p));  
        ASSERT_TRUE(Validate(&p, exp_speed, SAI_PORT_ATTR_SPEED));   
    }
}

TEST_F(PortsOrchTest, PortAdminTest)
{
    Port p;
    string exp_port = "Ethernet1";    
    bool exp_admin_status = true;
    sai_attribute_t attr;
    auto consumer = unique_ptr<Consumer>(new Consumer(
            new ConsumerStateTable(m_app_db.get(), APP_PORT_TABLE_NAME, 1, 1), gPortsOrch, APP_PORT_TABLE_NAME));
    
    deque<KeyOpFieldsValuesTuple> setData;
    setData.push_back(
        { "Ethernet1", 
            SET_COMMAND, 
            { { "lanes", "25,26,27,28" },
            { "admin_status", "up"}, 
            } 
        });                       
    consumerAddToSync(consumer.get(), setData);
    static_cast<Orch*>(gPortsOrch)->doTask(*consumer);           
        
    ASSERT_TRUE(gPortsOrch->getPort(exp_port, p));  
    ASSERT_TRUE(ValidateAdminStatus(&p, exp_admin_status, SAI_PORT_ATTR_ADMIN_STATE));   
}



}