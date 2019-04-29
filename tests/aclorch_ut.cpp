#include "ut_helper.h"

#include "orchdaemon.h"

extern sai_object_id_t gSwitchId;

extern CrmOrch* gCrmOrch;
extern PortsOrch* gPortsOrch;
extern RouteOrch* gRouteOrch;
extern IntfsOrch* gIntfsOrch;
extern NeighOrch* gNeighOrch;
extern FdbOrch* gFdbOrch;
extern AclOrch* gAclOrch;
extern MirrorOrch* gMirrorOrch;
extern VRFOrch* gVrfOrch;

extern sai_acl_api_t* sai_acl_api;
extern sai_switch_api_t* sai_switch_api;
extern sai_port_api_t* sai_port_api;
extern sai_vlan_api_t* sai_vlan_api;
extern sai_bridge_api_t* sai_bridge_api;
extern sai_route_api_t* sai_route_api;

namespace nsAclOrchTest {

using namespace std;

TEST(ConvertTest, field_value_to_attribute)
{
    auto v = std::vector<swss::FieldValueTuple>({ { "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST", "2:SAI_ACL_BIND_POINT_TYPE_PORT,SAI_ACL_BIND_POINT_TYPE_LAG" },
        { "SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE", "true" },
        { "SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE", "true" },
        { "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL", "true" },
        { "SAI_ACL_TABLE_ATTR_FIELD_SRC_IP", "true" },
        { "SAI_ACL_TABLE_ATTR_FIELD_DST_IP", "true" },
        { "SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT", "true" },
        { "SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT", "true" },
        { "SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS", "true" },
        { "SAI_ACL_TABLE_ATTR_FIELD_ACL_RANGE_TYPE", "2:SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE,SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE" },
        { "SAI_ACL_TABLE_ATTR_ACL_STAGE", "SAI_ACL_STAGE_INGRESS" } });
    SaiAttributeList attr_list(SAI_OBJECT_TYPE_ACL_TABLE, v, false);

    auto l = attr_list.get_attr_list();
    auto c = attr_list.get_attr_count();
    ASSERT_TRUE(c == 11);
}

class ConsumerExtend_Dont_Use : public Consumer {
public:
    ConsumerExtend_Dont_Use(ConsumerTableBase* select, Orch* orch, const string& name)
        : Consumer(select, orch, name)
    {
    }

    size_t addToSync(std::deque<KeyOpFieldsValuesTuple>& entries)
    {
        Consumer::addToSync(entries);
    }

    void clear()
    {
        Consumer::m_toSync.clear();
    }
};

struct AclTestBase : public ::testing::Test {
    std::vector<int32_t*> m_s32list_pool;

    virtual ~AclTestBase()
    {
        for (auto p : m_s32list_pool) {
            free(p);
        }
    }
};

struct AclTest : public AclTestBase {

    struct CreateAclResult {
        bool ret_val;

        std::vector<sai_attribute_t> attr_list;
    };

    std::shared_ptr<swss::DBConnector> m_config_db;

    AclTest()
    {
        m_config_db = std::make_shared<swss::DBConnector>(CONFIG_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    }

    void SetUp() override
    {
        // FIXME: doesn't use global variable !!
        assert(gCrmOrch == nullptr);
        gCrmOrch = new CrmOrch(m_config_db.get(), CFG_CRM_TABLE_NAME);
    }

    void TearDown() override
    {
        delete gCrmOrch; // FIXME: using auto ptr
        gCrmOrch = nullptr;
    }

    std::shared_ptr<CreateAclResult> createAclTable_4(AclTable& acl)
    {
        assert(sai_acl_api == nullptr);

        sai_acl_api = new sai_acl_api_t();
        auto sai_acl = std::shared_ptr<sai_acl_api_t>(sai_acl_api, [](sai_acl_api_t* p) {
            delete p;
            sai_acl_api = nullptr;
        });

        auto ret = std::make_shared<CreateAclResult>();

        auto spy = SpyOn<SAI_API_ACL, SAI_OBJECT_TYPE_ACL_TABLE>(&sai_acl_api->create_acl_table);
        spy->callFake([&](sai_object_id_t* oid, sai_object_id_t, uint32_t attr_count, const sai_attribute_t* attr_list) -> sai_status_t {
            for (auto i = 0; i < attr_count; ++i) {
                ret->attr_list.emplace_back(attr_list[i]);
            }
            return SAI_STATUS_SUCCESS;
        });

        ret->ret_val = acl.create();
        return ret;
    }
};

TEST_F(AclTest, create_default_acl_table_4)
{
    AclTable acltable;
    acltable.type = ACL_TABLE_L3;
    auto res = createAclTable_4(acltable);

    ASSERT_TRUE(res->ret_val == true);

    auto v = std::vector<swss::FieldValueTuple>(
        { { "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST", "2:SAI_ACL_BIND_POINT_TYPE_PORT,SAI_ACL_BIND_POINT_TYPE_LAG" },
            { "SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE", "true" },
            { "SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE", "true" },
            { "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL", "true" },
            { "SAI_ACL_TABLE_ATTR_FIELD_SRC_IP", "true" },
            { "SAI_ACL_TABLE_ATTR_FIELD_DST_IP", "true" },
            { "SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT", "true" },
            { "SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT", "true" },
            { "SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS", "true" },
            { "SAI_ACL_TABLE_ATTR_FIELD_ACL_RANGE_TYPE", "2:SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE,SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE" },
            { "SAI_ACL_TABLE_ATTR_ACL_STAGE", "SAI_ACL_STAGE_INGRESS" } });
    SaiAttributeList attr_list(SAI_OBJECT_TYPE_ACL_TABLE, v, false);

    ASSERT_TRUE(Check::AttrListEq_Miss_objecttype_Dont_Use(res->attr_list, attr_list));
}

// struct AclTestRedis_Old_Test_Refine_Then_Remove : public ::testing::Test {
//     AclTestRedis_Old_Test_Refine_Then_Remove() {}
//
//     void start_server_and_remote_all_data()
//     {
//         //.....
//     }
//
//     // override
//     void SetUp() override { start_server_and_remote_all_data(); }
//
//     void InjectData(int instance, void* data)
//     {
//         if (instance == APPL_DB) {
//             ///
//         } else if (instance == CONFIG_DB) {
//             ///
//         } else if (instance == STATE_DB) {
//             ///
//         }
//     }
//
//     int GetData(int instance) { return 0; }
// };
//
// TEST_F(AclTestRedis_Old_Test_Refine_Then_Remove, create_default_acl_table_on_redis)
// {
//     sai_status_t status;
//     AclTable acltable;
//
//     // DBConnector appl_db(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
//     DBConnector config_db(CONFIG_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
//     // DBConnector state_db(STATE_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
//
//     initSaiApi();
//     gCrmOrch = new CrmOrch(&config_db, CFG_CRM_TABLE_NAME);
//
//     sai_attribute_t attr;
//     vector<sai_attribute_t> attrs;
//     attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
//     attr.value.booldata = true;
//     attrs.push_back(attr);
//
//     status = sai_switch_api->create_switch(&gSwitchId, (uint32_t)attrs.size(),
//         attrs.data());
//     ASSERT_EQ(status, SAI_STATUS_SUCCESS);
//     sleep(1);
//
//     acltable.create();
//     sleep(2);
//     // validate ...
//     // auto x = GetData(ASIC_DB);
//     {
//         redisContext* c;
//         redisReply* reply;
//
//         struct timeval timeout = { 1, 500000 }; // 1.5 seconds
//         c = redisConnectUnixWithTimeout(DBConnector::DEFAULT_UNIXSOCKET, timeout);
//         if (c == NULL || c->err) {
//             ASSERT_TRUE(0);
//         }
//
//         reply = (redisReply*)redisCommand(c, "SELECT %d", ASIC_DB);
//         ASSERT_NE(reply->type, REDIS_REPLY_ERROR);
//         // printf("SELECT: %s\n", reply->str);
//         freeReplyObject(reply);
//
//         reply = (redisReply*)redisCommand(c, " LRANGE %s 0 -1",
//             "ASIC_STATE_KEY_VALUE_OP_QUEUE");
//         ASSERT_NE(reply->type, REDIS_REPLY_ERROR);
//         ASSERT_EQ(reply->elements, 6);
//         for (int i = 0; i < reply->elements; ++i) {
//             redisReply* sub_reply = reply->element[i];
//             // printf("(%d)LRANGE: %s\n", i, sub_reply->str);
//
//             if (i == 0) {
//                 string op = string("Screate");
//                 ASSERT_TRUE(0 == strncmp(op.c_str(), sub_reply->str, op.length()));
//             }
//         }
//         freeReplyObject(reply);
//
//         reply = (redisReply*)redisCommand(c, "FLUSHALL");
//         freeReplyObject(reply);
//         redisFree(c);
//     }
//
//     delete gCrmOrch;
//     sai_api_uninitialize();
// }

struct MockAclOrch {
    AclOrch* m_aclOrch;
    swss::DBConnector* config_db;

