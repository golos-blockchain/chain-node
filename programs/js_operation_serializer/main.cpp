#include <golos/protocol/protocol.hpp>
#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/plugins/follow/follow_operations.hpp>
#include <golos/chain/steem_objects.hpp>
#include <fc/smart_ref_impl.hpp>

using namespace golos::chain;
using namespace golos::protocol;
using namespace golos::plugins::private_message;
using namespace golos::plugins::follow;

using std::string;
using std::map;

namespace detail_ns {

    string remove_tail_if(const string &str, char c, const string &match) {
        auto last = str.find_last_of(c);
        if (last != std::string::npos) {
            if (str.substr(last + 1) == match) {
                return str.substr(0, last);
            }
        }
        return str;
    }

    string remove_namespace_if(const string &str, const string &match) {
        auto last = str.find(match);
        if (last != std::string::npos) {
            return str.substr(match.size() + 2);
        }
        return str;
    }


    string remove_namespace(string str) {
        str = remove_tail_if(str, '_', "operation");
        str = remove_tail_if(str, '_', "t");
        str = remove_tail_if(str, '_', "object");
        str = remove_tail_if(str, '_', "type");
        str = remove_namespace_if(str, "golos::chain");
        str = remove_namespace_if(str, "chainbase");
        str = remove_namespace_if(str, "std");
        str = remove_namespace_if(str, "fc");
        auto pos = str.find(":");
        if (pos != str.npos) {
            str.replace(pos, 2, "_");
        }
        return str;
    }


    template<typename T>
    void generate_serializer();

    template<typename T>
    void register_serializer();


    map<string, size_t> st;
    golos::vector<std::function<void()>> serializers;

    bool register_serializer(const string &name, std::function<void()> sr) {
        if (st.find(name) == st.end()) {
            serializers.push_back(sr);
            st[name] = serializers.size() - 1;
            return true;
        }
        return false;
    }

    template<typename T>
    struct js_name {
        static std::string name() {
            return remove_namespace(fc::get_typename<T>::name());
        };
    };

    template<typename T, size_t N>
    struct js_name<fc::array<T, N>> {
        static std::string name() {
            return "fixed_array " + fc::to_string(N) + ", " +
                   remove_namespace(fc::get_typename<T>::name());
        };
    };

    template<size_t N>
    struct js_name<fc::array<char, N>> {
        static std::string name() {
            return "bytes " + fc::to_string(N);
        };
    };

    template<>
    struct js_name<fc::fixed_string<> > {
        static std::string name() {
            return "string";
        }
    };

    template<size_t N>
    struct js_name<fc::array<uint8_t, N>> {
        static std::string name() {
            return "bytes " + fc::to_string(N);
        };
    };

    template<typename T>
    struct js_name<fc::optional<T>> {
        static std::string name() {
            return "optional " + js_name<T>::name();
        }
    };

    template<typename T>
    struct js_name<fc::smart_ref<T>> {
        static std::string name() {
            return js_name<T>::name();
        }
    };

    template<typename T>
    struct js_name<fc::flat_set<T>> {
        static std::string name() {
            return "set " + js_name<T>::name();
        }
    };

    template<typename T>
    struct js_name<std::vector<T>> {
        static std::string name() {
            return "array " + js_name<T>::name();
        }
    };

    template<typename T>
    struct js_name<fc::safe<T>> {
        static std::string name() {
            return js_name<T>::name();
        }
    };


    template<>
    struct js_name<std::vector<char>> {
        static std::string name() {
            return "bytes()";
        }
    };

    template<>
    struct js_name<fc::uint160> {
        static std::string name() {
            return "bytes 20";
        }
    };

    template<>
    struct js_name<fc::sha224> {
        static std::string name() {
            return "bytes 28";
        }
    };

    template<>
    struct js_name<fc::sha256> {
        static std::string name() {
            return "bytes 32";
        }
    };

    template<>
    struct js_name<fc::unsigned_int> {
        static std::string name() {
            return "varuint32";
        }
    };

    template<>
    struct js_name<fc::signed_int> {
        static std::string name() {
            return "varint32";
        }
    };

    template<>
    struct js_name<fc::time_point_sec> {
        static std::string name() {
            return "time_point_sec";
        }
    };

    template<>
    struct js_name<private_contact_type> {
        static std::string name() {
            return "private_contact_type";
        }
    };

    template<>
    struct js_name<private_group_privacy> {
        static std::string name() {
            return "private_group_privacy";
        }
    };

    template<>
    struct js_name<private_group_member_type> {
        static std::string name() {
            return "private_group_member_type";
        }
    };

