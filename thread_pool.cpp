#include "thread_pool.hpp"

#include <map>

#include <cassert>
#include <unistd.h>

using namespace std;

// Size of stacks used by task executor contexts.
constexpr std::size_t STACK_SIZE = 1024 * 8;

static map<std::thread::id, std::shared_ptr<priority_task>> cur_tasks;
static mutex cur_tasks_mutex;

priority_task::~priority_task() {
	assert(done);
	free(work_stack);

	// Fail fast.
	work_stack = nullptr;
	work_context.uc_stack.ss_sp = nullptr;
}

bool priority_task::operator<(const priority_task& t) const {
	return priority < t.priority;
}

/**
 * Actually runs the task, in a forked context.
 */
void priority_task::_run(void) {
	shared_ptr<priority_task> t;
	{
		lock_guard<mutex> lk(cur_tasks_mutex);
		auto it = cur_tasks.find(this_thread::get_id());
		assert(it != cur_tasks.end());
		t = it->second;
	}
	t->work();
	t->done = true;
}

/**
 * Starts or resumes the forked context and returns whether it is finished.
 */
bool priority_task::run() {
	paused = false;

	// This is where we'll resume when yield is called.
	getcontext(&pause_context);
	if(!started) {
		// Create the context which will execute the function.
		getcontext(&work_context);
		work_stack = malloc(STACK_SIZE);
		work_context.uc_stack.ss_size = STACK_SIZE;
		work_context.uc_stack.ss_sp = work_stack;
		work_context.uc_stack.ss_flags = 0;
		work_context.uc_link = &pause_context;
		makecontext(&work_context, &_run, 0);

		started = true;
		setcontext(&work_context);
	}
	else {
		if(done) {
			return true;
		}
		else if(!paused) {
			setcontext(&work_context);
		}
		else {
			return false;
		}
	}
}

/**
 * Pauses the work context and resumes the context in ::run().
 */
void priority_task::pause() {
	paused = true;
	getcontext(&work_context);
	if(paused) {
		setcontext(&pause_context);
	}
}

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

priority_thread_pool::priority_thread_pool(unsigned int n) : base_thread_pool(n) {
	init_mutex.unlock();
}

priority_thread_pool::~priority_thread_pool() {
	wait();
}

/**
 * Yields the task the current thread is running.
 */
void priority_thread_pool::yield() {
	cur_tasks_mutex.lock();
	auto it = cur_tasks.find(std::this_thread::get_id());
	assert(it != cur_tasks.end());
	auto task = it->second;
	cur_tasks_mutex.unlock();
	task->pause();
}

optional<shared_ptr<priority_task>> priority_thread_pool::get_task() {
	optional<shared_ptr<priority_task>> ret;
	lock_guard<mutex> lk(task_mutex);
	if(!tasks.empty()) {
		ret = tasks.top();
		tasks.pop();
	}
	return ret;
}

void priority_thread_pool::handle_task(shared_ptr<priority_task> t) {
	auto id = this_thread::get_id();
	{
		lock_guard<mutex> lk(cur_tasks_mutex);
		assert(cur_tasks.emplace(id, t).second);
	}
	bool finished = t->run();
	{
		lock_guard<mutex> lk(cur_tasks_mutex);
		cur_tasks.erase(id);
	}
	if(!finished) {
		lock_guard<mutex> lk(task_mutex);
		tasks.emplace(t);
	}
}
