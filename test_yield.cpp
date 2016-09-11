#include "thread_pool.hpp"

#include <vector>

#include <iostream>

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

using namespace std;

static void core_dump(int sigid)
{
    kill(getpid(), SIGSEGV);
}

const int num_threads = 3;

void test_void_void(){
	priority_thread_pool p(num_threads);
	vector<future<void>> f;

    std::function<void()> low_prio = []() {
        long long i = 0;
        for(;;) {
            ++i;
            if(i % 100000000 == 0) {
                cout << "L" << endl;
                i = 0;
                priority_thread_pool::yield();
            }
        }
    };

    std::function<void()> high_prio = []() {
        long long i = 0;
        for(;;) {
            ++i;
            if(i % 100000000 == 0) {
                cout << "H" << endl;
                i = 0;
                priority_thread_pool::yield();
            }
        }
    };

    // Saturate threads with low prio task.
	for(int i = 0;i < num_threads * 2;i++)
		f.emplace_back(p.async(0, low_prio));

    usleep(2000000);

   // Push high prio task.
    for(int i = 0;i < num_threads;i++)
        f.emplace_back(p.async(10, high_prio));
    cout << "HIGH PRIO PUSHED" << endl;
}

int main(){
	//signal(SIGINT, core_dump);
	test_void_void();
	return 0;
}
