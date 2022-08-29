thread_pool
===========

Simple thread pool using only standard library components. Also includes a class for a priority thread pool.

Requires concepts and C++20. Currently only GCC 10.0+ is sufficient. Use `-std=c++20 -fconcepts` to compile.

The priority thread pool is only supported on POSIX/-like systems. But it's still easy to use the normal pool on non-POSIX; just don't compile priority_thread_pool.cpp or include the header.

For just C++11, use `8bdfb9b`. `5ea01d0` was the latest to support <= C++14. For C++17, use `e3be25` and compile with `-std=c++17 -fconcepts`.

The priority pool has the same API as described below, accept it has an int parameter first for the priority of the task. E.g. `pool.async(5, func, arg1, arg2)` for priority 5.

An example that computes the primality of 2 to 10,000 using 8 threads:
```c++
#include "thread_pool.hpp"

#include <iostream>
#include <list>
#include <utility>

using namespace std;

// Return the integer argument and a boolean representing its primality.
// We need to return the integer because the loop that retrieves results doesn't know which integer corresponds to which future.
pair<int, bool> is_prime(int n){
  for(int i = 2;i < n;i++)
    if(n % i == 0)
      return make_pair(i, false);
  return make_pair(n, true);
}

int main(){
  thread_pool pool(8);                   // Contruct a thread pool with 8 threads.
  list<future<pair<int, bool>>> results;
  for(int i = 2;i < 10000;i++){
  	// Add a task to the queue.
    results.push_back(pool.async(is_prime, i));
  }
  
  for(auto i = results.begin();i != results.end();i++){
    pair<int, bool> result = i->get();  // Get the pair<int, bool> from the future<...>
    cout << result.first << ": " << (result.second ? "is prime" : "is composite") << endl;
  }
  return 0;
}
```

`thread_pool::async` is a templated method that accepts any `std::function` and arguments to pass to the function. It returns a `std::future<Ret>` where `Ret` is the return type of the aforementioned `std::function`.

To submit a task: `future<Ret> fut(pool.async(func, args...));`.

To wait on `fut` to complete: `Ret result = fut.get();`
