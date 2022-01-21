#include <stdio.h>
#include <stdlib.h>
// #include <time.h>
// #include <sys/time.h>

#include "sim_api.h"

#define N 100
#define MAGIC 365117

u_int64_t rdtsc()
{
    u_int32_t lo, hi;

    __asm__ __volatile__(
        "rdtsc"
        : "=a"(lo), "=d"(hi));
    return (u_int64_t)hi << 32 | lo;
}

int main(void)
{
    // u_int64_t start,end,duration;
    // start = rdtsc();

    // struct timeval start, end;
    // gettimeofday(&start, NULL);

    SimRoiStart();
    SimNamedMarker(1, "begin");

    int sum = 0;
    // handle_frontend_init();
    // default option: i should be 10000
    for (int i = 0; i < 5; i++) {
        sum += i + 365117;
    }

    // printf("hello world! %d\n", sum);

    // Result: The output proves that the loop section is in the simulation mode.
    // if (SimInSimulator()) {
    //     printf("API Test: Running in the simulator\n");
    //     fflush(stdout);
    // } else {
    //     printf("API Test: Not running in the simulator\n");
    //     fflush(stdout);
    // }

    SimNamedMarker(2, "end");
    SimRoiEnd();

    // gettimeofday(&end, NULL);
    // long seconds = (end.tv_sec - start.tv_sec);
    // printf("The elapsed time is %d microseconds", (((seconds * 1000000) + end.tv_usec) - (start.tv_usec)));

    // end = rdtsc();
    // duration = end - start;
    // printf("Execution time：%lld cpu clock ticks.\n", duration);
    // printf("Execution time：%lf seconds.\n", duration/2.3/1000000000.0);

    return 0;
}
