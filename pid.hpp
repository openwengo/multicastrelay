#include <string>
#include <iostream>
#include <list>
#include <atomic>

struct Pid
{
	Pid(): exist(false), pid(0), type(""), description(""), pkts_per_pids(0), continuity_error_per_pid(0),
		last_continuity_counter_per_pid(99), packet_per_pid_saved_value(0),
		contain_pcr(false), pes_stream_id(0) {};
	~Pid(){};
	std::atomic<bool>					exist;
	std::atomic<short>					pid;
	std::string							type;
	std::string							description;
	std::atomic<unsigned long long int>	pkts_per_pids;
	std::atomic<unsigned long long int>	continuity_error_per_pid;
	short								last_continuity_counter_per_pid;
	std::atomic<unsigned long long int>	packet_per_pid_saved_value;
	bool								contain_pcr;
	int									pes_stream_id;
};