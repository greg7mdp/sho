// compile on windows with: CL -O2 -I.. -EHsc test_sho.cc -o test_sho
// ----------------------------------------------------------------------
#include <memory>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm> 
#include <vector>
#include <chrono>

#include <unordered_map>
#include <sho.h>

// -----------------------------------------------------------
//                     Timer
// -----------------------------------------------------------
namespace dltest
{
    template<typename time_unit = std::milli>
    class Timer 
    {
    public:
        Timer()                 { reset(); }
        void reset()            { _start = _snap = clock::now();  }
        void snap()             { _snap = clock::now();  }

        float get_total() const { return get_diff<float>(_start, clock::now()); }
        float get_delta() const { return get_diff<float>(_snap, clock::now());  }
        
    private:
        using clock = std::chrono::high_resolution_clock;
        using point = std::chrono::time_point<clock>;

        template<typename T>
        static T get_diff(const point& start, const point& end) 
        {
            using duration_t = std::chrono::duration<T, time_unit>;

            return std::chrono::duration_cast<duration_t>(end - start).count();
        }

        point _start;
        point _snap;
    };
}

// -----------------------------------------------------------
//                     Memory usage
// -----------------------------------------------------------
#if defined(_WIN32) || defined( __CYGWIN__)
    #define SHO_TEST_WIN
#endif

#ifdef SHO_TEST_WIN
    #include <windows.h>
    #include <Psapi.h>
    #undef min
    #undef max
#else
    #include <sys/types.h>
    #include <sys/sysinfo.h>
#endif

namespace dltest
{
    uint64_t GetSystemMemory()
    {
#ifdef SHO_TEST_WIN
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        return static_cast<uint64_t>(memInfo.ullTotalPageFile);
#else
        struct sysinfo memInfo;
        sysinfo (&memInfo);
        auto totalVirtualMem = memInfo.totalram;

        totalVirtualMem += memInfo.totalswap;
        totalVirtualMem *= memInfo.mem_unit;
        return static_cast<uint64_t>(totalVirtualMem);
#endif
    }

    uint64_t GetTotalMemoryUsed()
    {
#ifdef SHO_TEST_WIN
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        return static_cast<uint64_t>(memInfo.ullTotalPageFile - memInfo.ullAvailPageFile);
#else
        struct sysinfo memInfo;
        sysinfo(&memInfo);
        auto virtualMemUsed = memInfo.totalram - memInfo.freeram;

        virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
        virtualMemUsed *= memInfo.mem_unit;

        return static_cast<uint64_t>(virtualMemUsed);
#endif
    }

    uint64_t GetProcessMemoryUsed()
    {
#ifdef SHO_TEST_WIN
        PROCESS_MEMORY_COUNTERS_EX pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pmc), sizeof(pmc));
        return static_cast<uint64_t>(pmc.PrivateUsage);
#else
        auto parseLine = 
            [](char* line)->int
            {
                auto i = strlen(line);
				
                while(*line < '0' || *line > '9') 
                {
                    line++;
                }

                line[i-3] = '\0';
                i = atoi(line);
                return i;
            };

        auto file = fopen("/proc/self/status", "r");
        auto result = -1;
        char line[128];

        while(fgets(line, 128, file) != nullptr)
        {
            if(strncmp(line, "VmSize:", 7) == 0)
            {
                result = parseLine(line);
                break;
            }
        }

        fclose(file);
        return static_cast<uint64_t>(result) * 1024;
#endif
    }

    uint64_t GetPhysicalMemory()
    {
#ifdef SHO_TEST_WIN
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        return static_cast<uint64_t>(memInfo.ullTotalPhys);
#else
        struct sysinfo memInfo;
        sysinfo(&memInfo);

        auto totalPhysMem = memInfo.totalram;

        totalPhysMem *= memInfo.mem_unit;
        return static_cast<uint64_t>(totalPhysMem);
#endif
    }
}

// -----------------------------------------------------------
//                        test code
// -----------------------------------------------------------
using namespace std;

static float _to_mb(uint64_t m) { return (float)((double)m / (1024 * 1024)); }

// -----------------------------------------------------------
// -----------------------------------------------------------
template <class Hash, class K, class V>
void run_test(const char *container_name, size_t num_iter)
{
    // ----------------------- some checks
    {
        Hash h;

        typename Hash::iterator it(h.begin());
        typename Hash::const_iterator cit(h.cbegin());
        cit = it;                // assign const & non-const iters
        it  = cit;
        if (cit != h.end() || it != h.cend())      // compare const & non-const iters
            abort();
    }
    
    printf("---------------- testing %s\n", container_name);

    printf("\nmem usage before the test: %4.1f\n", _to_mb(dltest::GetProcessMemoryUsed()));
    srand(43); // always same sequence of random numbers

    Hash *hashes = new Hash[num_iter];
    
    dltest::Timer<std::milli> timer;

    for (size_t i=0; i<num_iter; ++i)
    {
        size_t num_insert = rand() % 4;
        if (i % 5000 == 0)
            num_insert = 10000;

        K inserted = 0, first = 0;

        for (uint32_t j=0; j<num_insert; ++j)
        {
            inserted = rand();
            if (j == 0)
                first = inserted;

            hashes[i].insert(std::make_pair((K)inserted, (V)0));
            if (j && j == num_insert-1 && inserted != first)
                hashes[i].erase(first);
        }

        if (inserted && hashes[i].find(inserted) == hashes[i].end())
            abort();
        
    }
    printf("mem usage after hashes created and filled: %4.1f\n",
           _to_mb(dltest::GetProcessMemoryUsed()));
    printf("       in %3.2f seconds\n", timer.get_total() / 1000);

    delete [] hashes;
    printf("mem usage after hashes deleted: %4.1f\n",
           _to_mb(dltest::GetProcessMemoryUsed()));

    printf("\n");
}

#define STRINGIFY(A)  #A
#define TOSTRING(x) STRINGIFY(x)

#if 0
    #include <sparsepp/spp.h>
    #define BASE_MAP spp::sparse_hash_map
#else
    #define BASE_MAP std::unordered_map
#endif
// -----------------------------------------------------------
// -----------------------------------------------------------
int main()
{
    typedef BASE_MAP<uint32_t, void *>          StdMap;
    typedef sho<3, BASE_MAP, uint32_t, void *>  ShoMap;

    run_test<StdMap, uint32_t, void *>(TOSTRING(BASE_MAP), 5000000);

    char cont_name[128];
    sprintf(cont_name, "%s with sho", TOSTRING(BASE_MAP));
    run_test<ShoMap, uint32_t, void *>(cont_name, 5000000);
}
