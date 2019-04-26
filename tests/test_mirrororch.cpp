#include "gtest/gtest.h"

#include "consumerstatetable.h"
#include "hiredis.h"
#include "orchdaemon.h"
#include "sai_vs.h"
#include "saiattributelist.h"
#include "saihelper.h"

void syncd_apply_view() {}

using namespace std;

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

uint32_t set_attr_count;
sai_attribute_t set_attr_list[20];
vector<int32_t> bpoint_list;
vector<int32_t> range_types_list;

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
extern sai_next_hop_api_t *sai_next_hop_api;

class ConsumerExtend : public Consumer
{
  public:
    ConsumerExtend(ConsumerTableBase *select, Orch *orch, const string &name) : Consumer(select, orch, name)
    {
    }

    size_t addToSync(std::deque<KeyOpFieldsValuesTuple> &entries)
    {
        Consumer::addToSync(entries);
        return 0;
    }
};

const char* profile_get_value(
    _In_ sai_switch_profile_id_t profile_id,
    _In_ const char* variable)
{
    // UNREFERENCED_PARAMETER(profile_id);

    if (!strcmp(variable, "SAI_KEY_INIT_CONFIG_FILE")) {
        return "/usr/share/sai_2410.xml"; // FIXME: create a json file, and passing the path into test
    } else if (!strcmp(variable, "KV_DEVICE_MAC_ADDRESS")) {
        return "20:03:04:05:06:00";
    } else if (!strcmp(variable, "SAI_KEY_L3_ROUTE_TABLE_SIZE")) {
        return "1000";
    } else if (!strcmp(variable, "SAI_KEY_L3_NEIGHBOR_TABLE_SIZE")) {
        return "2000";
    } else if (!strcmp(variable, "SAI_VS_SWITCH_TYPE")) {
        return "SAI_VS_SWITCH_TYPE_BCM56850";
    }

    return NULL;
}

static int profile_get_next_value(
    _In_ sai_switch_profile_id_t profile_id,
    _Out_ const char** variable,
    _Out_ const char** value)
{
    if (value == NULL) {
        return 0;
    }

    if (variable == NULL) {
        return -1;
    }

    return -1;
}

struct TestBase : public ::testing::Test {
    //
    // spy functions
    //
    static sai_status_t sai_get_vlan_attribute_(_In_ sai_object_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t* attr_list)
    {
        return that->sai_get_vlan_attribute_fn(vlan_id, attr_count, attr_list);
    }

    static sai_status_t sai_remove_vlan_member_(_In_ sai_object_id_t vlan_member_id)
    {
        return that->sai_remove_vlan_member_fn(vlan_member_id);
    }

    static sai_status_t sai_get_bridge_attribute_(_In_ sai_object_id_t bridge_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t* attr_list)
    {
        return that->sai_get_bridge_attribute_fn(bridge_id, attr_count, attr_list);
    }

    static sai_status_t sai_get_bridge_port_attribute_(_In_ sai_object_id_t bridge_port_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t* attr_list)
    {
        return that->sai_get_bridge_port_attribute_fn(bridge_port_id, attr_count, attr_list);
    }

    static sai_status_t sai_remove_bridge_port_(_In_ sai_object_id_t bridge_port_id)
    {
        return that->sai_remove_bridge_port_fn(bridge_port_id);
    }

    static sai_status_t sai_create_hostif_(_Out_ sai_object_id_t* hostif_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t* attr_list)
    {
        return that->sai_create_hostif_fn(hostif_id, switch_id, attr_count, attr_list);
    }

    static sai_status_t sai_set_hostif_attribute_(_In_ sai_object_id_t hostif_id,
        _In_ const sai_attribute_t* attr)
    {
        return that->sai_set_hostif_attribute_fn(hostif_id, attr);
    }

    static TestBase* that;

    std::function<sai_status_t(sai_object_id_t, uint32_t, sai_attribute_t*)>
        sai_get_vlan_attribute_fn;

    std::function<sai_status_t(sai_object_id_t)>
        sai_remove_vlan_member_fn;

