#include <golos/network/rpc_search.hpp>
#include <cassert>
#include <set>
#include <iostream>
using golos::network::rpc_search;
int main() {
    for (auto origin : {rpc_search::source::cache, rpc_search::source::bootstrap, rpc_search::source::referral}) {
        rpc_search s;
        for(unsigned i=1;i<=64;++i) s.add("8.8.8."+std::to_string(i)+":1776",false,origin);
        // Every class is full; promotion must not require another admission slot.
        for(unsigned i=1;i<=64;++i) {
            s.add("9.9.9."+std::to_string(i)+":1776");
            s.add("11.0.0."+std::to_string(i)+":1776",false,rpc_search::source::referral);
            s.add("12.0.0."+std::to_string(i)+":1776",false,rpc_search::source::cache);
        }
        assert(s.queued()==64);
        const std::string target="8.8.8.21:1776";
        s.referrals({target}); assert(s.queued()==64);
        std::set<std::string> attempted; unsigned hit=0;
        for(unsigned i=0;i<16;++i) {
            auto ep=s.next(i*250); assert(!ep.empty()); assert(attempted.insert(ep).second);
            if(ep==target) { ++hit; s.referrals({target}); }
            assert(s.next(i*250+249).empty());
        }
        assert(hit==1); assert(s.next(4000).empty());
        s.cancel(); s.referrals({target}); assert(s.queued()==0 && s.next(5000).empty());
    }
    rpc_search private_seed;
    private_seed.add("127.0.0.1:1",true); private_seed.add("127.0.0.1:2",true);
    private_seed.referrals({"127.0.0.1:2"});
    assert(private_seed.next(0)=="127.0.0.1:1");
    assert(private_seed.next(250)=="127.0.0.1:2");
    private_seed.referrals({"127.0.0.1:2"}); assert(private_seed.next(500).empty());
    rpc_search capped;
    capped.add("8.8.8.1:1776"); capped.add("8.8.8.2:1776");
    for(unsigned i=0;i<16;++i) capped.referrals({"8.8.8.1:1776"});
    capped.referrals({"8.8.8.2:1776"}); assert(capped.next(0)=="8.8.8.1:1776");
    rpc_search deadline; deadline.add("8.8.8.1:1776"); assert(deadline.next(60000).empty());
    std::cout << "overlap all classes/full reservations/flood/no retry/private trust/cancel/budgets PASS\n";
}
