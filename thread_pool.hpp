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
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::promise<Ret> *p = new std::promise<Ret>;
		
		auto task_wrapper = [p, ready](F&& f, Args... args){
			p->set_value(f(args...));
			ready->store(true);
		};
		
		auto ret_wrapper = [p, ready]() -> Ret{
			while(!ready->load())
				std::this_thread::yield();
			auto temp = p->get_future().get();
			
			// Clean up resources
			delete p;
			delete ready;
			return temp;
		};
		
		task_mutex.lock();
		tasks.emplace_back(std::async(std::launch::deferred, 
			task_wrapper, std::move(f), args...));
		task_mutex.unlock();
		
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
		
		auto task_wrapper = [p, ready](F&& f){
			p->set_value(f());
			ready->store(true);
		};
		
		auto ret_wrapper = [p, ready]() -> Ret{
			while(!ready->load())
				std::this_thread::yield();
			auto temp = p->get_future().get();
			
			// Clean up resources
			delete p;
			delete ready;
			return temp;
		};
		
		task_mutex.lock();
		tasks.emplace_back(std::async(std::launch::deferred, 
			task_wrapper, std::move(f)));
		task_mutex.unlock();
		
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
		
		auto task_wrapper = [p, ready](F&& f, Args... args){
			f(args...);
			p->set_value();
			ready->store(true);
		};
		
		auto ret_wrapper = [p, ready](){
			while(!ready->load())
				std::this_thread::yield();
			p->get_future().get();
			
			// Clean up resources
			delete p;
			delete ready;
			return;
		};
		
		task_mutex.lock();
		tasks.emplace_back(std::async(std::launch::deferred, 
			task_wrapper, std::move(f), args...));
		task_mutex.unlock();
		
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
		
		auto task_wrapper = [p, ready](F&& f){
			f();
			p->set_value();
			ready->store(true);
		};
		
		auto ret_wrapper = [p, ready](){
			while(!ready->load())
				std::this_thread::yield();
			p->get_future().get();
			
			// Clean up resources
			delete p;
			delete ready;
			return;
		};
		
		task_mutex.lock();
		tasks.emplace_back(std::async(std::launch::deferred, 
			task_wrapper, std::move(f)));
		task_mutex.unlock();
		
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