    std::function<sai_status_t(sai_object_id_t, uint32_t, sai_attribute_t*)>
        sai_get_bridge_attribute_fn;

    std::function<sai_status_t(sai_object_id_t, uint32_t, sai_attribute_t*)>
        sai_get_bridge_port_attribute_fn;

    std::function<sai_status_t(sai_object_id_t)>
        sai_remove_bridge_port_fn;

    std::function<sai_status_t(sai_object_id_t*, sai_object_id_t, uint32_t, const sai_attribute_t*)>
        sai_create_hostif_fn;

    std::function<sai_status_t(sai_object_id_t, const sai_attribute_t*)>
        sai_set_hostif_attribute_fn;

    //
    // validation functions (NO NEED TO put into Test class => move to Validation class)
    //
    bool AttrListEq(sai_object_type_t objecttype, const std::vector<sai_attribute_t>& act_attr_list, /*const*/ SaiAttributeList& exp_attr_list)
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

            // auto act = sai_serialize_attr_value(*meta, act_attr_list[i].value, false);
            // auto exp = sai_serialize_attr_value(*meta, &exp_attr_list.get_attr_list()[i].value, false);

            assert(act_len < sizeof(act_buf));
            assert(exp_len < sizeof(exp_buf));

            if (act_len != exp_len) {
                std::cout << "AttrListEq failed\n";
                std::cout << "Actual:   " << act_buf << "\n";
                std::cout << "Expected: " << exp_buf << "\n";
                return false;
            }

            if (strcmp(act_buf, exp_buf) != 0) {
                std::cout << "AttrListEq failed\n";
                std::cout << "Actual:   " << act_buf << "\n";
                std::cout << "Expected: " << exp_buf << "\n";
                return false;
            }
        }

        return true;
    }
};

TestBase* TestBase::that = nullptr;

struct MirrorTest : public TestBase {

    std::shared_ptr<swss::DBConnector> m_app_db;
    std::shared_ptr<swss::DBConnector> m_config_db;
    std::shared_ptr<swss::DBConnector> m_state_db;

    MirrorTest()
    {

        m_app_db = std::make_shared<swss::DBConnector>(APPL_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_config_db = std::make_shared<swss::DBConnector>(CONFIG_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_state_db = std::make_shared<swss::DBConnector>(STATE_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    }
    ~MirrorTest()
    {
    }
    void SetUp() override
    {
        assert(gFdbOrch == nullptr);
        assert(gRouteOrch == nullptr);
        assert(gNeighOrch == nullptr);
        assert(gIntfsOrch == nullptr);
        assert(gVrfOrch == nullptr);
        assert(gCrmOrch == nullptr);
        assert(gPortsOrch == nullptr);
        assert(gBufferOrch == nullptr);

        ///////////////////////////////////////////////////////////////////////
        sai_service_method_table_t test_services = {
            profile_get_value,
            profile_get_next_value
        };

        auto status = sai_api_initialize(0, (sai_service_method_table_t*)&test_services);
        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);

        // FIXME: using clone not just assign
        sai_switch_api = const_cast<sai_switch_api_t*>(&vs_switch_api);
        sai_port_api = const_cast<sai_port_api_t*>(&vs_port_api);
        sai_route_api = const_cast<sai_route_api_t*>(&vs_route_api);
        sai_router_intfs_api = const_cast<sai_router_interface_api_t*>(&vs_router_interface_api);
        sai_neighbor_api = const_cast<sai_neighbor_api_t*>(&vs_neighbor_api);
        sai_next_hop_api = const_cast<sai_next_hop_api_t*>(&vs_next_hop_api);
        sai_mirror_api = const_cast<sai_mirror_api_t*>(&vs_mirror_api);

        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr.value.booldata = true;

        status = sai_switch_api->create_switch(&gSwitchId, 1, &attr);
        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);
        
        // Get switch source MAC address
        attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
        status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);

        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);

        gMacAddress = attr.value.mac;

