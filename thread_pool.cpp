#include "thread_pool.hpp"

using namespace std;

thread_pool::thread_pool(unsigned int num_threads) : num_threads(num_threads){
	task_mutex.lock();
	init_threads();
	task_mutex.unlock();
}

thread_pool::~thread_pool(){
	task_mutex.lock();
	join = true;
	task_mutex.unlock();
	for(auto i = threads.begin();i != threads.end();i++)
		i->join();
}

void thread_pool::init_threads(){
	for(int i = 0;i < num_threads;i++){
		std::function<void(void)> f = std::bind(&thread_pool::thread_func, this);
		threads.push_back(std::move(std::thread(f)));
	}
}

void thread_pool::thread_func(){
	for(;;){
		task_mutex.lock();
		if(tasks.empty() && !join){
			task_mutex.unlock();
			this_thread::yield();
			continue;
		}
		else if(!tasks.empty()){
			auto f = std::move(tasks.front());
			tasks.pop_front();
			
			task_mutex.unlock();
			
			f.get();
		}
		else if(join){
			task_mutex.unlock();
			return;
		}
	}
}
