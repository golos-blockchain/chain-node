#pragma once

namespace golos { namespace plugins { namespace database_api {

    enum class asset_api_sort {
        by_symbol_name,
        by_marketed,
    };

} } } // golos::plugins::database_api

FC_REFLECT_ENUM(golos::plugins::database_api::asset_api_sort, (by_symbol_name)(by_marketed))
