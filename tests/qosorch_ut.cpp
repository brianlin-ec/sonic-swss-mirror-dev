#include "ut_helper.h"

#include "orchdaemon.h"

extern sai_object_id_t gSwitchId;

extern PortsOrch* gPortsOrch;

extern sai_switch_api_t* sai_switch_api;
extern sai_qos_map_api_t* sai_qos_map_api;
extern sai_wred_api_t* sai_wred_api;

namespace nsQosOrchTest {

using namespace std;

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

struct TestBase : public ::testing::Test {
    struct SetQosResult {
        bool ret_val;
        sai_object_id_t sai_object_id;
        std::vector<sai_attribute_t> attr_list;
    };

    static sai_status_t create_qos_map(sai_object_id_t* sai_object_id,
        sai_object_id_t switch_id,
        uint32_t attr_count,
        const sai_attribute_t* attr_list)
    {
        return that->create_qos_map_fn(sai_object_id, switch_id, attr_count,
            attr_list);
    }

    static sai_status_t create_wred(sai_object_id_t* sai_object_id,
        sai_object_id_t switch_id,
        uint32_t attr_count,
        const sai_attribute_t* attr_list)
    {
        return that->create_wred_fn(sai_object_id, switch_id, attr_count,
            attr_list);
    }

    static sai_status_t set_wred_attribute(sai_object_id_t* sai_object_id,
        const sai_attribute_t* attr_list)
    {
        return that->set_wred_attribute_fn(sai_object_id, attr_list);
    }

    static sai_status_t remove_wred(sai_object_id_t sai_object_id)
    {
        return that->remove_wred_fn(sai_object_id);
    }

    static TestBase* that;

    std::function<sai_status_t(sai_object_id_t*, sai_object_id_t, uint32_t,
        const sai_attribute_t*)>
        create_qos_map_fn;

    std::function<sai_status_t(sai_object_id_t*, sai_object_id_t, uint32_t,
        const sai_attribute_t*)>
        create_wred_fn;

    std::function<sai_status_t(sai_object_id_t*, const sai_attribute_t*)>
        set_wred_attribute_fn;

    std::function<sai_status_t(sai_object_id_t)>
        remove_wred_fn;

    std::shared_ptr<SetQosResult> setDscp2Tc(DscpToTcMapHandler& dscpToTc, KeyOpFieldsValuesTuple& tuple)
    {
        assert(sai_qos_map_api == nullptr);

        sai_qos_map_api = new sai_qos_map_api_t();
        auto sai_qos = std::shared_ptr<sai_qos_map_api_t>(sai_qos_map_api, [](sai_qos_map_api_t* p) {
            delete p;
            sai_qos_map_api = nullptr;
        });

        // FIXME: add new function to setup spy function
        sai_qos_map_api->create_qos_map = create_qos_map;
        that = this;

        auto ret = std::make_shared<SetQosResult>();

        create_qos_map_fn =
            [&](sai_object_id_t* sai_object_id, sai_object_id_t switch_id,
                uint32_t attr_count,
                const sai_attribute_t* attr_list) -> sai_status_t {
            for (auto i = 0; i < attr_count; ++i) {
                ret->attr_list.emplace_back(attr_list[i]);
            }
            return SAI_STATUS_SUCCESS;
        };

        vector<sai_attribute_t> attrs;
        dscpToTc.convertFieldValuesToAttributes(tuple, attrs);
        ret->ret_val = dscpToTc.addQosItem(attrs);
        return ret;
    }

    std::shared_ptr<SetQosResult> setTc2Queue(TcToQueueMapHandler& tcToQueue, KeyOpFieldsValuesTuple& tuple)
    {
        assert(sai_qos_map_api == nullptr);

        sai_qos_map_api = new sai_qos_map_api_t();
        auto sai_qos = std::shared_ptr<sai_qos_map_api_t>(sai_qos_map_api, [](sai_qos_map_api_t* p) {
            delete p;
            sai_qos_map_api = nullptr;
        });

        // FIXME: add new function to setup spy function
        sai_qos_map_api->create_qos_map = create_qos_map;
        that = this;

        auto ret = std::make_shared<SetQosResult>();

        create_qos_map_fn =
            [&](sai_object_id_t* sai_object_id, sai_object_id_t switch_id,
                uint32_t attr_count,
                const sai_attribute_t* attr_list) -> sai_status_t {
            for (auto i = 0; i < attr_count; ++i) {
                ret->attr_list.emplace_back(attr_list[i]);
            }
            return SAI_STATUS_SUCCESS;
        };

        vector<sai_attribute_t> attrs;
        tcToQueue.convertFieldValuesToAttributes(tuple, attrs);
        ret->ret_val = tcToQueue.addQosItem(attrs);
        return ret;
    }

