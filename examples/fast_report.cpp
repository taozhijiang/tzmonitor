#include <string>
#include <sstream>
#include <iostream>
#include <syslog.h>


#include <Client/include/MonitorClient.h>

using namespace tzmonitor_client;

volatile bool start = false;
volatile bool stop  = false;

time_t            start_time = 0;
volatile uint64_t count = 0;

extern char * program_invocation_short_name;
static void usage() {
    std::stringstream ss;

    ss << program_invocation_short_name << " [thread_num] " << std::endl;
    ss << std::endl;

    std::cerr << ss.str();
}

int random_int() {
    return (random() % 100 ) + 1;
}

void* perf_run(void* x_void_ptr) {
    
    std::string addr_ip = "127.0.0.1";
    uint16_t    addr_port = 8435;
    
    auto reporter = std::make_shared<MonitorClient>();
    if (!reporter || !reporter ->init(addr_ip, addr_port, ::syslog)) {
        std::cout << "init client failed." << std::endl;
        return NULL;
    }
    
    while(!start)
        ::usleep(1);

    while(!stop) {

        reporter->report_event("event1", random_int(), "tag_T");
        reporter->report_event("event1", random_int(), "tag_F");
        reporter->report_event("event2", random_int(), "tag_T");
        reporter->report_event("event2", random_int(), "tag_F");
        reporter->report_event("event3", random_int(), "tag_T");
        reporter->report_event("event3", random_int(), "tag_F");

        reporter->report_event("event1", random_int(), "tag_T");
        reporter->report_event("event1", random_int(), "tag_F");
        reporter->report_event("event2", random_int(), "tag_T");
        reporter->report_event("event2", random_int(), "tag_F");
        reporter->report_event("event3", random_int(), "tag_T");
        reporter->report_event("event3", random_int(), "tag_F");
        ::usleep(500);

        // increment success case
        count += 12;
    }
}

int main(int argc, char* argv[]) {


    int thread_num = 0;
    if (argc < 2 || (thread_num = ::atoi(argv[1])) <= 0) {
        usage();
        return 0;
    }
    

    
    
    std::vector<pthread_t> tids( thread_num,  0);
    for(size_t i=0; i<tids.size(); ++i) {
        pthread_create(&tids[i], NULL, perf_run, NULL);
        std::cerr << "starting thread with id: " << tids[i] << std::endl;
    }

    ::sleep(3);
    std::cerr << "begin to test, press any to stop." << std::endl;
    start_time = ::time(NULL);
    start = true;

    int ch = getchar();
    stop = true;
    time_t stop_time = ::time(NULL);

    uint64_t count_per_sec = count / ( stop_time - start_time);
    fprintf(stderr, "total count %ld, time: %ld, perf: %ld tps\n", count, stop_time - start_time, count_per_sec);

    for(size_t i=0; i<tids.size(); ++i) {
        pthread_join(tids[i], NULL);
        std::cerr<< "joining " << tids[i] << std::endl;
    }

    std::cerr << "done" << std::endl;

    return 0;
}

