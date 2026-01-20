// Test to reproduce HPX collectives set bug on macOS ARM64
// This mimics the exact code from the GitHub issue example

#include <hpx/hpx_init.hpp>
#include <hpx/modules/collectives.hpp>
#include <hpx/future.hpp>
#include <iostream>

using namespace hpx::collectives;

constexpr char const* channel_communicator_name = "/test/channel_communicator/";

int hpx_main()
{
    std::cout << "Starting HPX collectives set test...\n";
    
    // Get locality info (from GitHub issue example)
    std::uint32_t num_localities = hpx::get_num_localities(hpx::launch::sync);
    std::uint32_t this_locality = hpx::get_locality_id();
    
    std::cout << "Number of localities: " << num_localities << "\n";
    std::cout << "This locality: " << this_locality << "\n";
    
    // Allocate channel communicator (from GitHub issue)
    auto comm = create_channel_communicator(hpx::launch::sync,
        channel_communicator_name, num_sites_arg(num_localities),
        this_site_arg(this_locality));
    
    std::cout << "Channel communicator created successfully\n";
    
    // Set up communication pattern (from GitHub issue)
    std::uint32_t next_locality = (this_locality + 1) % num_localities;
    std::vector<int> msg_vec(num_localities); // resize to avoid out of bounds
    for(size_t i=0; i<num_localities; ++i) msg_vec[i] = i;
    int msg = msg_vec[this_locality];
    
    std::cout << "About to call hpx::collectives::set() - this is where the bug occurs\n";
    
    try {
        // This is the exact line from the GitHub issue that crashes
        auto setf = set(comm, that_site_arg(next_locality), msg, tag_arg(0));
        
        std::cout << "set() call completed, waiting for result...\n";
        setf.get();
        std::cout << "set() operation successful!\n";
        
        // Also test get operation
        std::uint32_t prev_locality = (this_locality + num_localities - 1) % num_localities;
        auto got_msg = get<int>(comm, that_site_arg(prev_locality), tag_arg(0));
        int received = got_msg.get();
        std::cout << "get() operation successful, received: " << received << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << "\n";
        std::cerr << "typeid: " << typeid(e).name() << "\n";
        return hpx::finalize(); // hpx::error::exception not valid
    } catch (...) {
        std::cerr << "Unknown exception caught\n";
        return hpx::finalize(); // hpx::error::exception not valid
    }
    
    std::cout << "All operations completed successfully!\n";
    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    hpx::init_params params;
    params.cfg = {"--hpx:run-hpx-main"};
    return hpx::init(argc, argv, params);
}