    MockAclOrch(AclOrch* aclOrch, swss::DBConnector* config_db)
        : m_aclOrch(aclOrch)
        , config_db(config_db)
    {
    }

    operator const AclOrch*() const
    {
        return m_aclOrch;
    }

    static size_t consumerAddToSync(Consumer* consumer, const std::deque<KeyOpFieldsValuesTuple>& entries)
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

    void doAclTableTask(const std::deque<KeyOpFieldsValuesTuple>& entries)
    {
        auto consumer = std::unique_ptr<Consumer>(new Consumer(
            new swss::ConsumerStateTable(config_db, CFG_ACL_TABLE_NAME, 1, 1), m_aclOrch, CFG_ACL_TABLE_NAME));

        consumerAddToSync(consumer.get(), entries);

        static_cast<Orch*>(m_aclOrch)->doTask(*consumer);
    }

    void doAclRuleTask(const std::deque<KeyOpFieldsValuesTuple>& entries)
    {
        auto consumer = std::unique_ptr<Consumer>(new Consumer(
            new swss::ConsumerStateTable(config_db, CFG_ACL_RULE_TABLE_NAME, 1, 1), m_aclOrch, CFG_ACL_RULE_TABLE_NAME));

        consumerAddToSync(consumer.get(), entries);

        static_cast<Orch*>(m_aclOrch)->doTask(*consumer);
    }

    sai_object_id_t getTableById(const string& table_id)
    {
        return m_aclOrch->getTableById(table_id);
    }

    const map<sai_object_id_t, AclTable>& getAclTables() const
    {
        return Portal::AclOrchInternal::getAclTables(m_aclOrch);
    }
};

struct AclOrchTest : public AclTest {

    std::shared_ptr<swss::DBConnector> m_app_db;
    std::shared_ptr<swss::DBConnector> m_config_db;
    std::shared_ptr<swss::DBConnector> m_state_db;

