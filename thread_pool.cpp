#include "thread_pool.hpp"

using namespace std;

/**
 * Constructs a thread pool with `num_threads` threads and an empty work queue.
 */
thread_pool::thread_pool(unsigned int num_threads) : num_threads(num_threads){
	task_mutex.lock();
	init_threads();
	task_mutex.unlock();
}

/**
 * Destructs a thread pool, waiting on tasks to finish.
 */
thread_pool::~thread_pool(){
	task_mutex.lock();
	join = true;
	task_mutex.unlock();
	for(auto i = threads.begin();i != threads.end();i++)
		i->join();
}

/**
 * Creates threads for the thread pool.
 */
void thread_pool::init_threads(){
	for(int i = 0;i < num_threads;i++){
		std::function<void(void)> f = std::bind(&thread_pool::thread_func, this);
		threads.push_back(std::move(std::thread(f)));
	}
}

/**
 * Manages thread execution. This is the function that threads actually run.
 * It pulls a task out of the queue and executes it.
 */
void thread_pool::thread_func(){
	for(;;){
		// Lock the queue.
		task_mutex.lock();

		// If there's nothing to do and we're not ready to join, just
		// yield.
		if(tasks.empty() && !join){
			task_mutex.unlock();
			this_thread::yield();
			continue;
		}
		// If there's tasks waiting, do one.
		else if(!tasks.empty()){
			// Get a task.
			auto f = std::move(tasks.front());
			tasks.pop_front();
			
			// Unlock the queue.
			task_mutex.unlock();
			
			// Execute the async function.
			f.get();
		}
		// If there's no tasks and we're ready to join, then exit the
		// function (effectively joining).
		else if(join){
			task_mutex.unlock();
			return;
		}
	}
}