    std::shared_ptr<SetQosResult> setTc2Pg(TcToPgHandler& tcToPg, KeyOpFieldsValuesTuple& tuple)
    {
        assert(sai_qos_map_api == nullptr);

        sai_qos_map_api = new sai_qos_map_api_t();
        auto sai_qos = std::shared_ptr<sai_qos_map_api_t>(sai_qos_map_api, [](sai_qos_map_api_t* p) {
            delete p;
            sai_qos_map_api = nullptr;
        });

        // FIXME: add new function to setup spy function
        sai_qos_map_api->create_qos_map = create_qos_map;
        that = this;

        auto ret = std::make_shared<SetQosResult>();

        create_qos_map_fn =
            [&](sai_object_id_t* sai_object_id, sai_object_id_t switch_id,
                uint32_t attr_count,
                const sai_attribute_t* attr_list) -> sai_status_t {
            for (auto i = 0; i < attr_count; ++i) {
                ret->attr_list.emplace_back(attr_list[i]);
            }
            return SAI_STATUS_SUCCESS;
        };

        vector<sai_attribute_t> attrs;
        tcToPg.convertFieldValuesToAttributes(tuple, attrs);
        ret->ret_val = tcToPg.addQosItem(attrs);
        return ret;
    }

    std::shared_ptr<SetQosResult> setPfcPrio2Pg(PfcPrioToPgHandler& pfcPrioToPg, KeyOpFieldsValuesTuple& tuple)
    {
        assert(sai_qos_map_api == nullptr);

        sai_qos_map_api = new sai_qos_map_api_t();
        auto sai_qos = std::shared_ptr<sai_qos_map_api_t>(sai_qos_map_api, [](sai_qos_map_api_t* p) {
            delete p;
            sai_qos_map_api = nullptr;
        });

        // FIXME: add new function to setup spy function
        sai_qos_map_api->create_qos_map = create_qos_map;
        that = this;

        auto ret = std::make_shared<SetQosResult>();

        create_qos_map_fn =
            [&](sai_object_id_t* sai_object_id, sai_object_id_t switch_id,
                uint32_t attr_count,
                const sai_attribute_t* attr_list) -> sai_status_t {
            for (auto i = 0; i < attr_count; ++i) {
                ret->attr_list.emplace_back(attr_list[i]);
            }
            return SAI_STATUS_SUCCESS;
        };

        vector<sai_attribute_t> attrs;
        pfcPrioToPg.convertFieldValuesToAttributes(tuple, attrs);
        ret->ret_val = pfcPrioToPg.addQosItem(attrs);
        return ret;
    }

    std::shared_ptr<SetQosResult> setPfc2Queue(PfcToQueueHandler& pfcToQueue, KeyOpFieldsValuesTuple& tuple)
    {
        assert(sai_qos_map_api == nullptr);

        sai_qos_map_api = new sai_qos_map_api_t();
        auto sai_qos = std::shared_ptr<sai_qos_map_api_t>(sai_qos_map_api, [](sai_qos_map_api_t* p) {
            delete p;
            sai_qos_map_api = nullptr;
        });

        // FIXME: add new function to setup spy function
        sai_qos_map_api->create_qos_map = create_qos_map;
        that = this;

        auto ret = std::make_shared<SetQosResult>();

        create_qos_map_fn =
            [&](sai_object_id_t* sai_object_id, sai_object_id_t switch_id,
                uint32_t attr_count,
                const sai_attribute_t* attr_list) -> sai_status_t {
            for (auto i = 0; i < attr_count; ++i) {
                ret->attr_list.emplace_back(attr_list[i]);
            }
            return SAI_STATUS_SUCCESS;
        };

        vector<sai_attribute_t> attrs;
        pfcToQueue.convertFieldValuesToAttributes(tuple, attrs);
        ret->ret_val = pfcToQueue.addQosItem(attrs);
        return ret;
    }

