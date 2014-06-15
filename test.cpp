#include "thread_pool.hpp"

#include <vector>

#include <iostream>

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

using namespace std;

static void core_dump(int sigid)
{
    kill(getpid(), SIGSEGV);
}

const int num_threads = 6;

void func_void_void(){
	cout << "void" << endl;
}

void test_void_void(){
	thread_pool p(num_threads);
	vector<future<void>> f;
	for(int i = 10000000;i < 20000000;i++)
		f.emplace_back(p.async(function<void()>(func_void_void)));
	for(auto i = f.begin();i != f.end();i++)
		i->get();
}

void func_void_int(int n){
	bool prime = true;
	if(n < 2)
		prime = false;
	for(int i = 2;i < n;i++)
		if(n % i == 0)
			prime = false;
	cout << n << ": " << (prime ? "Prime" : "Composite") << endl;
}

void test_void_int(){
	thread_pool p(num_threads);
	vector<future<void>> f;
	for(int i = 10000000;i < 20000000;i++)
		f.emplace_back(p.async<int>(function<void(int)>(func_void_int), move(i)));
	for(auto i = f.begin();i != f.end();i++)
		i->get();
}

int func_int_void(){
	return 42;
}

void test_int_void(){
	thread_pool p(num_threads);
	vector<future<int>> f;
	for(int i = 100000;i < 1000000;i++)
		f.emplace_back(p.async(function<int()>(func_int_void)));
	for(auto i = f.begin();i != f.end();i++)
		cout << i->get() << endl;
}

bool func_bool_int(int n){
	if(n < 2)
		return false;
	for(int i = 2;i < n;i++)
		if(n % i == 0)
			return false;
	return true;
}

void test_bool_int(){
	thread_pool p(num_threads);
	vector<future<bool>> f;
	int n = 1000000;
	int max = 10 * n;
	for(int i = n;i < max;i++)
		f.emplace_back(p.async(function<bool(int)>(func_bool_int), i));
	for(auto i = f.begin();i != f.end();i++, n++)
		cout << n << ": " << i->get() << endl;
}

int main(){
	//signal(SIGINT, core_dump);
	test_bool_int();
	return 0;
}
