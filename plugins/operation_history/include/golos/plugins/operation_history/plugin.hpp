/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <appbase/application.hpp>
#include <golos/plugins/chain/plugin.hpp>

#include <golos/api/block_objects.hpp>
#include <golos/chain/database.hpp>
#include <boost/program_options.hpp>

#include <golos/plugins/json_rpc/utility.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/plugins/operation_history/applied_operation.hpp>
#include <golos/plugins/operation_history/history_object.hpp>


namespace golos { namespace plugins { namespace operation_history {
    using namespace chain;

    using golos::api::annotated_signed_block;
    using golos::api::block_operation;
    using golos::api::block_operations;

    using plugins::json_rpc::void_type;
    using plugins::json_rpc::msg_pack;
    using plugins::json_rpc::msg_pack_transfer;

    DEFINE_API_ARGS(get_block_with_virtual_ops, msg_pack, annotated_signed_block)
    DEFINE_API_ARGS(get_ops_in_block, msg_pack, std::vector<applied_operation>)
    DEFINE_API_ARGS(get_nft_token_ops, msg_pack, fc::mutable_variant_object)
    DEFINE_API_ARGS(get_transaction,  msg_pack, annotated_signed_transaction)

    /**
     *  This plugin is designed to track operations so that one node
     *  doesn't need to hold the full operation history in memory.
     */
    class plugin final : public appbase::plugin<plugin> {
    public:
        APPBASE_PLUGIN_REQUIRES(
            (json_rpc::plugin)
            (chain::plugin)
        )

        static const std::string& name();

        plugin( );
        ~plugin( );

        void set_program_options(
            boost::program_options::options_description &cli,
            boost::program_options::options_description &cfg) override;

        void plugin_initialize(const boost::program_options::variables_map &options) override;
        void plugin_startup() override;
        void plugin_shutdown() override;

        DECLARE_API(
            (get_block_with_virtual_ops)

            /**
             *  @brief Get sequence of operations included/generated within a particular block
             *  @param block_num Height of the block whose generated virtual operations should be returned
             *  @param only_virtual Whether to only include virtual operations in returned results (default: true)
             *  @return sequence of operations included/generated within the block
             */
            (get_ops_in_block)

            (get_nft_token_ops)

            (get_transaction)
        )
    private:
        struct plugin_impl;

        std::unique_ptr<plugin_impl> pimpl;
    };

} } } // golos::plugins::operation_history
