#ifndef PRIORITY_THREAD_POOL_HPP
#define PRIORITY_THREAD_POOL_HPP

#include "thread_pool.hpp"

#include <ucontext.h>

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

class priority_thread_pool : public base_thread_pool<std::shared_ptr<priority_task>>{
public:
	priority_thread_pool(unsigned int n);
	virtual ~priority_thread_pool();

	/**
	 * Pushes a new task to the queue.
	 *
	 * @param f the function to call when executing the task
	 * @param args the arguments to pass to the function
	 *
	 * @return the future used to wait on the task and get the result
	 */
	template<typename Fn, typename... Args>
	auto async(int priority, Fn f, Args... args){
		auto p = package<Fn, decltype(f(args...)), Args...>(f, args...);
		return std::move(add_task(priority, std::move(p)));
	}

	/// Called by tasks of this thread pool to yield.
	static void yield();

protected:
	virtual optional<std::shared_ptr<priority_task>> get_task() override;
	virtual void handle_task(std::shared_ptr<priority_task>) override;

	auto add_task(int priority, auto p) {
		auto t = std::shared_ptr<priority_task>(new priority_task(p.first, priority));
		task_mutex.lock();
		tasks.emplace(t);
		task_mutex.unlock();
		return std::move(p.second);
	}

private:
	std::priority_queue<std::shared_ptr<priority_task>> tasks;
};

#endif
