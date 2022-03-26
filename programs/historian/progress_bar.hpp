#include <indicators/progress_bar.hpp>
#include <golos/chain/database.hpp>

using namespace golos::chain;

class progress_bar {
private:
    indicators::ProgressBar* p = nullptr;

    std::string fmt_found(uint64_t found) {
        if (found > 10000) {
            return "     >10000 found ";
        }
        return std::string("     ") + std::to_string(found) + " found ";
    }
public:
    progress_bar() {
        p = new indicators::ProgressBar();
        p->set_option(indicators::option::BarWidth{40});
        p->set_option(indicators::option::Fill{"■"});
        p->set_option(indicators::option::Lead{"■"});
        p->set_option(indicators::option::ShowPercentage{true});
        p->set_option(indicators::option::PrefixText{this->fmt_found(0)});
        p->set_option(indicators::option::PostfixText{"2016-01-01T01:01:01"});
    }

    ~progress_bar() {
        delete p;
    }

    void update_progress(uint64_t found, uint64_t* old_found,
            const signed_block& block, uint32_t head_block_num,
            uint32_t* old_pct) {
        auto block_num = block.block_num();

        auto pct = 100 / std::max(uint32_t(1), head_block_num / block_num);
        pct = std::min(pct, uint32_t(99));
        if (block_num % 50000 == 0) {
            p->set_option(indicators::option::PostfixText{block.timestamp.to_iso_string()});
        }
        if (found > *old_found) {
            p->set_option(indicators::option::PrefixText{this->fmt_found(found)});
            p->set_progress(pct);
            *old_found = found;
            *old_pct = pct;
        } else if (pct - *old_pct >= 2) {
            p->set_progress(pct);
            *old_pct = pct;
        }
    }

    void complete(const signed_block& head_block) {
        p->set_option(indicators::option::PostfixText{head_block.timestamp.to_iso_string()});
        p->set_progress(100);
    }
};
