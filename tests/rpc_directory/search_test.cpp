#include <golos/network/rpc_search.hpp>
#include <cassert>
#include <iostream>
using namespace golos::network;
int main() {
    rpc_search s;
    assert(s.add("8.8.8.8:1776"));
    assert(!s.add("127.0.0.1:1776"));
    assert(!s.add("10.0.0.1:1776"));
    assert(!s.add("169.254.1.1:1776"));
    assert(!s.add("224.1.1.1:1776"));
    assert(!s.add("192.0.2.1:1776"));
    assert(!s.add("8.8.8.8:0"));
    assert(!s.add("name.org:1776"));
    assert(!s.add("8.8.8.8.:1776"));
    assert(!s.add("8.8.8.8:01776"));
    assert(!s.add("8.8.8.8:1776"));
    assert(s.next(0) == "8.8.8.8:1776");
    assert(s.next(0).empty());
    s.referrals({"9.9.9.9:1776", "8.8.8.8:1776", "127.0.0.1:1"});
    assert(s.next(249).empty());
    assert(s.next(250) == "9.9.9.9:1776");
    s.cancel();
    assert(s.next(500).empty());
    rpc_search capped;
    for (int i=1; i<255; ++i) capped.add("8.8.8."+std::to_string(i)+":1776");
    assert(capped.queued() == 21); // Bootstrap reservation; other classes retain admission.
    for (int i=0; i<16; ++i) assert(!capped.next(i*250).empty());
    assert(capped.next(5000).empty());
    auto cache=rpc_search::cache_encode({"8.8.8.8:1776", "127.0.0.1:1"},100);
    rpc_search timed; timed.add("8.8.4.4:1776"); assert(timed.next(60000).empty());
    rpc_search responses;
    for(int i=0;i<100;++i) responses.referrals({}); // Empty snapshot polls consume no wire budget.
    for(int i=1;i<=17;++i) responses.referrals({"9.9.9."+std::to_string(i)+":1776"});
    assert(responses.queued()==16);
    rpc_search large;
    std::vector<std::string> batch;
    for(int i=1;i<100;++i) batch.push_back("9.9.9."+std::to_string(i)+":1776");
    large.referrals(batch); assert(large.queued()==21); // Referral class cannot crowd cache/bootstrap.
    assert(rpc_search::cache_decode(cache,101).size()==1);
    assert(rpc_search::cache_decode(cache,3701).empty());
    assert(rpc_search::cache_decode(cache,99).empty());
    assert(rpc_search::cache_decode("999999999999999999999999999\n",101).empty());
    assert(rpc_search::cache_decode(std::string(8193,'x'),101).empty());
    std::cout << "search filter/dedup/cycle/queue/attempt/rate/cancel PASS\n";
}
