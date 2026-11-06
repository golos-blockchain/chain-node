#include <golos/network/rpc_search.hpp>
#include <cassert>
#include <iostream>
using golos::network::rpc_search;
int main() {
    for (unsigned count : {16u,64u}) {
        rpc_search s;
        for (unsigned i=1;i<=count;++i) s.add("8.8.8."+std::to_string(i)+":1776",false,rpc_search::source::cache);
        s.add("9.9.9.9:1776");
        bool bootstrap=false, referral=false;
        for(unsigned i=0;i<16;++i) {
            auto ep=s.next(i*250);
            if(i==0) assert(ep=="8.8.8.1:1776");
            if(ep=="9.9.9.9:1776") { bootstrap=true; s.referrals({"1.1.1.1:1776"}); }
            if(ep=="1.1.1.1:1776") referral=true;
        }
        assert(bootstrap && referral && "dead cache starves bootstrap and its referral");
    }
    // A referral flood arriving first cannot take reserved cache/bootstrap admission.
    rpc_search flooded;
    std::vector<std::string> spam;
    for(unsigned i=1;i<=64;++i) spam.push_back("11.0.0."+std::to_string(i)+":1776");
    for(unsigned i=0;i<16;++i) flooded.referrals(spam);
    for(unsigned i=1;i<=64;++i) {
        flooded.add("8.8.8."+std::to_string(i)+":1776",false,rpc_search::source::cache);
        flooded.add("9.9.9."+std::to_string(i)+":1776");
    }
    assert(flooded.queued()==64); // 22 cache + 21 bootstrap + 21 referral, never 192.
    for(unsigned i=0;i<16;++i) {
        auto endpoint=flooded.next(i*250);
        const char* prefix=i%3==0 ? "8.8.8." : i%3==1 ? "9.9.9." : "11.0.0.";
        assert(endpoint.compare(0,std::string(prefix).size(),prefix)==0);
        assert(flooded.next(i*250+249).empty());
        assert(flooded.queued()<=64);
    }
    assert(flooded.next(4000).empty());
    assert(!flooded.add("127.0.0.1:1",true,rpc_search::source::referral));
    assert(!flooded.add("127.0.0.1:1",true,rpc_search::source::cache));
    flooded.cancel(); assert(flooded.queued()==0 && flooded.next(5000).empty());
    // All response slots were consumed by the first flood; a new one cannot add work.
    rpc_search responses;
    for(unsigned i=0;i<16;++i) responses.referrals({"11.0.0.1:1776"});
    responses.referrals({"11.0.0.2:1776"}); assert(responses.queued()==1);
    std::cout << "L3 cache/bootstrap/referral fairness, reserved admission, rate/attempt/response budgets PASS\n";
}