    AclOrchTest()
    {
        // FIXME: move out from constructor
        m_app_db = std::make_shared<swss::DBConnector>(APPL_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_config_db = std::make_shared<swss::DBConnector>(CONFIG_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_state_db = std::make_shared<swss::DBConnector>(STATE_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    }

    static std::map<std::string, std::string> gProfileMap;
    static std::map<std::string, std::string>::iterator gProfileIter;

    static const char* profile_get_value(
        sai_switch_profile_id_t profile_id,
        const char* variable)
    {
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

        // std::map<std::string, std::string>::const_iterator it = gProfileMap.find(variable);
        // if (it == gProfileMap.end()) {
        //     printf("%s: NULL\n", variable);
        //     return NULL;
        // }
        //
        // return it->second.c_str();
    }

    static int profile_get_next_value(
        sai_switch_profile_id_t profile_id,
        const char** variable,
        const char** value)
    {
        if (value == NULL) {
            printf("resetting profile map iterator");

            gProfileIter = gProfileMap.begin();
            return 0;
        }

        if (variable == NULL) {
            printf("variable is null");
            return -1;
        }

        if (gProfileIter == gProfileMap.end()) {
            printf("iterator reached end");
            return -1;
        }

        *variable = gProfileIter->first.c_str();
        *value = gProfileIter->second.c_str();

        printf("key: %s:%s", *variable, *value);

        gProfileIter++;

        return 0;
    }

    void SetUp() override
    {
        AclTestBase::SetUp();

        assert(gAclOrch == nullptr);
        assert(gFdbOrch == nullptr);
        assert(gMirrorOrch == nullptr);
        assert(gRouteOrch == nullptr);
        assert(gNeighOrch == nullptr);
        assert(gIntfsOrch == nullptr);
        assert(gVrfOrch == nullptr);
        assert(gCrmOrch == nullptr);
        assert(gPortsOrch == nullptr);

        ///////////////////////////////////////////////////////////////////////
        // if (!strcmp(variable, "SAI_KEY_INIT_CONFIG_FILE")) {
        //     return "/usr/share/sai_2410.xml"; // FIXME: create a json file, and passing the path into test
        // } else if (!strcmp(variable, "KV_DEVICE_MAC_ADDRESS")) {
        //     return "20:03:04:05:06:00";
        // } else if (!strcmp(variable, "SAI_KEY_L3_ROUTE_TABLE_SIZE")) {
        //     return "1000";
        // } else if (!strcmp(variable, "SAI_KEY_L3_NEIGHBOR_TABLE_SIZE")) {
        //     return "2000";
        // } else if (!strcmp(variable, "SAI_VS_SWITCH_TYPE")) {
        //     return "SAI_VS_SWITCH_TYPE_BCM56850";
        // }

        gProfileMap.emplace("SAI_VS_SWITCH_TYPE", "SAI_VS_SWITCH_TYPE_BCM56850");
        gProfileMap.emplace("KV_DEVICE_MAC_ADDRESS", "20:03:04:05:06:00");

        sai_service_method_table_t test_services = {
            AclOrchTest::profile_get_value,
            AclOrchTest::profile_get_next_value
        };

        auto status = sai_api_initialize(0, (sai_service_method_table_t*)&test_services);
        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);

#if WITH_SAI == LIBVS
        sai_switch_api = const_cast<sai_switch_api_t*>(&vs_switch_api);
        sai_acl_api = const_cast<sai_acl_api_t*>(&vs_acl_api);
        sai_port_api = const_cast<sai_port_api_t*>(&vs_port_api);
        sai_vlan_api = const_cast<sai_vlan_api_t*>(&vs_vlan_api);
        sai_bridge_api = const_cast<sai_bridge_api_t*>(&vs_bridge_api);
        sai_route_api = const_cast<sai_route_api_t*>(&vs_route_api);
#endif

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

        TableConnector confDbAclTable(m_config_db.get(), CFG_ACL_TABLE_NAME);
        TableConnector confDbAclRuleTable(m_config_db.get(), CFG_ACL_RULE_TABLE_NAME);

        const int portsorch_base_pri = 40;

        vector<table_name_with_pri_t> ports_tables = {
            { APP_PORT_TABLE_NAME, portsorch_base_pri + 5 },
            { APP_VLAN_TABLE_NAME, portsorch_base_pri + 2 },
            { APP_VLAN_MEMBER_TABLE_NAME, portsorch_base_pri },
            { APP_LAG_TABLE_NAME, portsorch_base_pri + 4 },
            { APP_LAG_MEMBER_TABLE_NAME, portsorch_base_pri }
        };

        // FIXME: doesn't use global variable !!
        assert(gPortsOrch == nullptr);
        gPortsOrch = new PortsOrch(m_app_db.get(), ports_tables);

        // FIXME: doesn't use global variable !!
        assert(gCrmOrch == nullptr);
        gCrmOrch = new CrmOrch(m_config_db.get(), CFG_CRM_TABLE_NAME);

        // FIXME: doesn't use global variable !!
        assert(gVrfOrch == nullptr);
        gVrfOrch = new VRFOrch(m_app_db.get(), APP_VRF_TABLE_NAME);

        // FIXME: doesn't use global variable !!
        assert(gIntfsOrch == nullptr);
        gIntfsOrch = new IntfsOrch(m_app_db.get(), APP_INTF_TABLE_NAME, gVrfOrch);

        // FIXME: doesn't use global variable !!
        assert(gNeighOrch == nullptr);
        gNeighOrch = new NeighOrch(m_app_db.get(), APP_NEIGH_TABLE_NAME, gIntfsOrch);

        // FIXME: doesn't use global variable !!
        assert(gRouteOrch == nullptr);
        gRouteOrch = new RouteOrch(m_app_db.get(), APP_ROUTE_TABLE_NAME, gNeighOrch);

        TableConnector applDbFdb(m_app_db.get(), APP_FDB_TABLE_NAME);
        TableConnector stateDbFdb(m_state_db.get(), STATE_FDB_TABLE_NAME);

        // FIXME: doesn't use global variable !!
        assert(gFdbOrch == nullptr);
        gFdbOrch = new FdbOrch(applDbFdb, stateDbFdb, gPortsOrch);

        TableConnector stateDbMirrorSession(m_state_db.get(), APP_MIRROR_SESSION_TABLE_NAME);
        TableConnector confDbMirrorSession(m_config_db.get(), CFG_MIRROR_SESSION_TABLE_NAME);

        // FIXME: doesn't use global variable !!
        assert(gMirrorOrch == nullptr);
        gMirrorOrch = new MirrorOrch(stateDbMirrorSession, confDbMirrorSession,
            gPortsOrch, gRouteOrch, gNeighOrch, gFdbOrch);

        vector<TableConnector> acl_table_connectors = { confDbAclTable, confDbAclRuleTable };

        // FIXME: Using local variable or data member for aclorch ... ??
        gAclOrch = new AclOrch(acl_table_connectors, gPortsOrch, gMirrorOrch,
            gNeighOrch, gRouteOrch);

        auto consumerStateTable = new ConsumerStateTable(m_app_db.get(), APP_PORT_TABLE_NAME, 1, 1); // free by consumerStateTable
        auto consumerExt = std::make_shared<ConsumerExtend_Dont_Use>(consumerStateTable, gPortsOrch, APP_PORT_TABLE_NAME);

        auto setData = std::deque<KeyOpFieldsValuesTuple>(
            { { "PortInitDone",
                EMPTY_PREFIX,
                { { "", "" } } } });
        consumerExt->addToSync(setData);

        Consumer* consumer = consumerExt.get();
        static_cast<Orch*>(gPortsOrch)->doTask(*consumer);
    }

    void TearDown() override
    {
        AclTestBase::TearDown();

        delete gAclOrch; // FIXME: using auto ptr
        gAclOrch = nullptr;
        delete gFdbOrch; // FIXME: using auto ptr
        gFdbOrch = nullptr;
        delete gMirrorOrch; // FIXME: using auto ptr
        gMirrorOrch = nullptr;
        delete gRouteOrch; // FIXME: using auto ptr
        gRouteOrch = nullptr;
        delete gNeighOrch; // FIXME: using auto ptr
        gNeighOrch = nullptr;
        delete gIntfsOrch; // FIXME: using auto ptr
        gIntfsOrch = nullptr;
        delete gVrfOrch; // FIXME: using auto ptr
        gVrfOrch = nullptr;
        delete gCrmOrch; // FIXME: using auto ptr
        gCrmOrch = nullptr;
        delete gPortsOrch; // FIXME: using auto ptr
        gPortsOrch = nullptr;

        ///////////////////////////////////////////////////////////////////////

        auto status = sai_switch_api->remove_switch(gSwitchId);
        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);
        gSwitchId = 0;

        sai_api_uninitialize();

        sai_switch_api = nullptr;
        sai_acl_api = nullptr;
    }

    std::shared_ptr<MockAclOrch> createAclOrch()
    {
        return std::make_shared<MockAclOrch>(gAclOrch, m_config_db.get());
    }

    std::shared_ptr<SaiAttributeList> getAclTableAttributeList(sai_object_type_t objecttype, const AclTable& acl_table)
    {
        // const sai_object_type_t objecttype = SAI_OBJECT_TYPE_ACL_TABLE; // <----------
        std::vector<swss::FieldValueTuple> fields;

        switch (acl_table.type) {
        case ACL_TABLE_L3:
            // sai_object_type_t objecttype = SAI_OBJECT_TYPE_ACL_TABLE; // <----------
            // auto exp_fields = std::vector<swss::FieldValueTuple>( // <----------
            //     { { "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST", "2:SAI_ACL_BIND_POINT_TYPE_PORT,SAI_ACL_BIND_POINT_TYPE_LAG" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE", "true" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE", "true" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL", "true" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_SRC_IP", "true" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_DST_IP", "true" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT", "true" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT", "true" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS", "true" },
            //         { "SAI_ACL_TABLE_ATTR_FIELD_ACL_RANGE_TYPE", "2:SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE,SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE" },
            //         { "SAI_ACL_TABLE_ATTR_ACL_STAGE", "SAI_ACL_STAGE_INGRESS" } });
            // // SaiAttributeList exp_attrlist(objecttype, exp_fields, false);

            fields.push_back({ "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST", "2:SAI_ACL_BIND_POINT_TYPE_PORT,SAI_ACL_BIND_POINT_TYPE_LAG" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_SRC_IP", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_DST_IP", "true" });

            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_ACL_RANGE_TYPE", "2:SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE,SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_ACL_STAGE", "SAI_ACL_STAGE_INGRESS" });
            break;

        case ACL_TABLE_L3V6:
            // auto exp_fields = std::vector<swss::FieldValueTuple>( // <----------
            // { { "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST", "2:SAI_ACL_BIND_POINT_TYPE_PORT,SAI_ACL_BIND_POINT_TYPE_LAG" },
            //     { "SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE", "true" },
            //     { "SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE", "true" },
            //     { "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL", "true" },
            //     { "SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6", "true" },
            //     //                          ^^^^^^^^ sip v6
            //     { "SAI_ACL_TABLE_ATTR_FIELD_DST_IPV6", "true" },
            //     //                          ^^^^^^^^ dip v6
            //     { "SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT", "true" },
            //     { "SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT", "true" },
            //     { "SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS", "true" },
            //     { "SAI_ACL_TABLE_ATTR_FIELD_ACL_RANGE_TYPE", "2:SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE,SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE" },
            //     { "SAI_ACL_TABLE_ATTR_ACL_STAGE", "SAI_ACL_STAGE_INGRESS" } });

            fields.push_back({ "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST", "2:SAI_ACL_BIND_POINT_TYPE_PORT,SAI_ACL_BIND_POINT_TYPE_LAG" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_DST_IPV6", "true" });

            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS", "true" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_FIELD_ACL_RANGE_TYPE", "2:SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE,SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE" });
            fields.push_back({ "SAI_ACL_TABLE_ATTR_ACL_STAGE", "SAI_ACL_STAGE_INGRESS" });
            break;

        default:
            assert(false);
        }

        return std::shared_ptr<SaiAttributeList>(new SaiAttributeList(objecttype, fields, false));
    }

    std::shared_ptr<SaiAttributeList> getAclRuleAttributeList(sai_object_type_t objecttype, const AclRule& acl_rule, sai_object_id_t acl_table_oid, const AclTable& acl_table)
    {
        std::vector<swss::FieldValueTuple> fields;

        auto table_id = sai_serialize_object_id(acl_table_oid);
        auto counter_id = sai_serialize_object_id(const_cast<AclRule&>(acl_rule).getCounterOid()); // FIXME: getcounterOid() should be const

        switch (acl_table.type) {
        case ACL_TABLE_L3:
            //     auto table_id = sai_serialize_object_id(acl_table_oid);
            //     auto counter_id = sai_serialize_object_id(acl_rule->getCounterOid());
            //
            //     sai_object_type_t objecttype = SAI_OBJECT_TYPE_ACL_ENTRY; // <----------
            //     auto exp_fields = std::vector<swss::FieldValueTuple>( // <----------
            //         {
            //             { "SAI_ACL_ENTRY_ATTR_TABLE_ID", table_id },
            //             { "SAI_ACL_ENTRY_ATTR_PRIORITY", "0" },
            //             { "SAI_ACL_ENTRY_ATTR_ADMIN_STATE", "true" },
            //             { "SAI_ACL_ENTRY_ATTR_ACTION_COUNTER", counter_id },
            //
            //             // cfg fields
            //             { "SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP", "1.2.3.4&mask:255.255.255.255" },
            //             { "SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION", "1" }
            //             //                                            SAI_PACKET_ACTION_FORWARD
            //
            //         });
            //     SaiAttributeList exp_attrlist(objecttype, exp_fields, false);

            fields.push_back({ "SAI_ACL_ENTRY_ATTR_TABLE_ID", table_id });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_PRIORITY", "0" });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_ADMIN_STATE", "true" });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_ACTION_COUNTER", counter_id });

            fields.push_back({ "SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP", "1.2.3.4&mask:255.255.255.255" });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION", "1" });
            break;

        case ACL_TABLE_L3V6:
            // auto exp_fields = std::vector<swss::FieldValueTuple>( // <----------
            // {
            //     { "SAI_ACL_ENTRY_ATTR_TABLE_ID", table_id },
            //     { "SAI_ACL_ENTRY_ATTR_PRIORITY", "0" },
            //     { "SAI_ACL_ENTRY_ATTR_ADMIN_STATE", "true" },
            //     { "SAI_ACL_ENTRY_ATTR_ACTION_COUNTER", counter_id },
            //
            //     // cfg fields
            //     { "SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6", "::1.2.3.4&mask:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" },
            //     { "SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION", "1" }
            //     //                                            SAI_PACKET_ACTION_FORWARD
            //
            // });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_TABLE_ID", table_id });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_PRIORITY", "0" });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_ADMIN_STATE", "true" });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_ACTION_COUNTER", counter_id });

            fields.push_back({ "SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6", "::1.2.3.4&mask:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" });
            fields.push_back({ "SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION", "1" });
            break;

        default:
            assert(false);
        }

