// Owned encrypted provider sends 40x64 nonsupporting loopback referrals.
#include <golos/network/message_oriented_connection.hpp>
#include <golos/network/core_messages.hpp>
#include <golos/network/rpc_directory.hpp>
#include <golos/protocol/config.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/tcp_socket.hpp>
#include <iostream>
#include <csignal>
using namespace golos::network;
static volatile std::sig_atomic_t stopped=0;
static void stop(int){stopped=1;}
struct provider:message_oriented_connection_delegate {
 message_oriented_connection wire{this}; bool sent=false;
 void on_connection_closed(message_oriented_connection*) override {}
 void on_message(message_oriented_connection*,const message& m) override {
  if(m.msg_type==hello_message::type && !sent) {
   sent=true; auto key=fc::ecc::private_key::generate(); auto secret=wire.get_shared_secret(); fc::sha256::encoder enc; enc.write(secret.data(),sizeof(secret));
   rpc_directory::catalog c; auto now=fc::time_point::now().sec_since_epoch(); c.announce("https://rpc.example.org",now+600,now);
   wire.send_message(hello_message("owned-flood-provider",106,fc::ip::address("127.0.0.1"),0,0,key.get_public_key(),key.sign_compact(enc.result()),fc::mutable_variant_object("chain_id",STEEMIT_CHAIN_ID)("golos.discovery_only.v1",true)(rpc_directory::hello_key,c.hello(now))));
   wire.send_message(connection_accepted_message());
  }
  if(m.msg_type==address_request_message::type) {
   address_message a; for(unsigned i=0;i<64;++i) a.addresses.emplace_back(fc::ip::endpoint::from_string("127.0.0.1:"+std::to_string(20000+i)),fc::time_point_sec(),fc::microseconds(),node_id_t(),peer_connection_direction::inbound,firewalled_state::not_firewalled);
   for(unsigned i=0;i<40;++i) wire.send_message(a);
  }
 }
};
int main(){try{
 std::signal(SIGTERM,stop); fc::tcp_server listener; listener.listen(fc::ip::endpoint::from_string("127.0.0.1:0"));
 std::cout<<std::string(listener.get_local_endpoint())<<std::endl;
 std::vector<std::unique_ptr<provider>> peers;
 auto accept=fc::async([&]{while(!stopped){std::unique_ptr<provider> p(new provider); listener.accept(p->wire.get_socket()); p->wire.accept(); peers.push_back(std::move(p));}},"owned provider accept");
 while(!stopped) { fc::usleep(fc::milliseconds(25)); }
 listener.close(); accept.cancel_and_wait(); for(auto& p:peers)p->wire.close_connection(); return 0;
}catch(const fc::exception& e){std::cerr<<e.to_detail_string();return 1;}}