        // Get the default virtual router ID
        attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
        status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);

        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);

        gVirtualRouterId = attr.value.oid;
        ///////////////////////////////////////////////////////////////////////

        assert(sai_vlan_api == nullptr);
        assert(sai_bridge_api == nullptr);
        assert(sai_hostif_api == nullptr);

        auto sai_vlan = std::shared_ptr<sai_vlan_api_t>(new sai_vlan_api_t(), [](sai_vlan_api_t* p) {
            delete p;
            sai_vlan_api = nullptr;
        });
        auto sai_bridge = std::shared_ptr<sai_bridge_api_t>(new sai_bridge_api_t(), [](sai_bridge_api_t* p) {
            delete p;
            sai_bridge_api = nullptr;
        });
        auto sai_hostif = std::shared_ptr<sai_hostif_api_t>(new sai_hostif_api_t(), [](sai_hostif_api_t* p) {
            delete p;
            sai_hostif_api = nullptr;
        });

        sai_vlan_api = sai_vlan.get();
        sai_bridge_api = sai_bridge.get();
        sai_hostif_api = sai_hostif.get();

        sai_vlan_api->get_vlan_attribute = sai_get_vlan_attribute_;
        sai_vlan_api->remove_vlan_member = sai_remove_vlan_member_;
        sai_bridge_api->get_bridge_attribute = sai_get_bridge_attribute_;
        sai_bridge_api->get_bridge_port_attribute = sai_get_bridge_port_attribute_;
        sai_bridge_api->remove_bridge_port = sai_remove_bridge_port_;
        sai_hostif_api->create_hostif = sai_create_hostif_;
        sai_hostif_api->set_hostif_attribute = sai_set_hostif_attribute_;
        that = this;

        sai_get_vlan_attribute_fn =
            [](_In_ sai_object_id_t vlan_id,
                _In_ uint32_t attr_count,
                _Inout_ sai_attribute_t* attr_list) -> sai_status_t {
            return SAI_STATUS_SUCCESS;
        };

        sai_remove_vlan_member_fn =
            [](_In_ sai_object_id_t vlan_member_id) -> sai_status_t {
            return SAI_STATUS_SUCCESS;
        };

        sai_get_bridge_attribute_fn =
            [](_In_ sai_object_id_t bridge_id,
                _In_ uint32_t attr_count,
                _Inout_ sai_attribute_t* attr_list) -> sai_status_t {
            return SAI_STATUS_SUCCESS;
        };

        sai_get_bridge_port_attribute_fn =
            [](_In_ sai_object_id_t bridge_port_id,
                _In_ uint32_t attr_count,
                _Inout_ sai_attribute_t* attr_list) -> sai_status_t {
            return SAI_STATUS_SUCCESS;
        };

        sai_remove_bridge_port_fn =
            [](_In_ sai_object_id_t bridge_port_id) -> sai_status_t {
            return SAI_STATUS_SUCCESS;
        };

        sai_create_hostif_fn =
            [](_Out_ sai_object_id_t* hostif_id,
                _In_ sai_object_id_t switch_id,
                _In_ uint32_t attr_count,
                _In_ const sai_attribute_t* attr_list) -> sai_status_t {
            return SAI_STATUS_SUCCESS;
        };

        sai_set_hostif_attribute_fn =
            [](_In_ sai_object_id_t hostif_id,
                _In_ const sai_attribute_t* attr) -> sai_status_t {
            return SAI_STATUS_SUCCESS;
        };

        const int portsorch_base_pri = 40;

        vector<table_name_with_pri_t> ports_tables = {
            { APP_PORT_TABLE_NAME, portsorch_base_pri + 5 },
            { APP_VLAN_TABLE_NAME, portsorch_base_pri + 2 },
            { APP_VLAN_MEMBER_TABLE_NAME, portsorch_base_pri },
            { APP_LAG_TABLE_NAME, portsorch_base_pri + 4 },
            { APP_LAG_MEMBER_TABLE_NAME, portsorch_base_pri }
        };
        gPortsOrch = new PortsOrch(m_app_db.get(), ports_tables);

        gCrmOrch = new CrmOrch(m_config_db.get(), CFG_CRM_TABLE_NAME);
        gVrfOrch = new VRFOrch(m_app_db.get(), APP_VRF_TABLE_NAME);
        gIntfsOrch = new IntfsOrch(m_app_db.get(), APP_INTF_TABLE_NAME, gVrfOrch);
        gNeighOrch = new NeighOrch(m_app_db.get(), APP_NEIGH_TABLE_NAME, gIntfsOrch);
        gRouteOrch = new RouteOrch(m_app_db.get(), APP_ROUTE_TABLE_NAME, gNeighOrch);

        TableConnector applDbFdb(m_app_db.get(), APP_FDB_TABLE_NAME);
        TableConnector stateDbFdb(m_state_db.get(), STATE_FDB_TABLE_NAME);
        gFdbOrch = new FdbOrch(applDbFdb, stateDbFdb, gPortsOrch);

        vector<string> buffer_tables = {
            CFG_BUFFER_POOL_TABLE_NAME,
            CFG_BUFFER_PROFILE_TABLE_NAME,
            CFG_BUFFER_QUEUE_TABLE_NAME,
            CFG_BUFFER_PG_TABLE_NAME,
            CFG_BUFFER_PORT_INGRESS_PROFILE_LIST_NAME,
            CFG_BUFFER_PORT_EGRESS_PROFILE_LIST_NAME
        };
        gBufferOrch = new BufferOrch(m_config_db.get(), buffer_tables);

        auto consumerExt = std::unique_ptr<ConsumerExtend>(new ConsumerExtend(
            new ConsumerStateTable(m_app_db.get(), APP_PORT_TABLE_NAME, 1, 1), gPortsOrch, APP_PORT_TABLE_NAME));
        auto setData = std::deque<KeyOpFieldsValuesTuple>(
            { { "Ethernet0",
                  SET_COMMAND,
                  {
                      { "lanes", "29,30,31,32" },
                      { "alias", "Ethernet0" },
                      { "oper_status", "up" },
                      { "mtu", "9100" },
                      { "admin_status", "up" },
                  } },
                { "PortConfigDone",
                    SET_COMMAND,
                    { { "count", "1" } } },
                { "PortInitDone",
                    EMPTY_PREFIX,
                    { { "", "" } } } });
        consumerExt->addToSync(setData);
        static_cast<Orch*>(gPortsOrch)->doTask(*consumerExt);
    }

    void TearDown() override
    {
        delete gFdbOrch; // FIXME: using auto ptr
        gFdbOrch = nullptr;
        delete gRouteOrch; // FIXME: using auto ptr
        gRouteOrch = nullptr;
        delete gNeighOrch; // FIXME: using auto ptr
        gNeighOrch = nullptr;
        delete gIntfsOrch; // FIXME: using auto ptr
        gIntfsOrch = nullptr;
        delete gVrfOrch; // FIXME: using auto ptr
        gVrfOrch = nullptr;
        delete gPortsOrch; // FIXME: using auto ptr
        gPortsOrch = nullptr;
        delete gCrmOrch; // FIXME: using auto ptr
        gCrmOrch = nullptr;
        delete gBufferOrch; // FIXME: using auto ptr
        gBufferOrch = nullptr;

        ///////////////////////////////////////////////////////////////////////

        auto status = sai_switch_api->remove_switch(gSwitchId);
        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);
        gSwitchId = 0;

        sai_api_uninitialize();

        sai_switch_api = nullptr;
    }
};