    template<>
    struct js_name<delegator_payout_strategy> {
        static std::string name() {
            return "delegator_payout_strategy";
        }
    };

    template<>
    struct js_name<worker_request_state> {
        static std::string name() {
            return "worker_request_state";
        }
    };

    template<>
    struct js_name<sponsor_payment> {
        static std::string name() {
            return "sponsor_payment";
        }
    };

    template<>
    struct js_name<psro_inactive_reason> {
        static std::string name() {
            return "psro_inactive_reason";
        }
    };

    template<>
    struct js_name<curation_curve> {
        static std::string name() {
            return "curation_curve";
        }
    };

    template<typename O>
    struct js_name<chainbase::object_id<O>> {
        static std::string name() {
            return "protocol_id_type \"" +
                   remove_namespace(fc::get_typename<O>::name()) + "\"";
        };
    };


    template<typename T>
    struct js_name<std::set<T>> {
        static std::string name() {
            return "set " + js_name<T>::name();
        }
    };

    template<typename K, typename V>
    struct js_name<std::map<K, V>> {
        static std::string name() {
            return "map (" + js_name<K>::name() + "), (" + js_name<V>::name() +
                   ")";
        }
    };

    template<typename K, typename V>
    struct js_name<fc::flat_map<K, V>> {
        static std::string name() {
            return "map (" + js_name<K>::name() + "), (" + js_name<V>::name() +
                   ")";
        }
    };


    template<typename... T>
    struct js_sv_name;

    template<typename A>
    struct js_sv_name<A> {
        static std::string name() {
            return "\n    " + js_name<A>::name();
        }
    };

    template<typename A, typename... T>
    struct js_sv_name<A, T...> {
        static std::string name() {
            return "\n    " + js_name<A>::name() + "    " +
                   js_sv_name<T...>::name();
        }
    };

    template<typename... T>
    struct js_name<fc::static_variant<T...>> {
        static std::string name(std::string n = "") {
            static const std::string name = n;
            if (name == "") {
                return "static_variant [" + js_sv_name<T...>::name() + "\n]";
            } else {
                return name;
            }
        }
    };

    template<>
    struct js_name<fc::static_variant<>> {
        static std::string name(std::string n = "") {
            static const std::string name = n;
            if (name == "") {
                return "static_variant []";
            } else {
                return name;
            }
        }
    };


    template<typename T, bool reflected = fc::reflector<T>::is_defined::value>
    struct serializer;


    struct register_type_visitor {
        typedef void result_type;

        template<typename Type>
        result_type operator()(const Type &op) const {
            serializer<Type>::init();
        }
    };

    class register_member_visitor;

    struct serialize_type_visitor {
        typedef void result_type;

        int t = 0;

        serialize_type_visitor(int _t) : t(_t) {
        }

        template<typename Type>
        result_type operator()(const Type &op) const {
            std::cout << "    "
                      << remove_namespace(fc::get_typename<Type>::name())
                      << ": " << t << "\n";
        }
    };


    class serialize_member_visitor {
    public:
        template<typename Member, class Class, Member (Class::*member)>
        void operator()(const char *name) const {
            std::cout << "    " << name << ": " << js_name<Member>::name()
                      << "\n";
        }
    };

    template<typename T>
    struct serializer<T, false> {
        static_assert(fc::reflector<T>::is_defined::value ==
                      false, "invalid template arguments");

        static void init() {
        }

        static void generate() {
        }
    };

    template<typename T, size_t N>
    struct serializer<fc::array<T, N>, false> {
        static void init() {
            serializer<T>::init();
        }

        static void generate() {
        }
    };

    template<typename T>
    struct serializer<std::vector<T>, false> {
        static void init() {
            serializer<T>::init();
        }

        static void generate() {
        }
    };

    template<typename T>
    struct serializer<fc::smart_ref<T>, false> {
        static void init() {
            serializer<T>::init();
        }

        static void generate() {
        }
    };

    template<>
    struct serializer<std::vector<operation>, false> {
        static void init() {
        }

        static void generate() {
        }
    };

    template<>
    struct serializer<uint64_t, false> {
        static void init() {
        }

        static void generate() {
        }
    };

#ifdef __APPLE__

// on mac, size_t is a distinct type from uint64_t or uint32_t and needs a separate specialization
    template<>
    struct serializer<size_t, false> {
        static void init() {
        }

        static void generate() {
        }
    };

#endif

    template<>
    struct serializer<int64_t, false> {
        static void init() {
        }

        static void generate() {
        }
    };

