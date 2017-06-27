[![Build Status](https://travis-ci.org/greg7mdp/sho.svg?branch=master)](https://travis-ci.org/greg7mdp/sho)

# SHO: The Small Hash Optimization

When you need a key to value map with the fastest lookup, it is hard to beat a good hash map. Hash maps are so efficient that they are sometimes used as essential parts of other data structures, causing many instances of a hash map to exist in memory.

I decided to implement this class after working with Michael, a user of my [sparsepp](https://github.com/greg7mdp/sparsepp) hash map. His application builds a trie with close to one hundred million nodes, and each node keeps track of its children using a hash map. He originally used [std::unordered_map](http://www.cplusplus.com/reference/unordered_map/unordered_map/), but then switched to using [sparsepp](https://github.com/greg7mdp/sparsepp) in order to reduce the memory usage of his application.

Michael was pretty happy with the reduced memory usage of sparsepp (memory usage went down from 49GB to 33GB, and execution time was reduced a little from 13 to 12 minutes. However we remained in contact and discussed whether using a custom allocator would help reducing memory usage even further.

When testing with the new optional [allocator](https://github.com/greg7mdp/sparsepp/blob/master/sparsepp/spp_dlalloc.h) from sparsepp based on Doug Lea [malloc](http://g.oswego.edu/dl/html/malloc.html), Michael reported that memory usage was *significantly increased*, not what we had hoped for!

What happened was that Michael's application creates close to one hundred million hash maps. While some of them are very large, most have zero or very few entries. The new allocator, which reserve a private memory space for each hash map, was very ill suited for the task and increased greatly the memory usage of all those (almost) empty hash maps.

So we decided to try a small hash optimization, which would use a small array of `<key, value>` pairs for small hash maps, and would expand to a full blown hash map (dynamically allocated) when the small array overflowed. This is similar to LLVM's [Small Vector Class](http://llvm.org/docs/doxygen/html/classllvm_1_1SmallVector.html), which uses a local array to avoid memory allocations (and fragmentation) for small vectors.

This small hash optimization was a success even beyond our expectations for Michael's project, reducing the memory usage further from 33GB to 17GB, and the execution time from 12 to 11 minutes.

## Installation

No compilation is needed, as this is a header-only library. The installation consist in copying the sho.h header wherever it will be convenient to include in your project(s). 

## Example

Suppose you are using a std::unordered_map through a typedef such as: 


```c++
typedef std::unordered_map<uint32_t, void *> MyMap;
```

Adding the Small Hash Optimization is as easy as including sho.h and updating the typedef as follows:

```c++
#include <sho.h>

typedef sho::smo<3, std::unordered_map, uint32_t, void *> MyMap;
```

The number 3 passed as the first template parameter means that the local array cache will be dimensioned to contain a maximum of 3 entries (you may want to test a few numbers to see which one offers the best size and speed improvement for your use case.

The second template parameter is the hash map used when the array overflows. I have tested the code with both [std::unordered_map](http://www.cplusplus.com/reference/unordered_map/unordered_map/) and [sparsepp](https://github.com/greg7mdp/sparsepp)

## API support

I implemented sho.h over a week-end, and it does not yet support the full std::unordered_map interface. Feel free to enter issues for extra APIs, or even better send me pull requests.

## TODO

* Properly store the comparison functor and the allocator in sho, with minimal memory cost
* Support maps parametrized with types without default constructors
* support full std::unordered_map API
* support std::unordered_set