TEST_F(MirrorTest, Create_And_Delete_Mirror_Session)
{
    TableConnector stateDbMirrorSession(m_state_db.get(), APP_MIRROR_SESSION_TABLE_NAME);
    TableConnector confDbMirrorSession(m_config_db.get(), CFG_MIRROR_SESSION_TABLE_NAME);
    auto mirror_orch = MirrorOrch(stateDbMirrorSession, confDbMirrorSession, gPortsOrch, gRouteOrch, gNeighOrch, gFdbOrch);

    auto consumerExt = std::unique_ptr<ConsumerExtend>(new ConsumerExtend(
        new ConsumerStateTable(m_config_db.get(), CFG_MIRROR_SESSION_TABLE_NAME, 1, 1), &mirror_orch, CFG_MIRROR_SESSION_TABLE_NAME));
    std::string mirror_session_name = "mirror_session_1";
    auto mirror_cfg = std::deque<KeyOpFieldsValuesTuple>(
        { { mirror_session_name,
            SET_COMMAND,
            {
                { "src_ip", "1.1.1.1" },
                { "dst_ip", "2.2.2.2" },
                { "gre_type", "0x6558" },
                { "dscp", "8" },
                { "ttl", "100" },
                { "queue", "0" },
            } } });
    consumerExt->addToSync(mirror_cfg);

    static_cast<Orch*>(&mirror_orch)->doTask(*consumerExt);

    bool session_state;
    ASSERT_TRUE(mirror_orch.getSessionStatus(mirror_session_name, session_state)); // session exist
    ASSERT_TRUE(session_state == false); //session inactive
    //TODO: validate session fields

    auto del_cfg = std::deque<KeyOpFieldsValuesTuple>(
        { { mirror_session_name,
            DEL_COMMAND,
            {
                { "", "" },
            } } });
    consumerExt->addToSync(del_cfg);

    static_cast<Orch*>(&mirror_orch)->doTask(*consumerExt);

    ASSERT_TRUE(false == mirror_orch.sessionExists(mirror_session_name)); // session not exist
}