    std::shared_ptr<SetQosResult> addWredProfile(WredMapHandler& wredMap, KeyOpFieldsValuesTuple& tuple)
    {
        assert(sai_wred_api == nullptr);

        sai_wred_api = new sai_wred_api_t();
        auto sai_qos = std::shared_ptr<sai_wred_api_t>(sai_wred_api, [](sai_wred_api_t* p) {
            delete p;
            sai_wred_api = nullptr;
        });

        // FIXME: add new function to setup spy function
        sai_wred_api->create_wred = create_wred;
        that = this;

        auto ret = std::make_shared<SetQosResult>();

        create_wred_fn =
            [&](sai_object_id_t* sai_object_id, sai_object_id_t switch_id,
                uint32_t attr_count,
                const sai_attribute_t* attr_list) -> sai_status_t {
            for (auto i = 0; i < attr_count; ++i) {
                ret->attr_list.emplace_back(attr_list[i]);
            }
            return SAI_STATUS_SUCCESS;
        };

        vector<sai_attribute_t> attrs;
        wredMap.convertFieldValuesToAttributes(tuple, attrs);
        ret->ret_val = wredMap.addQosItem(attrs);
        return ret;
    }

    std::shared_ptr<SetQosResult> deleteWredProfile(WredMapHandler& wredMap, KeyOpFieldsValuesTuple& tuple)
    {
        assert(sai_wred_api == nullptr);

        sai_wred_api = new sai_wred_api_t();
        auto sai_qos = std::shared_ptr<sai_wred_api_t>(sai_wred_api, [](sai_wred_api_t* p) {
            delete p;
            sai_wred_api = nullptr;
        });

        // FIXME: add new function to setup spy function
        sai_wred_api->create_wred = create_wred;
        sai_wred_api->remove_wred = remove_wred;
        that = this;

        auto ret = std::make_shared<SetQosResult>();

        create_wred_fn =
            [&](sai_object_id_t* sai_object_id, sai_object_id_t switch_id,
                uint32_t attr_count,
                const sai_attribute_t* attr_list) -> sai_status_t {
            for (auto i = 0; i < attr_count; ++i) {
                ret->sai_object_id = *sai_object_id;
                ret->attr_list.emplace_back(attr_list[i]);
            }
            return SAI_STATUS_SUCCESS;
        };

        remove_wred_fn =
            [&](sai_object_id_t sai_object_id) -> sai_status_t {
            if (ret->sai_object_id == sai_object_id) {
                return SAI_STATUS_SUCCESS;
            } else {
                return SAI_STATUS_FAILURE;
            }
        };

        vector<sai_attribute_t> attrs;
        wredMap.convertFieldValuesToAttributes(tuple, attrs);
        wredMap.addQosItem(attrs);

        ret->ret_val = wredMap.removeQosItem(ret->sai_object_id);
        return ret;
    }

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

            int act_len;
            int exp_len;

            if (meta->attrvaluetype != _sai_attr_value_type_t::SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST) {
                act_len = sai_serialize_attribute_value(act_buf, meta, &act_attr_list[i].value);
                exp_len = sai_serialize_attribute_value(exp_buf, meta, &exp_attr_list.get_attr_list()[i].value);
            } else {
                act_len = sai_serialize_qos_map_list(act_buf, &act_attr_list[i].value.qosmap);
                exp_len = sai_serialize_qos_map_list(exp_buf, &exp_attr_list.get_attr_list()[i].value.qosmap);
            }

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

struct QosMapHandlerTest : public TestBase {
    void SetUp() override
    {
        // ...
        ASSERT_TRUE(0 == setenv("platform", "x86_64-barefoot_p4-r0", 1));
    }
};

struct MockQosOrch {
    QosOrch* m_qosOrch;
    swss::DBConnector* config_db;

    MockQosOrch(QosOrch* qosOrch, swss::DBConnector* config_db)
        : m_qosOrch(qosOrch)
        , config_db(config_db)
    {
    }

    size_t consumerAddToSync(Consumer* consumer, const std::deque<KeyOpFieldsValuesTuple>& entries)
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

    type_map& getTypeMap()
    {
        //SWSS_LOG_ENTER();
        return m_qosOrch->m_qos_maps;
    }

