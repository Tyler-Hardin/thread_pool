#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <list>

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
		
		std::promise<Ret> *p = new std::promise<Ret>;
		
		// Create a function to package as a task.
		auto task_wrapper = std::bind([p, f{std::move(f)}](Args... args){
			p->set_value(std::move(f(std::move(args)...)));
		}, std::move(args)...);
		
		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p]() -> Ret{
			auto temp = std::move(p->get_future().get());
			
			// Clean up resources
			delete p;
			
			return std::move(temp);
		};
		
		task_mutex.lock();
		
		// Package the task wrapper into a function to execute as a task.
		auto task = std::async(std::launch::deferred, task_wrapper);
			
		// Push the task onto the work queue.
		tasks.emplace_back(std::move(task));
		
		task_mutex.unlock();
		
		// Package the return wrapper into a function for user to call to wait for the task to 
		// complete and to get the result.
		return std::async(std::launch::deferred, ret_wrapper);
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
		
		std::promise<Ret> *p = new std::promise<Ret>;
				
		// Create a function to package as a task.
		auto task_wrapper = [p, f{std::move(f)}](){
			p->set_value(std::move(f()));
		};
				
		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p]() -> Ret{
			auto temp = std::move(p->get_future().get());
			
			// Clean up resources
			delete p;
			
			return std::move(temp);
		};
		
		task_mutex.lock();
				
		// Package the task wrapper into a function to execute as a task.
		auto task = std::async(std::launch::deferred, task_wrapper);
			
		// Push the task onto the work queue.
		tasks.emplace_back(std::move(task));
		
		task_mutex.unlock();
		
		// Package the return wrapper into a function for user to call to wait for the task to 
		// complete and to get the result.
		return std::async(std::launch::deferred, ret_wrapper);
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
		
		std::promise<void> *p = new std::promise<void>;
				
		// Create a function to package as a task.
		auto task_wrapper = std::bind([p, f{std::move(f)}](Args... args){
			f(std::move(args)...);
			p->set_value();
		}, std::move(args)...);
				
		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p](){
			p->get_future().get();
			
			// Clean up resources
			delete p;
		};
		
		task_mutex.lock();		
		
		// Package the task wrapper into a function to execute as a task.
		auto task = std::async(std::launch::deferred, task_wrapper);
			
		// Push the task onto the work queue.
		tasks.emplace_back(std::move(task));
		
		task_mutex.unlock();
		
		// Package the return wrapper into a function for user to call to wait for the task to 
		// complete and to get the result.
		return std::async(std::launch::deferred, ret_wrapper);
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
		
		std::promise<void> *p = new std::promise<void>;
				
		// Create a function to package as a task.
		auto task_wrapper = [p, f{std::move(f)}]{
			f();
			p->set_value();
		};
				
		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p](){
			p->get_future().get();
			
			// Clean up resources
			delete p;
		};
		
		task_mutex.lock();
				
		// Package the task wrapper into a function to execute as a task.
		auto task = std::async(std::launch::deferred, task_wrapper);
			
		// Push the task onto the work queue.
		tasks.emplace_back(std::move(task));
		
		task_mutex.unlock();
		
		// Package the return wrapper into a function for user to call to wait for the task to 
		// complete and to get the result.
		return std::async(std::launch::deferred, ret_wrapper);
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