TEST_F(MirrorTest, Create_Mirror_Session_And_Activate)
{
    TableConnector stateDbMirrorSession(m_state_db.get(), APP_MIRROR_SESSION_TABLE_NAME);
    TableConnector confDbMirrorSession(m_config_db.get(), CFG_MIRROR_SESSION_TABLE_NAME);
    auto mirror_orch = MirrorOrch(stateDbMirrorSession, confDbMirrorSession, gPortsOrch, gRouteOrch, gNeighOrch, gFdbOrch);

    auto consumerExt = std::unique_ptr<ConsumerExtend>(new ConsumerExtend(
        new ConsumerStateTable(m_config_db.get(), CFG_MIRROR_SESSION_TABLE_NAME, 1, 1), &mirror_orch, CFG_MIRROR_SESSION_TABLE_NAME));
    std::string mirror_session_name = "mirror_session_1";
    auto mirror_cfg = std::deque<KeyOpFieldsValuesTuple>(
        { { mirror_session_name,
            SET_COMMAND,
            {
                { "src_ip", "1.1.1.1" },
                { "dst_ip", "2.2.2.2" },
                { "gre_type", "0x6558" },
                { "dscp", "8" },
                { "ttl", "100" },
                { "queue", "0" },
            } } });
    consumerExt->addToSync(mirror_cfg);

    static_cast<Orch*>(&mirror_orch)->doTask(*consumerExt);

    bool session_state;
    ASSERT_TRUE(mirror_orch.getSessionStatus(mirror_session_name, session_state)); // session exist
    ASSERT_TRUE(session_state == false); //session inactive
    //TODO: validate session fields

    auto setData = std::deque<KeyOpFieldsValuesTuple>(
          { { "Ethernet0:192.168.1.1/24",
              SET_COMMAND,
              { }
          } });
    consumerExt->addToSync(setData);
    static_cast<Orch*>(gIntfsOrch)->doTask(*consumerExt);

    setData = std::deque<KeyOpFieldsValuesTuple>(
          { { "Ethernet0:2.2.2.2",
              SET_COMMAND,
              {{"neigh", "00:01:02:03:04:05"}}
          } });
    consumerExt->addToSync(setData);
    static_cast<Orch*>(gNeighOrch)->doTask(*consumerExt);

    ASSERT_TRUE(mirror_orch.getSessionStatus(mirror_session_name, session_state)); // session exist
    ASSERT_TRUE(session_state == true); //session active

}
