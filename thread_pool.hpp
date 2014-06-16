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
	
	template<typename Ret, typename... Args>
	std::future<Ret> async(std::function<Ret(Args...)> f, Args... args){
		typedef std::function<Ret(Args...)> F;
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::mutex *cv_mutex = new std::mutex;
		std::mutex *finish_mutex = new std::mutex;
		std::condition_variable_any *cv = new std::condition_variable_any;
		
		std::promise<Ret> *p = new std::promise<Ret>;
		
		auto task_wrapper = [cv, finish_mutex, p, ready]
		(F&& f, Args&&... args){
			p->set_value(f(args...));
			ready->store(true);
			finish_mutex->lock();
			cv->notify_all();
			finish_mutex->unlock();
		};
		
		auto ret_wrapper = [cv, cv_mutex, finish_mutex, p, ready]
		() -> Ret{
			if(!ready->load()){
				cv_mutex->lock();
				cv->wait(*cv_mutex, 
					[ready]() -> bool {return ready->load();}
				);
				cv_mutex->unlock();
			}
			auto temp = p->get_future().get();
			
			// Clean up resources
			
			finish_mutex->lock();
			delete cv;
			delete cv_mutex;
			delete finish_mutex;
			delete p;
			delete ready;
			return temp;
		};
		
		task_mutex.lock();
		tasks.emplace_back(std::async(std::launch::deferred, 
			task_wrapper, std::move(f), std::move(args...)));
		task_mutex.unlock();
		
		return std::async(std::launch::deferred, 
				ret_wrapper);
	}
	
	template<typename Ret>
	std::future<Ret> async(std::function<Ret()> f){
		typedef std::function<Ret()> F;
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::mutex *cv_mutex = new std::mutex;
		std::mutex *finish_mutex = new std::mutex;
		std::condition_variable_any *cv = new std::condition_variable_any;
		
		std::promise<Ret> *p = new std::promise<Ret>;
		
		auto task_wrapper = [cv, finish_mutex, p, ready](F&& f){
			p->set_value(f());
			ready->store(true);
			finish_mutex->lock();
			cv->notify_all();
			finish_mutex->unlock();
		};
		
		auto ret_wrapper = [cv, cv_mutex, finish_mutex, p, ready]() -> Ret{
			if(!ready->load()){
				cv_mutex->lock();
				cv->wait(*cv_mutex, 
					[ready]() -> bool {return ready->load();}
				);
				cv_mutex->unlock();
			}
			auto temp = p->get_future().get();
			
			// Clean up resources
			
			finish_mutex->lock();
			delete cv;
			delete cv_mutex;
			delete finish_mutex;
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
	
	template<typename... Args>
	std::future<void> async(std::function<void(Args...)> f, Args... args){
		typedef std::function<void(Args...)> F;
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::mutex *cv_mutex = new std::mutex;
		std::mutex *finish_mutex = new std::mutex;
		std::condition_variable_any *cv = new std::condition_variable_any;
		
		std::promise<void> *p = new std::promise<void>;
		
		auto task_wrapper = [cv, finish_mutex, p, ready]
		(F&& f, Args&&... args){
			f(args...);
			p->set_value();
			ready->store(true);
			finish_mutex->lock();
			cv->notify_all();
			finish_mutex->unlock();
		};
		
		auto ret_wrapper = [cv, cv_mutex, finish_mutex, p, ready](){
			if(!ready->load()){
				cv_mutex->lock();
				cv->wait(*cv_mutex, 
					[ready]() -> bool {return ready->load();}
				);
				cv_mutex->unlock();
			}
			p->get_future().get();
			
			// Clean up resources
			
			finish_mutex->lock();
			delete cv;
			delete cv_mutex;
			delete finish_mutex;
			delete p;
			delete ready;
			return;
		};
		
		task_mutex.lock();
		tasks.emplace_back(std::async(std::launch::deferred, 
			task_wrapper, std::move(f), std::move(args...)));
		task_mutex.unlock();
		
		return std::async(std::launch::deferred, 
				ret_wrapper);
	}
	
	std::future<void> async(std::function<void()> f){
		typedef std::function<void()> F;
		
		std::atomic<bool> *ready = new std::atomic<bool>(false);
		std::mutex *cv_mutex = new std::mutex;
		std::mutex *finish_mutex = new std::mutex;
		std::condition_variable_any *cv = new std::condition_variable_any;
		
		std::promise<void> *p = new std::promise<void>;
		
		auto task_wrapper = [cv, finish_mutex, p, ready](F&& f){
			f();
			p->set_value();
			ready->store(true);
			finish_mutex->lock();
			cv->notify_all();
			finish_mutex->unlock();
		};
		
		auto ret_wrapper = [cv, cv_mutex, finish_mutex, p, ready](){
			if(!ready->load()){
				cv_mutex->lock();
				cv->wait(*cv_mutex, 
					[ready]() -> bool {return ready->load();}
				);
				cv_mutex->unlock();
			}
			p->get_future().get();
			
			// Clean up resources
			
			finish_mutex->lock();
			delete cv;
			delete cv_mutex;
			delete finish_mutex;
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
