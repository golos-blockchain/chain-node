#ifndef BUILDER_HPP
#define BUILDER_HPP

#include <appbase/application.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/shared_authority.hpp>
#include <golos/protocol/exceptions.hpp>

#include <graphene/utilities/key_conversion.hpp>
#include <fc/io/json.hpp>

namespace golos { namespace chain {

using obj = fc::mutable_variant_object;

struct database_fixture;

template<typename T>
variant _var(const T& obj) {
    variant v;
    to_variant(obj, v);
    return v;
}

struct builder_op {
    database_fixture* _opt_fixture = nullptr;
    fc::variant op;

    builder_op(database_fixture* df, const std::string& n) : _opt_fixture(df) {
        op = fc::variants{n, fc::variant_object()};

        operation opp = to_operation();
        try {
            fc::variant opv;
            to_variant(opp, opv);
            op = opv;
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    builder_op operator() (std::vector<fc::variant> vec) {
        auto opd_v = op.get_array()[1];
        auto& opd_c = opd_v.get_object();
        auto opd = fc::mutable_variant_object(opd_c);
        auto itr = opd.begin();
        for (size_t i = 0; i < vec.size(); ++i) {
            const auto& v = vec[i];
            if (itr == opd.end()) {
                elog("builder_op: Argument " + std::to_string(i) + " is excessive");
                break;
            }
            auto key = itr->key();
            opd[key] = v;
            ++itr;
        }
        op.get_array()[1] = opd;
        return *this;
    }

    builder_op fields(const obj& o) {
        op.get_array()[1] = o;
        return *this;
    }

    operation to_operation() {
        operation oop;
        try {
            from_variant(op, oop);
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
        return oop;
    }

    void push(const private_key_type& private_key);

    void dump() const {
        elog(fc::json::to_string(op));
    }

    database_fixture* fixture() {
        FC_ASSERT(_opt_fixture, "operation_builder: No database_fixture");
        return _opt_fixture;
    }
};

struct operation_builder {
    database_fixture* fixture = nullptr;

    operation_builder() {}

    operation_builder(database_fixture* df) : fixture(df) {
    }

    builder_op operator [](const std::string& name) {
        return builder_op(fixture, name);
    }
};

} } // golos:chain

#endif
