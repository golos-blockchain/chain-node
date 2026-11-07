// Actual no-copy publication primitive used by CLI; includes atomicity and failure cleanup.
#include <fc/filesystem.hpp>
#include <golos/network/rpc_cache_file.hpp>
static bool replace_cache(const fc::path& from, const fc::path& to) {
#ifdef _WIN32
 return golos::network::atomic_replace(from.preferred_wstring(),to.preferred_wstring());
#else
 return golos::network::atomic_replace(from.to_native_ansi_path(),to.to_native_ansi_path());
#endif
}
#include <fc/exception/exception.hpp>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
static std::string read(const fc::path& p) { std::ifstream f(p.to_native_ansi_path(),std::ios::binary); return std::string(std::istreambuf_iterator<char>(f),{}); }
int main() { try {
    fc::temp_directory dir; auto target=dir.path()/"rpc_search_peers.v1";
    const std::string old(8192,'a'),fresh(8192,'b');
    { std::ofstream f(target.to_native_ansi_path()); f<<old; }
    std::ifstream held(target.to_native_ansi_path());
    std::atomic<bool> done{false},bad{false}; std::atomic<unsigned> reads{0};
    std::thread reader([&] { while(!done) { auto bytes=read(target); if(bytes!=old && bytes!=fresh) bad=true; ++reads; } });
    for(unsigned i=0;i<200;++i) {
        fc::temp_file pending(dir.path(),true);
        std::ofstream file(pending.path().to_native_ansi_path(),std::ios::binary|std::ios::trunc); file<<(i%2?old:fresh); file.close();
        if(file) FC_ASSERT(replace_cache(pending.path(),target));
    }
    done=true; reader.join(); FC_ASSERT(!bad && reads>0);
    FC_ASSERT(std::string(std::istreambuf_iterator<char>(held),{})==old,"Existing descriptor must retain old inode");
    auto before=read(target); bool failed=false;
    failed=!replace_cache(dir.path()/"fictional-missing-pending",target);
    FC_ASSERT(failed && read(target)==before,"Failed rename must retain old file");
    auto incompatible=dir.path()/"owned-pending-directory";
    fc::create_directories(incompatible);
    FC_ASSERT(!replace_cache(incompatible,target));
    FC_ASSERT(read(target)==before && !fc::exists(incompatible),"Failure preserves old bytes and cleans pending");
    std::cout<<"No-copy atomic replacement 200 iterations, "<<reads<<" concurrent reads, old descriptor, failure retains old bytes + cleans pending: PASS\n";
    return 0;
} catch(const fc::exception& e) { std::cerr<<e.to_detail_string(); return 1; } }
