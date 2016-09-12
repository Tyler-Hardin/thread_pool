#include "thread_pool.hpp"

using namespace std;

thread_pool::thread_pool(unsigned int n) : base_thread_pool(n) {
	init_mutex.unlock();
}

thread_pool::~thread_pool() {
	wait();
}

optional<std::future<void>> thread_pool::get_task() {
	optional<std::future<void>> ret;
	lock_guard<mutex> lk(task_mutex);
	if(!tasks.empty()) {
		ret = std::move(tasks.front());
		tasks.pop();
	}
	return ret;
}

void thread_pool::handle_task(std::future<void> f) {
	f.get();
}