        return std::shared_ptr<SaiAttributeList>(new SaiAttributeList(objecttype, fields, false));
    }

    bool validateAclRule(const std::string acl_rule_sid, const AclRule& acl_rule, sai_object_id_t acl_table_oid, const AclTable& acl_table)
    {
        sai_object_type_t objecttype = SAI_OBJECT_TYPE_ACL_ENTRY; // <----------
        auto exp_attrlist_2 = getAclRuleAttributeList(objecttype, acl_rule, acl_table_oid, acl_table);

        // auto it = acl_tables.find(acl_table_oid);
        // ASSERT_TRUE(it != acl_tables.end());

        // //auto acl_rule_oid = it->second.rules.begin()->first;
        // auto acl_rule = it->second.rules.begin()->second; // FIXME: assumpt only one rule inside
        auto acl_rule_oid = Portal::AclRuleInternal::getRuleOid(&acl_rule);

        {
            //     auto table_id = sai_serialize_object_id(acl_table_oid);
            //     auto counter_id = sai_serialize_object_id(acl_rule->getCounterOid());
            //
            //     sai_object_type_t objecttype = SAI_OBJECT_TYPE_ACL_ENTRY; // <----------
            //     auto exp_fields = std::vector<swss::FieldValueTuple>( // <----------
            //         {
            //             { "SAI_ACL_ENTRY_ATTR_TABLE_ID", table_id },
            //             { "SAI_ACL_ENTRY_ATTR_PRIORITY", "0" },
            //             { "SAI_ACL_ENTRY_ATTR_ADMIN_STATE", "true" },
            //             { "SAI_ACL_ENTRY_ATTR_ACTION_COUNTER", counter_id },
            //
            //             // cfg fields
            //             { "SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP", "1.2.3.4&mask:255.255.255.255" },
            //             { "SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION", "1" }
            //             //                                            SAI_PACKET_ACTION_FORWARD
            //
            //         });
            //     SaiAttributeList exp_attrlist(objecttype, exp_fields, false);
            auto& exp_attrlist = *exp_attrlist_2;

            std::vector<sai_attribute_t> act_attr;

            for (int i = 0; i < exp_attrlist.get_attr_count(); ++i) {
                const auto attr = exp_attrlist.get_attr_list()[i];
                auto meta = sai_metadata_get_attr_metadata(objecttype, attr.id);

                if (meta == nullptr) {
                    return false;
                }

                sai_attribute_t new_attr = { 0 };
                new_attr.id = attr.id;

                switch (meta->attrvaluetype) {
                case SAI_ATTR_VALUE_TYPE_INT32_LIST:
                    new_attr.value.s32list.list = (int32_t*)malloc(sizeof(int32_t) * attr.value.s32list.count);
                    new_attr.value.s32list.count = attr.value.s32list.count;
                    m_s32list_pool.emplace_back(new_attr.value.s32list.list);
                    break;

                default:
                    std::cout << "";
                    ;
                }

                act_attr.emplace_back(new_attr);
            }

            auto status = sai_acl_api->get_acl_entry_attribute(acl_rule_oid, act_attr.size(), act_attr.data()); // <----------
            if (status != SAI_STATUS_SUCCESS) {
                return false;
            }

            auto b_attr_eq = Check::AttrListEq(objecttype, act_attr, exp_attrlist);
            if (!b_attr_eq) {
                return false;
            }
        }

        return true;
    }