    void doQosMapTask(const std::deque<KeyOpFieldsValuesTuple>& entries, std::string tableName)
    {
        auto consumer = std::unique_ptr<Consumer>(new Consumer(
            new swss::ConsumerStateTable(config_db, tableName, 1, 1), m_qosOrch, tableName));

        consumerAddToSync(consumer.get(), entries);

        static_cast<Orch*>(m_qosOrch)->doTask(*consumer);
    }
};

struct QosOrchTest : public TestBase {

    std::shared_ptr<swss::DBConnector> m_app_db;
    std::shared_ptr<swss::DBConnector> m_config_db;
    std::vector<sai_qos_map_t*> m_qos_map_list_pool;
    QosOrch* qos_orch;

    vector<string> qos_tables = {
        CFG_TC_TO_QUEUE_MAP_TABLE_NAME,
        CFG_SCHEDULER_TABLE_NAME,
        CFG_DSCP_TO_TC_MAP_TABLE_NAME,
        CFG_QUEUE_TABLE_NAME,
        CFG_PORT_QOS_MAP_TABLE_NAME,
        CFG_WRED_PROFILE_TABLE_NAME,
        CFG_TC_TO_PRIORITY_GROUP_MAP_TABLE_NAME,
        CFG_PFC_PRIORITY_TO_PRIORITY_GROUP_MAP_TABLE_NAME,
        CFG_PFC_PRIORITY_TO_QUEUE_MAP_TABLE_NAME
    };

    QosOrchTest()
    {
        // FIXME: move out from constructor
        m_app_db = std::make_shared<swss::DBConnector>(APPL_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_config_db = std::make_shared<swss::DBConnector>(CONFIG_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    }

    virtual ~QosOrchTest()
    {
        for (auto p : m_qos_map_list_pool) {
            free(p);
        }
    }

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
    }

    static int profile_get_next_value(
        sai_switch_profile_id_t profile_id,
        const char** variable,
        const char** value)
    {
        if (value == NULL) {
            return 0;
        }

        if (variable == NULL) {
            return -1;
        }

        return -1;
    }

    void SetUp() override
    {
        qos_orch = new QosOrch(m_config_db.get(), qos_tables);
        sai_service_method_table_t test_services = {
            QosOrchTest::profile_get_value,
            QosOrchTest::profile_get_next_value
        };

        auto status = sai_api_initialize(0, (sai_service_method_table_t*)&test_services);
        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);

#if WITH_SAI == LIBVS
        sai_switch_api = const_cast<sai_switch_api_t*>(&vs_switch_api);
        sai_qos_map_api = const_cast<sai_qos_map_api_t*>(&vs_qos_map_api);
#endif

        sai_attribute_t swattr;

        swattr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        swattr.value.booldata = true;

        status = sai_switch_api->create_switch(&gSwitchId, 1, &swattr);
        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);

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
        delete qos_orch;
        qos_orch = nullptr;
        auto status = sai_switch_api->remove_switch(gSwitchId);
        ASSERT_TRUE(status == SAI_STATUS_SUCCESS);
        gSwitchId = 0;

        sai_api_uninitialize();

        sai_switch_api = nullptr;
        sai_qos_map_api = nullptr;
    }

    std::shared_ptr<MockQosOrch> createQosOrch()
    {
        return std::make_shared<MockQosOrch>(qos_orch, m_config_db.get());
    }

    std::shared_ptr<SaiAttributeList> getQosMapAttributeList(sai_object_type_t objecttype, std::string qos_table_name, const vector<FieldValueTuple>& values)
    {
        std::vector<swss::FieldValueTuple> fields;

        if (qos_table_name == CFG_DSCP_TO_TC_MAP_TABLE_NAME) {
            fields.push_back({ "SAI_QOS_MAP_ATTR_TYPE", "SAI_QOS_MAP_TYPE_DSCP_TO_TC" });
            fields.push_back({ "SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST", convertValuesToQosMapListStr(qos_table_name, values) });
        } else if (qos_table_name == CFG_TC_TO_QUEUE_MAP_TABLE_NAME) {
            fields.push_back({ "SAI_QOS_MAP_ATTR_TYPE", "SAI_QOS_MAP_TYPE_TC_TO_QUEUE" });
            fields.push_back({ "SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST", convertValuesToQosMapListStr(qos_table_name, values) });
        }

        return std::shared_ptr<SaiAttributeList>(new SaiAttributeList(objecttype, fields, false));
    }