    template<>
    struct serializer<int64_t, true> {
        static void init() {
        }

        static void generate() {
        }
    };

    template<>
    struct serializer<private_contact_type, true> {
        static void init() {
        }

        static void generate() {
        }
    };

    template<>
    struct serializer<private_group_privacy, true> {
        static void init() {
        }

        static void generate() {
        }
    };

    template<>
    struct serializer<private_group_member_type, true> {
        static void init() {
        }

        static void generate() {
        }
    };

    template<>
    struct serializer<delegator_payout_strategy, true> {
        static void init() {
            static bool init = false;
            if (!init) {
                init = true;
                register_serializer(js_name<delegator_payout_strategy>::name(), [=]() { generate(); });
            }
        }

        static void generate() {
            std::cout << "ChainTypes." << js_name<delegator_payout_strategy>::name() << " =\n";
            for (uint8_t i = uint8_t(delegator_payout_strategy::to_delegator); i < uint8_t(delegator_payout_strategy::_size); ++i) {
                std::cout << "    " << fc::json::to_string(delegator_payout_strategy(i)) << ": " << int(i) << "\n";
            }
            std::cout << "\n";
        }
    };

    template<>
    struct serializer<worker_request_state, true> {
        static void init() {
            static bool init = false;
            if (!init) {
                init = true;
                register_serializer(js_name<worker_request_state>::name(), [=]() { generate(); });
            }
        }

        static void generate() {
            std::cout << "ChainTypes." << js_name<worker_request_state>::name() << " =\n";
            for (uint8_t i = uint8_t(worker_request_state::created); i < uint8_t(worker_request_state::_size); ++i) {
                std::cout << "    " << fc::json::to_string(worker_request_state(i)) << ": " << int(i) << "\n";
            }
            std::cout << "\n";
        }
    };

    template<>
    struct serializer<sponsor_payment, true> {
        static void init() {
            static bool init = false;
            if (!init) {
                init = true;
                register_serializer(js_name<sponsor_payment>::name(), [=]() { generate(); });
            }
        }

        static void generate() {
            std::cout << "ChainTypes." << js_name<sponsor_payment>::name() << " =\n";
            for (uint8_t i = uint8_t(sponsor_payment::first); i < uint8_t(sponsor_payment::_size); ++i) {
                std::cout << "    " << fc::json::to_string(sponsor_payment(i)) << ": " << int(i) << "\n";
            }
            std::cout << "\n";
        }
    };

    template<>
    struct serializer<psro_inactive_reason, true> {
        static void init() {
            static bool init = false;
            if (!init) {
                init = true;
                register_serializer(js_name<psro_inactive_reason>::name(), [=]() { generate(); });
            }
        }

        static void generate() {
            std::cout << "ChainTypes." << js_name<psro_inactive_reason>::name() << " =\n";
            for (uint8_t i = uint8_t(psro_inactive_reason::none); i < uint8_t(psro_inactive_reason::_size); ++i) {
                std::cout << "    " << fc::json::to_string(psro_inactive_reason(i)) << ": " << int(i) << "\n";
            }
            std::cout << "\n";
        }
    };

    template<>
    struct serializer<curation_curve, true> {
        static void init() {
            static bool init = false;
            if (!init) {
                init = true;
                register_serializer(js_name<curation_curve>::name(), [=]() { generate(); });
            }
        }

        static void generate() {
            std::cout << "ChainTypes." << js_name<curation_curve>::name() << " =\n";
            for (uint8_t i = uint8_t(curation_curve::bounded); i < uint8_t(curation_curve::_size); ++i) {
                std::cout << "    " << fc::json::to_string(curation_curve(i)) << ": " << int(i) << "\n";
            }
            std::cout << "\n";
        }
    };

    template<typename T>
    struct serializer<fc::optional<T>, false> {
        static void init() {
            serializer<T>::init();
        }

        static void generate() {
        }
    };

    template<typename T>
    struct serializer<chainbase::object_id<T>, true> {
        static void init() {
        }

        static void generate() {
        }
    };

    template<typename... T>
    struct serializer<fc::static_variant<T...>, false> {
        static void init() {
            static bool init = false;
            if (!init) {
                init = true;
                fc::static_variant<T...> var;
                for (int i = 0; i < var.count(); ++i) {
                    var.set_which(i);
                    var.visit(register_type_visitor());
                }
                register_serializer(js_name<fc::static_variant<T...>>::name(), [=]() { generate(); });
            }
        }

        static void generate() {
            std::cout << js_name<fc::static_variant<T...>>::name()
                      << " = static_variant [" + js_sv_name<T...>::name() +
                         "\n]\n\n";
        }
    };