    bool validateAclTable(sai_object_id_t acl_table_oid, const AclTable& acl_table)
    {
        const sai_object_type_t objecttype = SAI_OBJECT_TYPE_ACL_TABLE; // <----------
        auto exp_attrlist_2 = getAclTableAttributeList(objecttype, acl_table);

        {
            //     sai_object_type_t objecttype = SAI_OBJECT_TYPE_ACL_TABLE; // <----------
            //     auto exp_fields = std::vector<swss::FieldValueTuple>( // <----------
            //         { { "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST", "2:SAI_ACL_BIND_POINT_TYPE_PORT,SAI_ACL_BIND_POINT_TYPE_LAG" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE", "true" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE", "true" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL", "true" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_SRC_IP", "true" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_DST_IP", "true" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT", "true" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT", "true" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS", "true" },
            //             { "SAI_ACL_TABLE_ATTR_FIELD_ACL_RANGE_TYPE", "2:SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE,SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE" },
            //             { "SAI_ACL_TABLE_ATTR_ACL_STAGE", "SAI_ACL_STAGE_INGRESS" } });
            //     SaiAttributeList exp_attrlist(objecttype, exp_fields, false);
            auto& exp_attrlist = *exp_attrlist_2;

            std::vector<sai_attribute_t> act_attr;

            for (int i = 0; i < exp_attrlist.get_attr_count(); ++i) {
                const auto attr = exp_attrlist.get_attr_list()[i];
                auto meta = sai_metadata_get_attr_metadata(objecttype, attr.id);

                //ASSERT_TRUE(meta != nullptr);
                if (meta == nullptr) {
                    return false;
                }

                sai_attribute_t new_attr = { 0 };
                new_attr.id = attr.id;

                switch (meta->attrvaluetype) {
                case SAI_ATTR_VALUE_TYPE_INT32_LIST:
                    new_attr.value.s32list.list = (int32_t*)malloc(sizeof(int32_t) * attr.value.s32list.count);
                    new_attr.value.s32list.count = attr.value.s32list.count;
                    m_s32list_pool.emplace_back(new_attr.value.s32list.list);
                    break;

                default:
                    std::cout << "";
                    ;
                }

                act_attr.emplace_back(new_attr);
            }

            auto status = sai_acl_api->get_acl_table_attribute(acl_table_oid, act_attr.size(), act_attr.data());
            if (status != SAI_STATUS_SUCCESS) {
                return false;
            }

            auto b_attr_eq = Check::AttrListEq(objecttype, act_attr, exp_attrlist);
            if (!b_attr_eq) {
                return false;
            }
        }

        for (const auto& sid_acl_rule : acl_table.rules) {
            auto b_valid = validateAclRule(sid_acl_rule.first, *sid_acl_rule.second, acl_table_oid, acl_table);
            if (!b_valid) {
                return false;
            }
        }

        return true;
    }

    // consistency validation with CRM
    bool validateResourceCountWithCrm(const AclOrch* aclOrch, CrmOrch* crmOrch)
    {
        auto resourceMap = Portal::CrmOrchInternal::getResourceMap(crmOrch);
        auto ifpPortKey = Portal::CrmOrchInternal::getCrmAclKey(crmOrch, SAI_ACL_STAGE_INGRESS, SAI_ACL_BIND_POINT_TYPE_PORT);
        auto efpPortKey = Portal::CrmOrchInternal::getCrmAclKey(crmOrch, SAI_ACL_STAGE_EGRESS, SAI_ACL_BIND_POINT_TYPE_PORT);

        auto ifpPortAclTableCount = resourceMap.at(CrmResourceType::CRM_ACL_TABLE).countersMap[ifpPortKey].usedCounter;
        auto efpPortAclTableCount = resourceMap.at(CrmResourceType::CRM_ACL_TABLE).countersMap[efpPortKey].usedCounter;

        return ifpPortAclTableCount + efpPortAclTableCount == Portal::AclOrchInternal::getAclTables(aclOrch).size();

        // TODO: add rule check
    }

