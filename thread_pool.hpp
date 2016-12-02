#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <functional>
#include <future>
#include <list>
#include <memory>
#include <queue>

#include <experimental/optional>

template<typename Fn, typename Ret, typename... Args>
concept bool Callable = requires(Fn f, Args... args) {
	{ f(args...) } -> Ret;
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
	template<typename Fn, typename Ret, typename... Args>
	requires Callable<Fn, Ret, Args...>
	std::pair<std::function<void()>,std::future<Ret>>
	package(Fn f, Args... args){
		std::promise<Ret> *p = new std::promise<Ret>;

		// Create a function to package as a task.
		auto task_wrapper = std::bind([p, f{std::move(f)}](Args... args){
            if constexpr (std::is_same<Ret,void>::value) {
			    f(std::move(args)...);
			    p->set_value();
            } else {
			    p->set_value(std::move(f(std::move(args)...)));
            }
		}, std::move(args)...);

		// Create a function to package as a future for the user to wait on.
		auto ret_wrapper = [p]() -> Ret{
            if constexpr (std::is_same<Ret,void>::value) {
                p->get_future().get();
                delete p;
            } else {
                auto temp = std::move(p->get_future().get());
                delete p;
                return std::move(temp);
            }
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
			auto f = std::bind(&base_thread_pool::thread_func, this);
			threads.push_back(std::move(std::thread(f)));
	 	}
	 	task_mutex.unlock();
	}

	/**
	 * Waits for threads to exit. Leaves thread pool in unusable state. Used by 
	 * destructors.
	 */
	void wait() {
		task_mutex.lock();
		join = true;
		task_mutex.unlock();
		while(threads.size() > 0) {
			auto &t = threads.back();
			t.join();
			threads.pop_back();
		}
	}

	/**
	 * Returns the next task, if there is one. None if there isn't.
	 */
	virtual std::optional<Task> get_task() = 0;

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
	 * Pushes a new task to the queue.
	 *
	 * @param f the function to call when executing the task
	 * @param args the arguments to pass to the function
	 *
	 * @return the future used to wait on the task and get the result
	 */
	template<typename Fn, typename... Args>
	auto async(Fn f, Args... args){
		auto p = package<Fn, decltype(f(args...)), Args...>(f, args...);
		return std::move(add_task(std::move(p)));
	}

protected:
	virtual std::optional<std::future<void>> get_task() override;
	virtual void handle_task(std::future<void>) override;

	auto add_task(auto p) {
		auto t = std::async(std::launch::deferred, p.first);
		task_mutex.lock();
		tasks.emplace(std::move(t));
		task_mutex.unlock();
		return std::move(p.second);
	}
private:
	std::queue<std::future<void>> tasks;
};

#endif
