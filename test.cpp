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

const int num_threads = 8;

void func_void_void(){
	cout << "void" << endl;
}

void test_void_void(){
	thread_pool p(num_threads);
	vector<future<void>> f;
	for(int i = 10000000;i < 20000000;i++)
		f.emplace_back(p.async(func_void_void));
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
		f.emplace_back(p.async(func_void_int, i));
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
		f.emplace_back(p.async<decltype(func_int_void), int>(func_int_void));
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
	int n = 100000;
	int max = 10 * n;
	for(int i = n;i < max;i++)
		f.emplace_back(p.async<decltype(func_bool_int), bool, int>(func_bool_int, i));
	for(auto i = f.begin();i != f.end();i++, n++)
		cout << n << ": " << i->get() << endl;
}

int func_int_int_int(int a, int b) {
	return a * b;
}

void test_void_int_int() {
	thread_pool p(num_threads);
	vector<future<int>> f;
	for(int i = 1;i < 100;i++) {
		for(int j = 1;j < 100;j++) {
		f.emplace_back(p.async<decltype(func_int_int_int), int, int, int>(
			func_int_int_int, i, j));
		}
	}
	for(auto i = f.begin();i != f.end();i++)
		i->get();
}

int main(){
	//signal(SIGINT, core_dump);
	test_bool_int();
	return 0;
}