    // leakage check
    bool validateResourceCountWithLowerLayerDb(const AclOrch* aclOrch)
    {
#if WITH_SAI == LIBVS
        {
            auto& aclTableHash = g_switch_state_map.at(gSwitchId)->objectHash.at(SAI_OBJECT_TYPE_ACL_TABLE);

            return aclTableHash.size() == Portal::AclOrchInternal::getAclTables(aclOrch).size();
        }

        // TODO: add rule check
#endif

        return true;
    }

    // validate consistency between aclOrch and mock data (via SAI)
    bool validateLowerLayerDb(const MockAclOrch* orch)
    {
        assert(orch != nullptr);

        if (!validateResourceCountWithCrm(orch->m_aclOrch, gCrmOrch)) {
            return false;
        }

        if (!validateResourceCountWithLowerLayerDb(orch->m_aclOrch)) {
            return false;
        }

        const auto& acl_tables = orch->getAclTables();

        for (const auto& id_acl_table : acl_tables) {
            if (!validateAclTable(id_acl_table.first, id_acl_table.second)) {
                return false;
            }
        }

        return true;
    }

    bool validateAclTableByConfOp(const AclTable& acl_table, const std::vector<swss::FieldValueTuple>& values)
    {
        for (const auto& fv : values) {
            if (fv.first == TABLE_DESCRIPTION) {

            } else if (fv.first == TABLE_TYPE) {
                if (fv.second == TABLE_TYPE_L3) {
                    if (acl_table.type != ACL_TABLE_L3) {
                        return false;
                    }
                } else if (fv.second == TABLE_TYPE_L3V6) {
                    if (acl_table.type != ACL_TABLE_L3V6) {
                        return false;
                    }
                } else {
                    return false;
                }
            } else if (fv.first == TABLE_STAGE) {
                if (fv.second == TABLE_INGRESS) {
                    if (acl_table.stage != ACL_STAGE_INGRESS) {
                        return false;
                    }
                } else {
                    return false;
                }
            } else if (fv.first == TABLE_PORTS) {
            }
        }

        return true;
    }

    bool validateAclRuleAction(const AclRule& acl_rule, const std::string& attr_name, const std::string& attr_value)
    {
        const auto& rule_actions = Portal::AclRuleInternal::getActions(&acl_rule);

        if (attr_name == ACTION_PACKET_ACTION) {
            auto it = rule_actions.find(SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION);
            if (it == rule_actions.end()) {
                return false;
            }

            if (it->second.aclaction.enable != true) {
                return false;
            }

            if (attr_value == PACKET_ACTION_FORWARD) {
                if (it->second.aclaction.parameter.s32 != SAI_PACKET_ACTION_FORWARD) {
                    return false;
                }
            } else if (attr_value == PACKET_ACTION_DROP) {
                if (it->second.aclaction.parameter.s32 != SAI_PACKET_ACTION_DROP) {
                    return false;
                }
            } else {
                // unkonw attr_value
                return false;
            }
        } else {
            // unknow attr_name
            return false;
        }

        return true;
    }

    bool validateAclRuleMatch(const AclRule& acl_rule, const std::string& attr_name, const std::string& attr_value)
    {
        const auto& rule_matches = Portal::AclRuleInternal::getMatches(&acl_rule);

        if (attr_name == MATCH_SRC_IP | attr_name == MATCH_DST_IP) {
            auto it_field = rule_matches.find(attr_name == MATCH_SRC_IP ? SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP : SAI_ACL_ENTRY_ATTR_FIELD_DST_IP);
            if (it_field == rule_matches.end()) {
                return false;
            }

            char addr[20];
            sai_serialize_ip4(addr, it_field->second.aclfield.data.ip4);
            if (attr_value != addr) {
                return false;
            }

            char mask[20];
            sai_serialize_ip4(mask, it_field->second.aclfield.mask.ip4);
            if (std::string(mask) != "255.255.255.255") {
                return false;
            }
        } else if (attr_name == MATCH_SRC_IPV6) {
            auto it_field = rule_matches.find(SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6);
            if (it_field == rule_matches.end()) {
                return false;
            }

            char addr[46];
            sai_serialize_ip6(addr, it_field->second.aclfield.data.ip6);
            if (attr_value != addr) {
                return false;
            }

            char mask[46];
            sai_serialize_ip6(mask, it_field->second.aclfield.mask.ip6);
            if (std::string(mask) != "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff") {
                return false;
            }

        } else {
            // unknow attr_name
            return false;
        }

        return true;
    }

