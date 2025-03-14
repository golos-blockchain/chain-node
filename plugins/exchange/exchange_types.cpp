#include <golos/plugins/exchange/exchange_types.hpp>

namespace golos {
    namespace plugins {
        namespace exchange {
            const ex_chain* get_direct_chain(const std::vector<ex_chain>& chains) {
                auto itr = std::find_if(chains.begin(), chains.end(), [&](const auto& chain) {
                    return chain.size() == 1;
                });
                if (itr != chains.end()) {
                    return &(*itr);
                }
                return nullptr;
            }
        }
    }
} // golos::plugins::exchange
