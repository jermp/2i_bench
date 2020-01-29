#pragma once

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <thread>

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS
#include <boost/config.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/experimental/parallel/v2/task_region.hpp>

namespace pvb {

typedef boost::executors::basic_thread_pool executor_type;
typedef boost::experimental::parallel::v2::task_region_handle_gen<executor_type>
    task_region_handle;
using boost::experimental::parallel::v2::task_region;

class configuration {
public:
    configuration() {
        fillvar("DS2I_K", k, 10);
        fillvar("DS2I_EPS1", eps1, 0.03);
        fillvar("DS2I_EPS2", eps2, 0.3);
        fillvar("DS2I_EPS3", eps3, 0.01);
        fillvar("DS2I_LOG_PART", log_partition_size, 7);
        fillvar("DS2I_THREADS", worker_threads,
                std::thread::hardware_concurrency());
        executor.reset(new executor_type(worker_threads));
    }

    double eps1;
    double eps2;
    double eps3;

    uint64_t k;
    size_t log_partition_size;
    size_t worker_threads;

    std::unique_ptr<executor_type> executor;

private:
    template <typename T, typename T2>
    void fillvar(const char* envvar, T& var, T2 def) {
        const char* val = std::getenv(envvar);
        if (!val || !strlen(val)) {
            var = def;
        } else {
            var = boost::lexical_cast<T>(val);
        }
    }
};

}  // namespace pvb
