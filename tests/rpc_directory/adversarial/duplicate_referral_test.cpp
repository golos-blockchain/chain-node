#include <golos/network/rpc_search.hpp>
#include <iostream>
using golos::network::rpc_search;
int main(){
 rpc_search s;
 for(unsigned i=1;i<=64;++i) s.add("8.8.8."+std::to_string(i)+":1776",false,rpc_search::source::cache);
 s.add("9.9.9.9:1776");
 bool tried=false;
 for(unsigned i=0;i<16;++i){
  auto ep=s.next(i*250);
  if(ep=="9.9.9.9:1776")s.referrals({"8.8.8.22:1776"});
  if(ep=="8.8.8.22:1776") tried=true;
  std::cout<<ep<<"\n";
 }
 std::cout<<"Bootstrap returned cached #22 as only working provider. Provider attempted: "<<tried<<"\n";
 return tried?0:1;
}
