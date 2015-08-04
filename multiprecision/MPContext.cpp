#include "MPContext.hpp"
#include "fhe/NumbTh.h"
#include <set>
#include <NTL/ZZ.h>
#include <cmath>
#include <vector>
#include <thread>
#ifdef FHE_THREADS
const long WORKER_NR = 8;
#else
const long WORKER_NR = 1;
#endif

static long getSlots(long m, long p)
{
    return phi_N(m) / multOrd(p, m);
}

static std::set<long> FindPrimes(long m, long p, long parts)
{
    auto slots = getSlots(m, p);
    auto bits = static_cast<long>(std::ceil(std::log2(static_cast<double>(p))));
    std::set<long> primes{p};
    long generated = 1;
    long trial = 0;
    while (generated < parts) {

        auto prime = NTL::RandomPrime_long(bits);
        if (getSlots(m, prime) >= slots && (p % m) != 0) {
            auto ok = primes.insert(prime);
            if (ok.second) { 
                printf("prime %ld\n", prime);
                generated += 1;
            }
        }

        if (trial++ > 1000) {
            printf("Error: Can not find enough primes, only found %ld\n",
                   generated);
            break;
        }
    }

    return primes;
}

MPContext::MPContext(long m, long p, long r, long parts)
{
    contexts.reserve(parts);
    auto primesSet = FindPrimes(m, p, parts);

    for (auto &prime : primesSet) {
        m_plainSpace *= prime;
        m_primes.push_back(prime);
    }

    std::vector<std::thread> worker;
    std::atomic<size_t> counter(0);
    const size_t num = m_primes.size();
    auto job = [this, &counter, &m, &r, &num]() {
        size_t i;
        while ((i = counter.fetch_add(1)) < num) {
            contexts[i] = std::make_shared<FHEcontext>(m, m_primes[i], r);
        }
    };

    contexts.resize(num);
    
    for (long wr = 0; wr < WORKER_NR; wr++) worker.push_back(std::thread(job));

    for (auto &&wr : worker) wr.join();
}

void MPContext::buildModChain(long L)
{
    std::vector<std::thread> worker;
    std::atomic<size_t> counter(0);
    const size_t num = contexts.size();
    auto job = [this, &counter, &num, &L]() {
        size_t i;
        while ((i = counter.fetch_add(1)) < num) {
            ::buildModChain(*contexts[i], L);
        }
    };

    contexts.resize(num);
    
    for (long wr = 0; wr < WORKER_NR; wr++) worker.push_back(std::thread(job));

    for (auto &&wr : worker) wr.join();
}

double MPContext::precision() const
{
    return NTL::log(plainSpace()) / NTL::log(NTL::to_ZZ(2));
}
