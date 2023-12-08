#include <golos/protocol/nft_operations.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>
#include <fc/io/json.hpp>

namespace golos { namespace protocol {

    void validate_nft_collection_name(const std::string& name) {
        GOLOS_CHECK_VALUE(name.size(), "NFT collection name should not be empty");
        for (const auto& c : name) {
            if ((c > 'Z' || c < 'A') && c != '.') {
                GOLOS_CHECK_VALUE(false, "NFT collection name can contain only A-Z symbols and dots.");
                break;
            }
        }
        asset::from_string(std::string("0. ") + name);
    }

    asset_symbol_type nft_name_from_string(const std::string& name) {
        return asset::from_string(std::string("0. ") + name).symbol;
    }

    std::string nft_name_to_string(asset_symbol_type symbol) {
        return asset(0, symbol).symbol_name();
    }

    void nft_collection_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(creator);
        GOLOS_CHECK_PARAM(name, {
            validate_nft_collection_name(name);
        });
        if (json_metadata.size() > 0) {
            GOLOS_CHECK_PARAM(json_metadata, {
                GOLOS_CHECK_VALUE_MAX_SIZE(json_metadata, 512);
                GOLOS_CHECK_VALUE_UTF8(json_metadata);
                GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
            });
        }
        GOLOS_CHECK_PARAM(max_token_count, {
            GOLOS_CHECK_VALUE_GT(max_token_count, 0);
        });
    }

    void nft_collection_delete_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(creator);
        GOLOS_CHECK_PARAM(name, {
            validate_nft_collection_name(name);
        });
    }

    void nft_issue_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(creator);
        GOLOS_CHECK_PARAM(name, {
            validate_nft_collection_name(name);
        });
        GOLOS_CHECK_PARAM_ACCOUNT(to);
        GOLOS_CHECK_PARAM(to, {
            GOLOS_CHECK_VALUE(to != STEEMIT_NULL_ACCOUNT, "Cannot issue NFT to null account");
        });
        if (json_metadata.size() > 0) {
            GOLOS_CHECK_PARAM(json_metadata, {
                GOLOS_CHECK_VALUE_UTF8(json_metadata);
                GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
            });
        }
    }

    void nft_transfer_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(from);
        GOLOS_CHECK_PARAM_ACCOUNT(to);
        GOLOS_CHECK_PARAM(memo, {
            GOLOS_CHECK_VALUE(memo.size() < STEEMIT_MAX_MEMO_SIZE, "Memo is too large");
            GOLOS_CHECK_VALUE(fc::is_utf8(memo), "Memo is not UTF8");
        });
    }

    void nft_sell_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(seller);
        if (buyer != account_name_type()) {
            GOLOS_CHECK_PARAM_ACCOUNT(buyer);
        } else {
            GOLOS_CHECK_PARAM(price, {
                GOLOS_CHECK_VALUE(price.amount > 0, "If sell is first, sell price should be > 0");
            });
        }
        if (price.amount > 0) {
            GOLOS_CHECK_PARAM(price, {
                GOLOS_CHECK_VALUE(price.symbol != VESTS_SYMBOL, "Price cannot be GESTS");
            });
        }
    }

    void nft_buy_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(buyer);
        if (!token_id) {
            GOLOS_CHECK_PARAM(name, {
                GOLOS_CHECK_VALUE(name.size(), "If buy is first, you should set NFT collection name");
            });
        }
        if (name.size()) {
            GOLOS_CHECK_PARAM(name, {
                validate_nft_collection_name(name);
            });
            GOLOS_CHECK_PARAM(price, {
                GOLOS_CHECK_VALUE(price.amount > 0, "If buy is first, buy price should be > 0");
            });
        }
        if (price.amount > 0) {
            GOLOS_CHECK_PARAM(price, {
                GOLOS_CHECK_VALUE(price.symbol != VESTS_SYMBOL, "Price cannot be GESTS");
            });
        }
    }

    void nft_cancel_order_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(owner);
    }
}}
