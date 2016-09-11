#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <atomic>
#include <csetjmp>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <queue>

#include <experimental/optional>

#include <ucontext.h>

using std::experimental::optional;

class priority_task {
public:
	priority_task(std::function<void()> work, int priority = 0) :
		work(work), priority(priority) {}
	priority_task(const priority_task&) = delete;
	~priority_task();

	bool operator<(const priority_task& t) const;

	bool run();
	void pause();

private:
	std::function<void()> work;

	void* volatile work_stack = nullptr;

	// Used to to pause. Returns to scheduler.
	ucontext_t pause_context;

	// Returns to running task.
	ucontext_t work_context;

	int priority;
	volatile bool started = false;
	volatile bool paused = false;
	volatile bool done = false;

	static void _run(void);
};

template<typename Task>
class base_thread_pool{
protected:
	/**
	 * Wraps tasks in a executor function and wraps the promise which will
	 * receive the return value of the function.
	 *
	 * @param f the function to call when executing the task
	 * @param args the arguments to pass to the function
	 *
	 * @return the future used to wait on the task and get the result
	 */
	template<typename Ret, typename... Args>
	std::pair<std::function<void()>,std::future<Ret>>
	package(std::function<Ret(Args...)> f, Args... args){
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
		
		return make_pair(task_wrapper, std::async(std::launch::deferred, ret_wrapper));
	}
	
	/**
	 * Wraps tasks in a executor function and wraps the promise which will
	 * receive the return value of the function.
	 *
	 * @param f the function to call when executing the task
	 *
	 * @return the future used to wait on the task and get the result
	 */
	template<typename Ret>
	std::pair<std::function<void()>,std::future<Ret>>
	package(std::function<Ret()> f){
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
		
		return make_pair(task_wrapper, std::async(std::launch::deferred, ret_wrapper));
	}
	
	/**
	 * Wraps tasks in a executor function and wraps the promise which will
	 * receive the return value of the function.
	 *
	 * @param f the function to call when executing the task
	 * @param args the arguments to pass to the function
	 *
	 * @return the future used to wait on the task
	 */
	template<typename... Args>
	std::pair<std::function<void()>,std::future<void>>
	package(std::function<void(Args...)> f, Args... args){
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
		
		return make_pair(task_wrapper, std::async(std::launch::deferred, ret_wrapper));
	}
	
	/**
	 * Wraps tasks in a executor function and wraps the promise which will
	 * receive the return value of the function.
	 *
	 * @param f the function to call when executing the task
	 *
	 * @return the future used to wait on the task
	 */
	std::pair<std::function<void()>,std::future<void>>
	package(std::function<void()> f){
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
		
		return make_pair(task_wrapper, std::async(std::launch::deferred, ret_wrapper));
	}

	/**
 	* Constructs a thread pool with `num_threads` threads.
	 */
	base_thread_pool(unsigned int num_threads) : num_threads(num_threads){
		init_mutex.lock();
	 	init_threads();
	}
	
	/**
	 * Destructs a thread pool, waiting on tasks to finish.
	 */
	virtual ~base_thread_pool(){
		task_mutex.lock();
		join = true;
		task_mutex.unlock();
		wait();
	}

	/**
	 * Manages thread execution. This is the function that threads actually run.
	 * It pulls a task out of the queue and executes it.
	 */
	void thread_func(){
		// Can't call get_task until parent class is constructed.
		init_mutex.lock();
		init_mutex.unlock();
		for(;;){
			auto task = get_task();

			// If there's nothing to do and we're not ready to join, just
			// yield.
			if(!task && !join){
				std::this_thread::yield();
				continue;
			}
			// If there's tasks waiting, do one.
			else if(task){
				handle_task(std::move(*task));
			}
			// If there's no tasks and we're ready to join, then exit the
			// function (effectively joining).
			else if(join){
				return;
			}
		}
	}

	/**
	 * Creates threads for the thread pool.
	 */
	void init_threads(){
	 	task_mutex.lock();
		for(int i = 0;i < num_threads;i++){
			std::function<void(void)> f = 
				std::bind(&base_thread_pool::thread_func, this);
			threads.push_back(std::move(std::thread(f)));
	 	}
	 	task_mutex.unlock();
	}

	/**
	 * Waits for threads to exit. Leaves thread pool in unusable state. Used by 
	 * destructors.
	 */
	void wait() {
		while(threads.size() > 0) {
			auto &t = threads.back();
			t.join();
			threads.pop_back();
		}
	}

	/**
	 * Returns the next task, if there is one. None if there isn't.
	 */
	virtual optional<Task> get_task() = 0;

