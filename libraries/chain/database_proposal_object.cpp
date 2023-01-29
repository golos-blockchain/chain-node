#include <golos/chain/database.hpp>
#include <golos/chain/proposal_object.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace chain {

    const proposal_object& database::get_proposal(
        const account_name_type& author,
        const std::string& title
    ) const { try {
        return get<proposal_object, by_account>(std::make_tuple(author, title));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("proposal", fc::mutable_variant_object()("account",author)("proposal",title));
    } FC_CAPTURE_AND_RETHROW((author)(title)) }

    const proposal_object *database::find_proposal(
        const account_name_type& author,
        const std::string& title
    ) const {
        return find<proposal_object, by_account>(std::make_tuple(author, title));
    }

// Yet another temporary PoC
// TODO: This can be generalized to generate all 3: get_, find_ and throw_if_exists_
// methods with variable number of params (like DEFINE_/DECLARE_API + PLUGIN_API_VALIDATE_ARGS)
#define DB_DEFINE_THROW_IF_EXIST(O, T1, N1, T2, N2) \
    void database::throw_if_exists_##O(T1 N1, T2 N2) const { \
        if (nullptr != find_##O(N1, N2)) { \
            GOLOS_THROW_OBJECT_ALREADY_EXIST(#O, fc::mutable_variant_object()(#N1,N1)(#N2,N2)); \
        } \
    }

    DB_DEFINE_THROW_IF_EXIST(proposal, const account_name_type&, author, const std::string&, title);

    void database::push_proposal(const proposal_object& proposal) { try {
        auto ops = proposal.operations();
        auto session = start_undo_session();
        for (auto& op : ops) {
            apply_operation(op, true);
        }
        // the parent session has been created in _push_block()/_push_transaction()
        session.squash();
        remove(proposal);
    } FC_CAPTURE_AND_RETHROW((proposal.author)(proposal.title)) }

    void database::remove(const proposal_object& p) {
        flat_set<account_name_type> required_total;
        required_total.insert(p.required_active_approvals.begin(), p.required_active_approvals.end());
        required_total.insert(p.required_owner_approvals.begin(), p.required_owner_approvals.end());
        required_total.insert(p.required_posting_approvals.begin(), p.required_posting_approvals.end());

        auto& idx = get_index<required_approval_index>().indices().get<by_account>();
        for (const auto& account: required_total) {
            auto itr = idx.find(std::make_tuple(account, p.id));
            if (idx.end() != itr) {
                remove(*itr);
            }
        }

        chainbase::database::remove(p);
    }

    void database::clear_expired_proposals() { try {
        const auto& proposal_expiration_index = get_index<proposal_index>().indices().get<by_expiration>();
        const auto now = head_block_time();

        while (!proposal_expiration_index.empty() && proposal_expiration_index.begin()->expiration_time <= now) {
            const proposal_object& proposal = *proposal_expiration_index.begin();

            try {
                if (proposal.is_authorized_to_execute(*this)) {
                    push_proposal(proposal);
                    continue;
                }
            } catch (const fc::exception& e) {
                elog(
                    "Failed to apply proposed transaction on its expiration. "
                    "Deleting it.\n${author}::${title}\n${error}",
                    ("author", proposal.author)("title", proposal.title)("error", e.to_detail_string()));
            }
            remove(proposal);
        }
    } FC_CAPTURE_AND_RETHROW() }

} } // golos::chain