    template<>
    struct serializer<fc::static_variant<>, false> {
        static void init() {
            static bool init = false;
            if (!init) {
                init = true;
                fc::static_variant<> var;
                register_serializer(js_name<fc::static_variant<>>::name(), [=]() { generate(); });
            }
        }

        static void generate() {
            std::cout << js_name<fc::static_variant<>>::name()
                      << " = static_variant []\n\n";
        }
    };


    class register_member_visitor {
    public:
        template<typename Member, class Class, Member (Class::*member)>
        void operator()(const char *name) const {
            serializer<Member>::init();
        }
    };

    template<typename T, bool reflected>
    struct serializer {
        static_assert(fc::reflector<T>::is_defined::value ==
                      reflected, "invalid template arguments");

        static void init() {
            auto name = js_name<T>::name();
            if (st.find(name) == st.end()) {
                fc::reflector<T>::visit(register_member_visitor());
                register_serializer(name, [=]() { generate(); });
            }
        }

        static void generate() {
            auto name = remove_namespace(js_name<T>::name());
            if (name == "int64") {
                return;
            }
            std::cout << "" << name
                      << " = new Serializer( \n"
                      << "    \"" + name + "\"\n";

            fc::reflector<T>::visit(serialize_member_visitor());

            std::cout << ")\n\n";
        }
    };

} // namespace detail_ns

int main(int argc, char **argv) {
    try {
        operation op;

        std::cout << "ChainTypes.operations=\n";
        for (int i = 0; i < op.count(); ++i) {
            op.set_which(i);
            op.visit(detail_ns::serialize_type_visitor(i));
        }
        std::cout << "\n";

        versioned_chain_properties cp;
        std::cout << "ChainTypes.chain_properties=\n";
        for (int i = 0; i < cp.count(); ++i) {
            cp.set_which(i);
            cp.visit(detail_ns::serialize_type_visitor(i));
        }
        std::cout << "\n";

//        follow_plugin_operation fp;
//        std::cout << "ChainTypes.follow_operations=\n";
//        for (int i = 0; i < fp.count(); ++i) {
//            fp.set_which(i);
//            fp.visit(detail_ns::serialize_type_visitor(i));
//        }
//        std::cout << "\n";

        std::cout << "ChainTypes.private_contact_types=\n";
        for (uint8_t i = unknown; i < private_contact_type_size; ++i) {
            std::cout << "    " << fc::json::to_string(static_cast<private_contact_type>(i)) << ": " << int(i) << "\n";
        }
        std::cout << "\n";

        std::cout << "ChainTypes.private_group_privacies=\n";
        for (uint8_t i = unknown; i < uint8_t(private_group_privacy::_size); ++i) {
            std::cout << "    " << fc::json::to_string(static_cast<private_group_privacy>(i)) << ": " << int(i) << "\n";
        }
        std::cout << "\n";

        std::cout << "ChainTypes.private_group_member_types=\n";
        for (uint8_t i = unknown; i < uint8_t(private_group_member_type::_size); ++i) {
            std::cout << "    " << fc::json::to_string(static_cast<private_group_member_type>(i)) << ": " << int(i) << "\n";
        }
        std::cout << "\n";

//        private_message_plugin_operation pmp;
//        std::cout << "ChainTypes.private_message_operations=\n";
//        for (int i = 0; i < pmp.count(); ++i) {
//            pmp.set_which(i);
//            pmp.visit(detail_ns::serialize_type_visitor(i));
//        }
//        std::cout << "\n";

        detail_ns::js_name<operation>::name("operation");
        detail_ns::js_name<future_extensions>::name("future_extensions");
        detail_ns::js_name<versioned_chain_properties>::name("chain_properties");
        detail_ns::js_name<follow_plugin_operation>::name("follow");
        detail_ns::js_name<private_message_plugin_operation>::name("private_message");
        detail_ns::serializer<signed_block>::init();
        detail_ns::serializer<block_header>::init();
        detail_ns::serializer<signed_block_header>::init();
        detail_ns::serializer<operation>::init();
        detail_ns::serializer<versioned_chain_properties>::init();
        detail_ns::serializer<follow_plugin_operation>::init();
        detail_ns::serializer<private_message_plugin_operation>::init();
        detail_ns::serializer<transaction>::init();
        detail_ns::serializer<signed_transaction>::init();

        for (const auto &gen : detail_ns::serializers) {
            gen();
        }

    } catch (const fc::exception &e) {
        edump((e.to_detail_string()));
    }
    return 0;
}
