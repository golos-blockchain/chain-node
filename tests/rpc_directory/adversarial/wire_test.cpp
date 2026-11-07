// Real FC encrypted loopback, using public message_oriented_connection; no node hooks.
#include <golos/network/discovery_delegate.hpp>
#include <golos/network/message_oriented_connection.hpp>
#include <golos/network/core_messages.hpp>
#include <golos/protocol/config.hpp>
#include <fc/thread/thread.hpp>
#include <fc/filesystem.hpp>
#include <iostream>
using namespace golos::network;
static void settle() { fc::usleep(fc::milliseconds(180)); }
struct attacker : message_oriented_connection_delegate {
    message_oriented_connection wire{this};
    bool hello=false, accepted=false, closed=false;
    void on_message(message_oriented_connection*,const message& m) override {
        if(m.msg_type==hello_message::type) hello=true;
        if(m.msg_type==connection_accepted_message::type) accepted=true;
    }
    void on_connection_closed(message_oriented_connection*) override { closed=true; }
    void handshake(bool wrong=false) {
        auto key=fc::ecc::private_key::generate(); auto secret=wire.get_shared_secret();
        fc::sha256::encoder enc; enc.write(secret.data(),sizeof(secret));
        fc::variant chain=wrong?fc::variant(fc::sha256::hash("fictional-wrong-chain")):fc::variant(STEEMIT_CHAIN_ID);
        wire.send_message(hello_message("owned-adversarial",106,fc::ip::address("127.0.0.1"),0,0,
            key.get_public_key(),key.sign_compact(enc.result()),fc::mutable_variant_object("chain_id",chain)("golos.discovery_only.v1",true)));
        wire.send_message(connection_accepted_message()); settle();
    }
    void raw(std::vector<char> data) { message m; m.msg_type=address_message::type; m.data=std::move(data); m.size=m.data.size(); wire.send_message(m); }
    void addresses(unsigned n) {
        address_message m;
        for(unsigned i=0;i<n;++i) m.addresses.emplace_back(fc::ip::endpoint::from_string("127.0.0.1:"+std::to_string(20000+i)),fc::time_point_sec(),fc::microseconds(),node_id_t(),peer_connection_direction::inbound,firewalled_state::not_firewalled);
        wire.send_message(m);
    }
};
static void test(const std::string& mode) {
    fc::temp_directory config; discovery_delegate delegate; node victim("owned-victim");
    victim.load_configuration(config.path()); victim.set_node_delegate(&delegate); victim.set_discovery_only(true);
    victim.disable_peer_advertising(); victim.listen_on_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0"),false);
    victim.listen_to_p2p_network(); victim.connect_to_p2p_network();
    attacker a; a.wire.connect_to(victim.get_actual_listening_endpoint()); settle(); FC_ASSERT(a.hello);
    if(mode!="prehello") a.handshake(mode=="wrongchain");
    if(mode=="prehello" || mode=="wrongchain") { a.addresses(1); settle(); FC_ASSERT(victim.take_discovery_addresses().empty()); }
    else {
        FC_ASSERT(a.accepted,"Handshake failed");
        if(mode=="rejected") { a.wire.send_message(connection_rejected_message()); settle(); a.addresses(1); settle(); FC_ASSERT(victim.take_discovery_addresses().empty()); }
        else {
            if(mode=="count") a.raw({char(0xff),char(0xff),char(0xff),char(0xff),char(0x0f)});
            if(mode=="payload") a.raw(std::vector<char>(16385,0));
            if(mode=="count" || mode=="payload") { settle(); FC_ASSERT(victim.take_discovery_addresses().empty()); FC_ASSERT(!a.closed,"Malformed message was decoded/closed instead of ignored"); }
            a.addresses(64); settle(); FC_ASSERT(victim.take_discovery_addresses().size()==64,"Valid positive-control response missing");
            for(unsigned i=0;i<40;++i) a.addresses(64);
            settle(); FC_ASSERT(victim.take_discovery_addresses().empty(),"Duplicate response flood fed crawler");
        }
    }
    a.wire.close_connection(); victim.close(); FC_ASSERT(delegate.chain_call_count()==0);
    std::cout<<"encrypted-wire "<<mode<<": PASS\n";
}
static void global_caps(bool drain) {
    fc::temp_directory config; discovery_delegate delegate; node victim("owned-cap-victim");
    victim.load_configuration(config.path()); victim.set_node_delegate(&delegate); victim.set_discovery_only(true);
    victim.disable_peer_advertising(); victim.listen_on_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0"),false);
    victim.listen_to_p2p_network(); victim.connect_to_p2p_network();
    std::vector<std::unique_ptr<attacker>> peers;
    for(unsigned i=0;i<(drain?18:3);++i) {
        peers.emplace_back(new attacker); auto& a=*peers.back();
        a.wire.connect_to(victim.get_actual_listening_endpoint()); settle(); a.handshake(); FC_ASSERT(a.accepted);
        a.addresses(drain?1:64); settle();
        if(drain) FC_ASSERT(victim.take_discovery_addresses().size()==(i<16?1:0),"Global response cap");
    }
    if(!drain) FC_ASSERT(victim.take_discovery_addresses().size()==128,"Global queued-address cap");
    victim.close(); FC_ASSERT(victim.take_discovery_addresses().empty()); FC_ASSERT(delegate.chain_call_count()==0);
    for(auto& a:peers) a->wire.close_connection();
    std::cout<<"encrypted-wire "<<(drain?"global-response-16":"global-queue-128")<<": PASS\n";
}
int main() { try { for(auto mode:{"prehello","wrongchain","rejected","count","payload","flood"}) test(mode); global_caps(false); global_caps(true); return 0; }
catch(const fc::exception& e) { std::cerr<<e.to_detail_string(); return 1; } }