    std::string convertValuesToQosMapListStr(std::string qos_table_name, const vector<FieldValueTuple> values)
    {
        const sai_attr_metadata_t* meta;
        sai_attribute_t attr;
        attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
        attr.value.qosmap.count = values.size();
        attr.value.qosmap.list = new sai_qos_map_t[attr.value.qosmap.count]();
        uint32_t ind = 0;

        if (qos_table_name == CFG_DSCP_TO_TC_MAP_TABLE_NAME) {
            for (auto i = values.begin(); i != values.end(); i++, ind++) {
                attr.value.qosmap.list[ind].key.dscp = (uint8_t)stoi(fvField(*i));
                attr.value.qosmap.list[ind].value.tc = (uint8_t)stoi(fvValue(*i));
            }
        } else if (qos_table_name == CFG_TC_TO_QUEUE_MAP_TABLE_NAME) {
            for (auto i = values.begin(); i != values.end(); i++, ind++) {
                attr.value.qosmap.list[ind].key.tc = (uint8_t)stoi(fvField(*i));
                attr.value.qosmap.list[ind].value.queue_index = (uint8_t)stoi(fvValue(*i));
            }
        }

        meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_QOS_MAP, attr.id);

        return sai_serialize_attr_value(*meta, attr);
    }

    bool Validate(MockQosOrch* orch, const std::string table_name, const vector<FieldValueTuple>& values)
    {
        const sai_object_type_t objecttype = SAI_OBJECT_TYPE_QOS_MAP;

        auto qos_maps = orch->getTypeMap();
        auto qos_map = qos_maps.find(table_name);
        if (qos_map == qos_maps.end()) {
            return false;
        }

        auto obj_map = *(qos_map->second)->find(table_name);
        if (obj_map == *(qos_map->second)->end()) {
            return false;
        }

        auto exp_attr_list = getQosMapAttributeList(objecttype, table_name, values);
        if (!ValidateQosMap(objecttype, obj_map.second, *exp_attr_list.get())) {
            return false;
        }

        return true;
    }

    bool ValidateQosMap(sai_object_type_t objecttype, sai_object_id_t object_id, SaiAttributeList& exp_attrlist)
    {
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
            case SAI_ATTR_VALUE_TYPE_INT32:
                new_attr.value.u32 = attr.value.u32;
                break;
            case SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
                new_attr.value.qosmap.count = attr.value.qosmap.count;
                new_attr.value.qosmap.list = (sai_qos_map_t*)malloc(sizeof(sai_qos_map_t) * attr.value.qosmap.count);
                m_qos_map_list_pool.emplace_back(new_attr.value.qosmap.list);
                break;
            default:
                std::cout << "";
            }

            act_attr.emplace_back(new_attr);
        }

        auto status = sai_qos_map_api->get_qos_map_attribute(object_id, act_attr.size(), act_attr.data());
        if (status != SAI_STATUS_SUCCESS) {
            return false;
        }

        auto b_attr_eq = AttrListEq(objecttype, act_attr, exp_attrlist);
        if (!b_attr_eq) {
            return false;
        }

        return true;
    }
};

TEST_F(QosMapHandlerTest, DscpToTcMap)
{
    DscpToTcMapHandler dscpToTcMapHandler;
    KeyOpFieldsValuesTuple dscp_to_tc_tuple(CFG_DSCP_TO_TC_MAP_TABLE_NAME, SET_COMMAND,
        { { "1", "0" }, { "2", "0" }, { "3", "3" } });

    auto res = setDscp2Tc(dscpToTcMapHandler, dscp_to_tc_tuple);

    ASSERT_TRUE(res->ret_val == true);

    auto v = std::vector<swss::FieldValueTuple>({ { "SAI_QOS_MAP_ATTR_TYPE", "SAI_QOS_MAP_TYPE_DSCP_TO_TC" },
        { "SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST", "{\"count\":3,\"list\":[{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":1,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0}},{\
         \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":2,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":3,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":3}}]}" } });
    SaiAttributeList attr_list(SAI_OBJECT_TYPE_QOS_MAP, v, false);

    ASSERT_TRUE(Check::AttrListEq_Miss_objecttype_Dont_Use(res->attr_list, attr_list));
}

