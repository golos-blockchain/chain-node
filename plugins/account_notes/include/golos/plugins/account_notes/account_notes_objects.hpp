#pragma once

#include <golos/chain/database.hpp>

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//

#ifndef ACCOUNT_NOTES_SPACE_ID
#define ACCOUNT_NOTES_SPACE_ID 13
#endif


namespace golos { namespace plugins { namespace account_notes {

using chainbase::allocator;
using chainbase::object;
using chainbase::object_id;
using golos::chain::by_id;
using golos::chain::shared_string;
using namespace boost::multi_index;

enum account_object_types {
    account_note_object_type = (ACCOUNT_NOTES_SPACE_ID << 8),
    account_note_stats_object_type = (ACCOUNT_NOTES_SPACE_ID << 8) + 1
};

class account_note_object final : public object<account_note_object_type, account_note_object> {
public:
    template<typename Constructor, typename Allocator>
    account_note_object(Constructor&& c, allocator<Allocator> a)
            :  key(a), value(a) {
        c(*this);
    }

    id_type id;

    account_name_type account;
    shared_string key;
    shared_string value;
};

using account_note_id_type = object_id<account_note_object>;

struct by_account_key;
struct by_global_key;

using account_note_index = multi_index_container<
    account_note_object,
    indexed_by<
        ordered_unique<tag<by_id>,
            member<account_note_object, account_note_id_type, &account_note_object::id>
        >,
        ordered_unique<tag<by_account_key>, composite_key<account_note_object,
            member<account_note_object, account_name_type, &account_note_object::account>,
            member<account_note_object, shared_string, &account_note_object::key>
            >, composite_key_compare<std::less<account_name_type>, chainbase::strcmp_less>
        >,
        ordered_non_unique<tag<by_global_key>, 
            member<account_note_object, shared_string, &account_note_object::key>,
            chainbase::strcmp_less
        >
    >, allocator<account_note_object>>;

class account_note_stats_object final : public object<account_note_stats_object_type, account_note_stats_object> {
public:
    template<typename Constructor, typename Allocator>
    account_note_stats_object(Constructor&& c, allocator<Allocator> a) {
        c(*this);
    }

    id_type id;

    account_name_type account;
    uint16_t note_count = 0;
};

using account_note_stats_id_type = object_id<account_note_stats_object>;

struct by_account;

using account_note_stats_index = multi_index_container<
    account_note_stats_object,
    indexed_by<
        ordered_unique<
            tag<by_id>,
            member<account_note_stats_object, account_note_stats_id_type, &account_note_stats_object::id>>,
        ordered_unique<
            tag<by_account>,
            member<account_note_stats_object, account_name_type, &account_note_stats_object::account>>>,
    allocator<account_note_stats_object>>;

} } } // golos::plugins::account_notes

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::account_notes::account_note_object,
    golos::plugins::account_notes::account_note_index)

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::account_notes::account_note_stats_object,
    golos::plugins::account_notes::account_note_stats_index)