    bool validateAclRuleByConfOp(const AclRule& acl_rule, const std::vector<swss::FieldValueTuple>& values)
    {
        for (const auto& fv : values) {
            auto attr_name = fv.first;
            auto attr_value = fv.second;

            if (attr_name == ACTION_PACKET_ACTION) {
                if (!validateAclRuleAction(acl_rule, attr_name, attr_value)) {
                    return false;
                }
            } else if (attr_name == MATCH_SRC_IP | attr_name == MATCH_DST_IP | attr_name == MATCH_SRC_IPV6) {
                if (!validateAclRuleMatch(acl_rule, attr_name, attr_value)) {
                    return false;
                }
            } else {
                // unknow attr_name
                return false;
            }
        }
        return true;
    }
};

std::map<std::string, std::string> AclOrchTest::gProfileMap;
std::map<std::string, std::string>::iterator AclOrchTest::gProfileIter = AclOrchTest::gProfileMap.begin();

// AclTable::create
// validate the attribute list of each type {L3, L3V6 ....}, gCrmOrch will increase if create success

// AclTable::... not function to handler remove just call sai

// AclRule::create
// validate the attribute list will eq matchs + SAI_ACL_ENTRY_ATTR_TABLE_ID + SAI_ACL_ENTRY_ATTR_PRIORITY + SAI_ACL_ENTRY_ATTR_ACTION_COUNTER
// support SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE
// call sai_acl_api->create_acl_entry to create and incCrmAclTableUsedCounter

// AclTable::remove => remove rule, that will call AclRule::remove => sai_acl_api->remove_acl_entry

//
// doAclTableTask
//
// using op=set_command to create acl table
//      passing TABLE_DESCRIPTION / TABLE_TYPE / TABLE_PORTS / TABLE_STAGE to create acl_table
//          TABLE_TYPE / TABLE_PORTS / TABLE_STAGE is required
//      ignore if command include TABLE_SERVICES that is for COPP only
//      type = ACL_TABLE_CTRLPLANE <= what's that ?
//      if acl_table_is exist => remove then create new
//      if op successed, the acl will be create in m_AclTables (ref: AclTable::create) and lower layer (SAI, ref: sai->create_acl_table), and bind to ports (TABLE_PORTS ? aclTable.ports)
//
// using op=del_command to delete acl table
//      if acl_table_id is not exist => do nothing
//      if acl_table_id will be remove from internal table and sai, (unbind port before remove)
//
// unknow op will be ignored
//
//
// PS: m_AclTables keep controlplan acl too
//
// doAclRuleTask
//
// using op=set_command to create acl rule
//    ignore is acl_id is Skip the control plane rules
//    using AclRule::makeShared to create tmpl rule object
//    fill priority / match / action then add rule to acl table (ref: AclTable::add() and AclRule::create())
//                    (json to matchs and action convert)
//
// using op=del_command to delete acl rule
//
// unknow op will be ignored
//

//
// The order will be doAclTableTask => doAclRuleTask => AclTable => AclRule ....
//

// When received ACL table SET_COMMAND, orchagent can create corresponding ACL.
// When received ACL table DEL_COMMAND, orchagent can delete corresponding ACL.
//
// Input by type = {L3, L3V6, PFCCMD ...}, stage = {INGRESS, EGRESS}.
//
// Using fixed ports = {"1,2"} for now.
// The bind operations will be another separately test cases.
TEST_F(AclOrchTest, ACL_Creation_and_Destorying)
{
    auto orch = createAclOrch();

    for (const auto& acl_table_type : { TABLE_TYPE_L3, TABLE_TYPE_L3V6 }) {
        for (const auto& acl_table_stage : { TABLE_INGRESS /*, TABLE_EGRESS*/ }) {
            std::string acl_table_id = "acl_table_1";

            auto kvfAclTable = std::deque<KeyOpFieldsValuesTuple>(
                { { acl_table_id,
                    SET_COMMAND,
                    { { TABLE_DESCRIPTION, "filter source IP" },
                        { TABLE_TYPE, acl_table_type },
                        { TABLE_STAGE, acl_table_stage },
                        { TABLE_PORTS, "1,2" } } } });
            // FIXME:                  ^^^^^^^^^^^^^ fixed port

            orch->doAclTableTask(kvfAclTable);

            auto oid = orch->getTableById(acl_table_id);
            ASSERT_TRUE(oid != SAI_NULL_OBJECT_ID);

            const auto& acl_tables = orch->getAclTables();

            auto it = acl_tables.find(oid);
            ASSERT_TRUE(it != acl_tables.end());

            const auto& acl_table = it->second;

            ASSERT_TRUE(validateAclTableByConfOp(acl_table, kfvFieldsValues(kvfAclTable.front())));
            ASSERT_TRUE(validateLowerLayerDb(orch.get()));

            // delete acl table ...

            kvfAclTable = std::deque<KeyOpFieldsValuesTuple>(
                { { acl_table_id,
                    DEL_COMMAND,
                    {} } });

            orch->doAclTableTask(kvfAclTable);

            oid = orch->getTableById(acl_table_id);
            ASSERT_TRUE(oid == SAI_NULL_OBJECT_ID);

            ASSERT_TRUE(validateLowerLayerDb(orch.get()));
        }
    }
}

// When received ACL rule SET_COMMAND, orchagent can create corresponding ACL rule.
// When received ACL rule DEL_COMMAND, orchagent can delete corresponding ACL rule.
//
// Verify ACL table type = { L3 }, stage = { INGRESS, ENGRESS }
// Input by matchs = { SIP, DIP ...}, pkg:actions = { FORWARD, DROP ... }
//
TEST_F(AclOrchTest, L3Acl_Matches_Actions)
{
    std::string acl_table_id = "acl_table_1";
    std::string acl_rule_id = "acl_rule_1";

    auto orch = createAclOrch();

    auto kvfAclTable = std::deque<KeyOpFieldsValuesTuple>(
        { { acl_table_id,
            SET_COMMAND,
            { { TABLE_DESCRIPTION, "filter source IP" },
                { TABLE_TYPE, TABLE_TYPE_L3 },
                //            ^^^^^^^^^^^^^ L3 ACL
                { TABLE_STAGE, TABLE_INGRESS },
                // FIXME:      ^^^^^^^^^^^^^ only support / test for ingress ?
                { TABLE_PORTS, "1,2" } } } });
    // FIXME:                  ^^^^^^^^^^^^^ fixed port

    orch->doAclTableTask(kvfAclTable);

    // validate acl table ...

    auto acl_table_oid = orch->getTableById(acl_table_id);
    ASSERT_TRUE(acl_table_oid != SAI_NULL_OBJECT_ID);

    const auto& acl_tables = orch->getAclTables();
    auto it_table = acl_tables.find(acl_table_oid);
    ASSERT_TRUE(it_table != acl_tables.end());

    const auto& acl_table = it_table->second;

    ASSERT_TRUE(validateAclTableByConfOp(acl_table, kfvFieldsValues(kvfAclTable.front())));
    ASSERT_TRUE(validateLowerLayerDb(orch.get()));

    // add rule ...
    for (const auto& acl_rule_pkg_action : { PACKET_ACTION_FORWARD /*, PACKET_ACTION_DROP*/ }) {

        auto kvfAclRule = std::deque<KeyOpFieldsValuesTuple>({ { acl_table_id + "|" + acl_rule_id,
            SET_COMMAND,
            { { ACTION_PACKET_ACTION, acl_rule_pkg_action },

                // if (attr_name == ACTION_PACKET_ACTION || attr_name == ACTION_MIRROR_ACTION ||
                // attr_name == ACTION_DTEL_FLOW_OP || attr_name == ACTION_DTEL_INT_SESSION ||
                // attr_name == ACTION_DTEL_DROP_REPORT_ENABLE ||
                // attr_name == ACTION_DTEL_TAIL_DROP_REPORT_ENABLE ||
                // attr_name == ACTION_DTEL_FLOW_SAMPLE_PERCENT ||
                // attr_name == ACTION_DTEL_REPORT_ALL_PACKETS)
                //
                // TODO: required field (add new test cases for that ....)
                //

                { MATCH_SRC_IP, "1.2.3.4" },
                { MATCH_DST_IP, "4.3.2.1" } } } });

        // TODO: RULE_PRIORITY (important field)
        // TODO: MATCH_DSCP / MATCH_SRC_IPV6 || attr_name == MATCH_DST_IPV6

        orch->doAclRuleTask(kvfAclRule);

        // validate acl rule ...

        auto it_rule = acl_table.rules.find(acl_rule_id);
        ASSERT_TRUE(it_rule != acl_table.rules.end());

        ASSERT_TRUE(validateAclRuleByConfOp(*it_rule->second, kfvFieldsValues(kvfAclRule.front())));
        ASSERT_TRUE(validateLowerLayerDb(orch.get()));

        // delete acl rule ...

        kvfAclRule = std::deque<KeyOpFieldsValuesTuple>({ { acl_table_id + "|" + acl_rule_id,
            DEL_COMMAND,
            {} } });

        orch->doAclRuleTask(kvfAclRule);

        // validate acl rule ...

        it_rule = acl_table.rules.find(acl_rule_id);
        ASSERT_TRUE(it_rule == acl_table.rules.end());
        ASSERT_TRUE(validateLowerLayerDb(orch.get()));
    }
}

// When received ACL rule SET_COMMAND, orchagent can create corresponding ACL rule.
// When received ACL rule DEL_COMMAND, orchagent can delete corresponding ACL rule.
//
// Verify ACL table type = { L3V6 }, stage = { INGRESS, ENGRESS }
// Input by matchs = { SIP, DIP ...}, pkg:actions = { FORWARD, DROP ... }
//
TEST_F(AclOrchTest, L3V6Acl_Matches_Actions)
{
    std::string acl_table_id = "acl_table_1";
    std::string acl_rule_id = "acl_rule_1";

    auto orch = createAclOrch();

    auto kvfAclTable = std::deque<KeyOpFieldsValuesTuple>(
        { { acl_table_id,
            SET_COMMAND,
            { { TABLE_DESCRIPTION, "filter source IP" },
                { TABLE_TYPE, TABLE_TYPE_L3V6 },
                //            ^^^^^^^^^^^^^ L3V6 ACL
                { TABLE_STAGE, TABLE_INGRESS },
                // FIXME:      ^^^^^^^^^^^^^ only support / test for ingress ?
                { TABLE_PORTS, "1,2" } } } });
    // FIXME:                  ^^^^^^^^^^^^^ fixed port

    orch->doAclTableTask(kvfAclTable);

    // validate acl table ...

    auto acl_table_oid = orch->getTableById(acl_table_id);
    ASSERT_TRUE(acl_table_oid != SAI_NULL_OBJECT_ID);

    const auto& acl_tables = orch->getAclTables();
    auto it_table = acl_tables.find(acl_table_oid);
    ASSERT_TRUE(it_table != acl_tables.end());

    const auto& acl_table = it_table->second;

    ASSERT_TRUE(validateAclTableByConfOp(acl_table, kfvFieldsValues(kvfAclTable.front())));
    ASSERT_TRUE(validateLowerLayerDb(orch.get()));

    // add rule ...
    for (const auto& acl_rule_pkg_action : { PACKET_ACTION_FORWARD /*, PACKET_ACTION_DROP*/ }) {

        auto kvfAclRule = std::deque<KeyOpFieldsValuesTuple>({ { acl_table_id + "|" + acl_rule_id,
            SET_COMMAND,
            { { ACTION_PACKET_ACTION, acl_rule_pkg_action },

                // if (attr_name == ACTION_PACKET_ACTION || attr_name == ACTION_MIRROR_ACTION ||
                // attr_name == ACTION_DTEL_FLOW_OP || attr_name == ACTION_DTEL_INT_SESSION ||
                // attr_name == ACTION_DTEL_DROP_REPORT_ENABLE ||
                // attr_name == ACTION_DTEL_TAIL_DROP_REPORT_ENABLE ||
                // attr_name == ACTION_DTEL_FLOW_SAMPLE_PERCENT ||
                // attr_name == ACTION_DTEL_REPORT_ALL_PACKETS)
                //
                // TODO: required field (add new test cases for that ....)
                //

                { MATCH_SRC_IPV6, "::1.2.3.4" },
                /*{ MATCH_DST_IP, "4.3.2.1" }*/ } } });

        // TODO: RULE_PRIORITY (important field)
        // TODO: MATCH_DSCP / MATCH_SRC_IPV6 || attr_name == MATCH_DST_IPV6

        orch->doAclRuleTask(kvfAclRule);

        // validate acl rule ...

        auto it_rule = acl_table.rules.find(acl_rule_id);
        ASSERT_TRUE(it_rule != acl_table.rules.end());

        ASSERT_TRUE(validateAclRuleByConfOp(*it_rule->second, kfvFieldsValues(kvfAclRule.front())));
        ASSERT_TRUE(validateLowerLayerDb(orch.get()));

        // delete acl rule ...

        kvfAclRule = std::deque<KeyOpFieldsValuesTuple>({ { acl_table_id + "|" + acl_rule_id,
            DEL_COMMAND,
            {} } });

        orch->doAclRuleTask(kvfAclRule);

        // validate acl rule ...

        it_rule = acl_table.rules.find(acl_rule_id);
        ASSERT_TRUE(it_rule == acl_table.rules.end());
        ASSERT_TRUE(validateLowerLayerDb(orch.get()));
    }
}

/* FIXME: test case pseudo code
// 1. RULE_CTRL_UT_Apply_ACL()
for (type : {MAC, IP_STD, IP_EXTEND, IPv6_STD, IPv6_EXT})
    acl = new ACL()
    for (i : {0..max_number_ace_of_acl})
        ace = new ACE()
        a.addAce(ace)

    //for (port : {1,2,3, max+1, ...})
    //    for (dir : {ingress, egress})
    //        acl.bindTo(port, dir)
    //        validate rule

    //for (port : {1,2,3, max+1, ...})
    //    for (dir : {ingress, egress})
    //        acl.unBindTo(port, dir)
    //        validate rule

    delete acl


// 2. RULE_CTRL_UT_Set_QoS_With_Modifying_ACE_On_Fly()
//Bind Policy-map and ACL on port
p1 = create Policy Map
c = create Class Map
p1.add_class(c)
a2 = create ACL
c.add_acl(a2)

for (port : {1, 2, 3, 4 ...})
    for (dir : {ingress, egress})
        p1.bindTo(port, dir);
        validate rule
//Add on fly
ace = new ACE();
a2.addAce(ace)
validate rule
a2.removeAce(ace)
validate rule


// 3. RULE_OM_TEST_Max_ACE_Of_ACL()
acls = []
for (type : {MAC, IP_STD, IP_EXTEND, IPv6_STD, IPv6_EXT})
    acls[type] = new ACL()

for (type : {MAC, IP_STD, IP_EXTEND, IPv6_STD, IPv6_EXT})
    for (i : {0...max_number_ace-1})
        new_ace = new ACE()
        acls[type].add_ace(new_ace)

    delete acls[type] validate rule


// 4. RULE_OM_TEST_Max_ACE_Of_System()
ace_count = 0
acls = []
while true
    acl = new ACL()
    acls.push(acl)

    for (i : {0...max_number_ace_of_acl - 1})
        ace = new ACE()
        acl.add_ace(ace) if fail, goto exit
        ace_count++

exit :
check the ace_count == max_number_ace_of_system


// 5. RULE_OM_TEST_Max_ACL_Of_System()
acl_count = 0
while true
    acl = new ACL() if fail, break
    acl_count++;

check the acl_count == max_number_acl
*/
}