TEST_F(QosOrchTest, DscpToTcMapViaVS)
{
    auto orch = createQosOrch();

    vector<FieldValueTuple> dscp_to_tc_values = { { "1", "0" }, { "2", "0" }, { "3", "3" } };
    KeyOpFieldsValuesTuple dscp_to_tc_tuple(CFG_DSCP_TO_TC_MAP_TABLE_NAME, SET_COMMAND, dscp_to_tc_values);
    std::deque<KeyOpFieldsValuesTuple> setData = { dscp_to_tc_tuple };

    orch->doQosMapTask(setData, CFG_DSCP_TO_TC_MAP_TABLE_NAME);
    ASSERT_TRUE(Validate(orch.get(), CFG_DSCP_TO_TC_MAP_TABLE_NAME, dscp_to_tc_values));
}

TEST_F(QosMapHandlerTest, TcToQueueMap)
{
    TcToQueueMapHandler tcToQueueMapHandler;
    KeyOpFieldsValuesTuple tc_to_queue_tuple(CFG_TC_TO_QUEUE_MAP_TABLE_NAME, SET_COMMAND,
        { { "0", "0" }, { "1", "1" }, { "3", "3" } });

    auto res = setTc2Queue(tcToQueueMapHandler, tc_to_queue_tuple);

    ASSERT_TRUE(res->ret_val == true);

    auto v = std::vector<swss::FieldValueTuple>({ { "SAI_QOS_MAP_ATTR_TYPE", "SAI_QOS_MAP_TYPE_TC_TO_QUEUE" },
        { "SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST", "{\"count\":3,\"list\":[{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":1},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":1,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":3},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":3,\"tc\":0}}]}" } });
    SaiAttributeList attr_list(SAI_OBJECT_TYPE_QOS_MAP, v, false);

    ASSERT_TRUE(Check::AttrListEq_Miss_objecttype_Dont_Use(res->attr_list, attr_list));
}

TEST_F(QosMapHandlerTest, TcToPgMap)
{
    TcToPgHandler tcToPgHandler;
    KeyOpFieldsValuesTuple tc_to_pg_tuple(CFG_TC_TO_PRIORITY_GROUP_MAP_TABLE_NAME, SET_COMMAND,
        { { "0", "0" }, { "1", "1" }, { "3", "3" } });

    auto res = setTc2Pg(tcToPgHandler, tc_to_pg_tuple);

    ASSERT_TRUE(res->ret_val == true);

    auto v = std::vector<swss::FieldValueTuple>({ { "SAI_QOS_MAP_ATTR_TYPE", "SAI_QOS_MAP_TYPE_TC_TO_PRIORITY_GROUP" },
        { "SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST", "{\"count\":3,\"list\":[{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":1},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":1,\"prio\":0,\"qidx\":0,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":3},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":3,\"prio\":0,\"qidx\":3,\"tc\":0}}]}" } });
    SaiAttributeList attr_list(SAI_OBJECT_TYPE_QOS_MAP, v, false);

    ASSERT_TRUE(Check::AttrListEq_Miss_objecttype_Dont_Use(res->attr_list, attr_list));
}

TEST_F(QosMapHandlerTest, PfcPrioToPgMap)
{
    PfcPrioToPgHandler pfcPrioToPgHandler;
    KeyOpFieldsValuesTuple pfc_prio_to_pg_tuple(CFG_PFC_PRIORITY_TO_PRIORITY_GROUP_MAP_TABLE_NAME, SET_COMMAND,
        { { "0", "0" }, { "1", "1" }, { "3", "3" } });

    auto res = setPfcPrio2Pg(pfcPrioToPgHandler, pfc_prio_to_pg_tuple);

    ASSERT_TRUE(res->ret_val == true);

    auto v = std::vector<swss::FieldValueTuple>({ { "SAI_QOS_MAP_ATTR_TYPE", "SAI_QOS_MAP_TYPE_PFC_PRIORITY_TO_PRIORITY_GROUP" },
        { "SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST", "{\"count\":3,\"list\":[{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":1,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":1,\"prio\":0,\"qidx\":0,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":3,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":3,\"prio\":0,\"qidx\":0,\"tc\":0}}]}" } });
    SaiAttributeList attr_list(SAI_OBJECT_TYPE_QOS_MAP, v, false);

    ASSERT_TRUE(Check::AttrListEq_Miss_objecttype_Dont_Use(res->attr_list, attr_list));
}

