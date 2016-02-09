#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <list>

/* REAMDE: The design of the async functions is unnecessarily convoluted because of an issue
 * with promises. They contain a data race between the set_value and get_future function
 * (http://wg21.cmeerw.net/lwg/issue2412). To fix this, I added an atomic bool `ready` and busy
 * loop on it until the promise's value is set. A conditional variable would be a more standard
 * alternative, but then I'd have to deal with their (theoretical) unreliability.
 */

class thread_pool{
public:
	thread_pool(unsigned int);
	~thread_pool();
	
	/**
	 * Pushes a new task (that returns a value and takes arguments) into 
	 * the queue.
	 *
	 * @param f the function to call when executing the task
	 * @param args the arguments to pass to the function
	 *
	 * @return the future used to wait on the task and get the result
	 */
	template<typename Ret, typename... Args>
	std::future<Ret> async(std::function<Ret(Args...)> f, Args... args){
		typedef std::function<Ret(Args...)> F;
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::promise<Ret> *p = new std::promise<Ret>;
		
		// Create a function to package as a task.
		auto task_wrapper = [p, ready](F&& f, Args... args){
			p->set_value(f(args...));
			ready->store(true);
		};
		
		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p, ready]() -> Ret{
			// Workaround. See readme.
			while(!ready->load())
				std::this_thread::yield();
			auto temp = p->get_future().get();
			
			// Clean up resources
			delete p;
			delete ready;
			
			return temp;
		};
		
		task_mutex.lock();
		
		// Package the task wrapper into a function to execute as a task.
		auto task = std::async(std::launch::deferred, 
			task_wrapper, 
			std::move(f), 
			args...);
			
		// Push the task onto the work queue.
		tasks.emplace_back(std::move(task));
		
		task_mutex.unlock();
		
		// Package the return wrapper into a function for user to call to wait for the task to 
		// complete and to get the result.
		return std::async(std::launch::deferred, 
				ret_wrapper);
	}
	
	/**
	 * Pushes a new task (that returns a value and doens't take arguments)
	 * into the queue.
	 *
	 * @param f the function to call when executing the task
	 *
	 * @return the future used to wait on the task and get the result
	 */
	template<typename Ret>
	std::future<Ret> async(std::function<Ret()> f){
		typedef std::function<Ret()> F;
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::promise<Ret> *p = new std::promise<Ret>;
				
		// Create a function to package as a task.
		auto task_wrapper = [p, ready](F&& f){
			p->set_value(f());
			ready->store(true);
		};
				
		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p, ready]() -> Ret{
			// Workaround. See readme.
			while(!ready->load())
				std::this_thread::yield();
			auto temp = p->get_future().get();
			
			// Clean up resources
			delete p;
			delete ready;
			
			return temp;
		};
		
		task_mutex.lock();
				
		// Package the task wrapper into a function to execute as a task.
		auto task = std::async(std::launch::deferred, 
			task_wrapper,
			std::move(f));
			
		// Push the task onto the work queue.
		tasks.emplace_back(std::move(task));
		
		task_mutex.unlock();
		
		// Package the return wrapper into a function for user to call to wait for the task to 
		// complete and to get the result.
		return std::async(std::launch::deferred, 
				ret_wrapper);
	}
	
	/**
	 * Pushes a new task (that doesn't return a value and does take 
	 * arguments) into the queue.
	 *
	 * @param f the function to call when executing the task
	 * @param args the arguments to pass to the function
	 *
	 * @return the future used to wait on the task
	 */
	template<typename... Args>
	std::future<void> async(std::function<void(Args...)> f, Args... args){
		typedef std::function<void(Args...)> F;
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::promise<void> *p = new std::promise<void>;
				
		// Create a function to package as a task.
		auto task_wrapper = [p, ready](F&& f, Args... args){
			f(args...);
			p->set_value();
			ready->store(true);
		};
				
		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p, ready](){
			// Workaround. See readme.
			while(!ready->load())
				std::this_thread::yield();
			p->get_future().get();
			
			// Clean up resources
			delete p;
			delete ready;
			
			return;
		};
		
		task_mutex.lock();		
		
		// Package the task wrapper into a function to execute as a task.
		auto task = std::async(std::launch::deferred, 
			task_wrapper,
			std::move(f),
			args...);
			
		// Push the task onto the work queue.
		tasks.emplace_back(std::move(task));
		
		task_mutex.unlock();
		
		// Package the return wrapper into a function for user to call to wait for the task to 
		// complete and to get the result.
		return std::async(std::launch::deferred, 
				ret_wrapper);
	}
	
	/**
	 * Pushes a new task (that doesn't return a value and doesn't that
	 * arguments) into the queue.
	 *
	 * @param f the function to call when executing the task
	 *
	 * @return the future used to wait on the task
	 */
	std::future<void> async(std::function<void()> f){
		typedef std::function<void()> F;
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::promise<void> *p = new std::promise<void>;
				
		// Create a function to package as a task.
		auto task_wrapper = [p, ready](F&& f){
			f();
			p->set_value();
			ready->store(true);
		};
				
		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p, ready](){
			// Workaround. See readme.
			while(!ready->load())
				std::this_thread::yield();
			p->get_future().get();
			
			// Clean up resources
			delete p;
			delete ready;
			
			return;
		};
		
		task_mutex.lock();
				
		// Package the task wrapper into a function to execute as a task.
		auto task = std::async(std::launch::deferred, 
			task_wrapper,
			std::move(f));
			
		// Push the task onto the work queue.
		tasks.emplace_back(std::move(task));
		
		task_mutex.unlock();
		
		// Package the return wrapper into a function for user to call to wait for the task to 
		// complete and to get the result.
		return std::async(std::launch::deferred, 
				ret_wrapper);
	}

protected:
	void thread_func();
	
	void init_threads();
	
private:
	bool join = false;
	unsigned int num_threads;
	
	std::mutex task_mutex;
	std::deque<std::future<void>> tasks;

	std::list<std::thread> threads;
};

#endif // THREAD_POOL_HPP
