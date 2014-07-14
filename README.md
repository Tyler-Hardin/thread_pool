thread_pool
===========

Thread pool using std::* primitives from C++11. The entire library is only one class (`thread_pool`) with one method (`async`) for submitting tasks.


An example that computes the primality of 2 to 10,000:
```
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
    results.push_back(pool.async(std::function<pair<int, bool>(int)>(is_prime), i));
  }
  
  for(auto i = results.begin();i != results.end();i++){
    pair<int, bool> result = i->get();  // Get the pair<int, bool> from the future<...>
    cout << result.first << ": " << (result.second ? "is prime" : "is composite") << endl;
  }
  return 0;
}
```

`thread_pool::async` is a templated method that accepts any `std::function` and arguments to pass to the function. It returns a `std::future<Ret>` where `Ret` is the return type of the aforementioned `std::function`.

To submit a task: `future<Ret> fut(pool.async(std::function<Ret(Args...)>, args...));`.

To wait on `fut` to complete: `Ret result = fut.get();`