TEST_F(QosMapHandlerTest, PfcToQueueMap)
{
    PfcToQueueHandler pfcToQueueHandler;
    KeyOpFieldsValuesTuple pfc_to_queue_tuple(CFG_PFC_PRIORITY_TO_QUEUE_MAP_TABLE_NAME, SET_COMMAND,
        { { "0", "0" }, { "1", "1" }, { "3", "3" } });

    auto res = setPfc2Queue(pfcToQueueHandler, pfc_to_queue_tuple);

    ASSERT_TRUE(res->ret_val == true);

    auto v = std::vector<swss::FieldValueTuple>({ { "SAI_QOS_MAP_ATTR_TYPE", "SAI_QOS_MAP_TYPE_PFC_PRIORITY_TO_QUEUE" },
        { "SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST", "{\"count\":3,\"list\":[{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":0,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":1,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":1,\"tc\":0}},{\
        \"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":3,\"qidx\":0,\"tc\":0},\
        \"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":0,\"dscp\":0,\"pg\":0,\"prio\":0,\"qidx\":3,\"tc\":0}}]}" } });
    SaiAttributeList attr_list(SAI_OBJECT_TYPE_QOS_MAP, v, false);

    ASSERT_TRUE(Check::AttrListEq_Miss_objecttype_Dont_Use(res->attr_list, attr_list));
}

TEST_F(QosMapHandlerTest, AddWredProfile)
{
    WredMapHandler wredMapHandler;
    KeyOpFieldsValuesTuple wred_profile_tuple(CFG_WRED_PROFILE_TABLE_NAME, SET_COMMAND,
        { { "wred_yellow_enable", "true" },
            { "wred_red_enable", "true" },
            { "ecn", "ecn_all" },
            { "red_max_threshold", "312000" },
            { "red_min_threshold", "104000" },
            { "yellow_max_threshold", "312000" },
            { "yellow_min_threshold", "104000" },
            { "green_max_threshold", "312000" },
            { "green_min_threshold", "104000" } });

    auto res = addWredProfile(wredMapHandler, wred_profile_tuple);

    ASSERT_TRUE(res->ret_val == true);

    auto v = std::vector<swss::FieldValueTuple>({ { "SAI_WRED_ATTR_WEIGHT", "0" },
        { "SAI_WRED_ATTR_YELLOW_ENABLE", "true" },
        { "SAI_WRED_ATTR_RED_ENABLE", "true" },
        { "SAI_WRED_ATTR_ECN_MARK_MODE", "SAI_ECN_MARK_MODE_ALL" },
        { "SAI_WRED_ATTR_RED_MAX_THRESHOLD", "312000" },
        { "SAI_WRED_ATTR_RED_MIN_THRESHOLD", "104000" },
        { "SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD", "312000" },
        { "SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD", "104000" },
        { "SAI_WRED_ATTR_GREEN_MAX_THRESHOLD", "312000" },
        { "SAI_WRED_ATTR_GREEN_MIN_THRESHOLD", "104000" },
        { "SAI_WRED_ATTR_GREEN_DROP_PROBABILITY", "100" },
        { "SAI_WRED_ATTR_YELLOW_DROP_PROBABILITY", "100" },
        { "SAI_WRED_ATTR_RED_DROP_PROBABILITY", "100" } });
    SaiAttributeList attr_list(SAI_OBJECT_TYPE_WRED, v, false);

    ASSERT_TRUE(Check::AttrListEq_Miss_objecttype_Dont_Use(res->attr_list, attr_list));
}

// TEST_F(QosMapHandlerTest, DeleteWredProfile)
// {
//     WredMapHandler wredMapHandler;
//     KeyOpFieldsValuesTuple wred_profile_tuple(CFG_WRED_PROFILE_TABLE_NAME, SET_COMMAND,
//         { { "wred_yellow_enable", "true" },
//             { "wred_red_enable", "true" },
//             { "ecn", "ecn_all" },
//             { "red_max_threshold", "312000" },
//             { "red_min_threshold", "104000" },
//             { "yellow_max_threshold", "312000" },
//             { "yellow_min_threshold", "104000" },
//             { "green_max_threshold", "312000" },
//             { "green_min_threshold", "104000" } });

//     auto res = deleteWredProfile(wredMapHandler, wred_profile_tuple);

//     ASSERT_TRUE(res->ret_val == true);
// }
}