	/**
	 * Executes a task.
	 */
	virtual void handle_task(Task) = 0;

	/// Must be unlocked in child constructors.
	std::mutex init_mutex;
	std::mutex task_mutex;
private:
	bool join = false;
	unsigned int num_threads;
	std::list<std::thread> threads;
};

class thread_pool : public base_thread_pool<std::future<void>>{
public:
	thread_pool(unsigned int);
	virtual ~thread_pool();
	
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
		auto p = package(f, args...);
		auto t = std::async(std::launch::deferred, p.first);
		task_mutex.lock();
		tasks.emplace(std::move(t));
		task_mutex.unlock();
		return std::move(p.second);
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
		auto p = package(f);
		auto t = std::async(std::launch::deferred, p.first);
		task_mutex.lock();
		tasks.emplace(std::move(t));
		task_mutex.unlock();
		return std::move(p.second);
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
		auto p = package<Args...>(f, args...);
		auto t = std::async(std::launch::deferred, p.first);
		task_mutex.lock();
		tasks.emplace(std::move(t));
		task_mutex.unlock();
		return std::move(p.second);
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
		auto p = package(f);
		auto t = std::async(std::launch::deferred, p.first);
		task_mutex.lock();
		tasks.emplace(std::move(t));
		task_mutex.unlock();
		return std::move(p.second);
	}

protected:
	virtual optional<std::future<void>> get_task() override;
	virtual void handle_task(std::future<void>) override;
	
private:
	std::queue<std::future<void>> tasks;
};

class priority_thread_pool : public base_thread_pool<std::shared_ptr<priority_task>>{
public:
	priority_thread_pool(unsigned int n);
	virtual ~priority_thread_pool();

	/**
	 * Pushes a new task (that returns a value and takes arguments) into 
	 * the queue.
	 *
	 * @param priority the priority of this task
	 * @param f the function to call when executing the task
	 * @param args the arguments to pass to the function
	 *
	 * @return the future used to wait on the task and get the result
	 */
	template<typename Ret, typename... Args>
	std::future<Ret> async(int priority, std::function<Ret(Args...)> f, 
		Args... args){
		auto p = package(f, args...);
		auto t = std::shared_ptr<priority_task>(new priority_task(p.first, priority));
		task_mutex.lock();
		tasks.emplace(t);
		task_mutex.unlock();
		return std::move(p.second);
	}
	
	/**
	 * Pushes a new task (that returns a value and doens't take arguments)
	 * into the queue.
	 *
	 * @param priority the priority of this task
	 * @param f the function to call when executing the task
	 *
	 * @return the future used to wait on the task and get the result
	 */
	template<typename Ret>
	std::future<Ret> async(int priority, std::function<Ret()> f){
		auto p = package(f);
		auto t = std::shared_ptr<priority_task>(new priority_task(p.first, priority));
		task_mutex.lock();
		tasks.emplace(t);
		task_mutex.unlock();
		return std::move(p.second);
	}
	
	/**
	 * Pushes a new task (that doesn't return a value and does take 
	 * arguments) into the queue.
	 *
	 * @param priority the priority of this task
	 * @param f the function to call when executing the task
	 * @param args the arguments to pass to the function
	 *
	 * @return the future used to wait on the task
	 */
	template<typename... Args>
	std::future<void> async(int priority, std::function<void(Args...)> f, Args... args){
		auto p = package(f, args...);
		auto t = std::shared_ptr<priority_task>(new priority_task(p.first, priority));
		task_mutex.lock();
		tasks.emplace(t);
		task_mutex.unlock();
		return std::move(p.second);
	}
	
	/**
	 * Pushes a new task (that doesn't return a value and doesn't that
	 * arguments) into the queue.
	 *
	 * @param priority the priority of this task
	 * @param f the function to call when executing the task
	 *
	 * @return the future used to wait on the task
	 */
	std::future<void> async(int priority, std::function<void()> f){
		auto p = package(f);
		auto t = std::shared_ptr<priority_task>(new priority_task(p.first, priority));
		task_mutex.lock();
		tasks.emplace(t);
		task_mutex.unlock();
		return std::move(p.second);
	}

	/// Called by tasks of this thread pool to yield.
	static void yield();

protected:
	virtual optional<std::shared_ptr<priority_task>> get_task() override;
	virtual void handle_task(std::shared_ptr<priority_task>) override;

private:
	std::priority_queue<std::shared_ptr<priority_task>> tasks;
};

#endif // THREAD_POOL_HPP
