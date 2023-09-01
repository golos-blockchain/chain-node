#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/authority.hpp>
#include <golos/protocol/base.hpp>

namespace golos { namespace protocol {

    void validate_nft_collection_name(const std::string& name);

    asset_symbol_type nft_name_from_string(const std::string& name);

    std::string nft_name_to_string(asset_symbol_type symbol);

    struct nft_collection_operation : public base_operation {
        account_name_type creator;
        std::string name;
        std::string json_metadata;
        uint32_t max_token_count = 4294967295; // UINT32_MAX, means infinity

        extensions_type extensions;

        void validate() const;

        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            a.insert(creator);
        }
    };

    struct nft_collection_delete_operation : public base_operation {
        account_name_type creator;
        std::string name;

        extensions_type extensions;

        void validate() const;

        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            a.insert(creator);
        }
    };

    struct nft_issue_operation : public base_operation {
        account_name_type creator;
        std::string name;
        account_name_type to;
        std::string json_metadata;

        extensions_type extensions;

        void validate() const;
 
        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            a.insert(creator);
        }
    };

    struct nft_transfer_operation : public base_operation {
        uint32_t token_id = 0;
        account_name_type from;
        account_name_type to;
        std::string memo;

        extensions_type extensions;

        void validate() const;

        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            a.insert(from);
        }
    };

    struct nft_sell_operation : public base_operation {
        account_name_type seller;
        uint32_t token_id = 0;
        account_name_type buyer = account_name_type();
        uint32_t order_id = 0;
        asset price;

        extensions_type extensions;

        void validate() const;

        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            a.insert(seller);
        }
    };

    struct nft_buy_operation : public base_operation {
        account_name_type buyer;
        std::string name;
        uint32_t token_id = 0;
        uint32_t order_id = 0;
        asset price;

        extensions_type extensions;

        void validate() const;

        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            a.insert(buyer);
        }
    };

    struct nft_cancel_order_operation : public base_operation {
        account_name_type owner;
        uint32_t order_id = 0;

        extensions_type extensions;

        void validate() const;

        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            a.insert(owner);
        }
    };

} } // golos::protocol

FC_REFLECT(
    (golos::protocol::nft_collection_operation),
    (creator)(name)(json_metadata)(max_token_count)(extensions)
)

FC_REFLECT(
    (golos::protocol::nft_collection_delete_operation),
    (creator)(name)(extensions)
)

FC_REFLECT(
    (golos::protocol::nft_issue_operation),
    (creator)(name)(to)(json_metadata)(extensions)
)

FC_REFLECT(
    (golos::protocol::nft_transfer_operation),
    (token_id)(from)(to)(memo)(extensions)
)

FC_REFLECT(
    (golos::protocol::nft_sell_operation),
    (seller)(token_id)(buyer)(order_id)(price)(extensions)
)

FC_REFLECT(
    (golos::protocol::nft_buy_operation),
    (buyer)(name)(token_id)(order_id)(price)(extensions)
)

FC_REFLECT(
    (golos::protocol::nft_cancel_order_operation),
    (owner)(order_id)(extensions)
)
