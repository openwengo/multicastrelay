#include <string>
#include <iostream>
#include <list>
#include <atomic>

class Pid
{
	public:
	Pid(short val, std::string type_val, std::string desc_val, unsigned long long int pkts_per_pids_val,
	unsigned long long int continuity_error_per_pid_val, short last_continuity_counter_per_pid_val, unsigned long long int packet_per_pid_saved_value_val
	, bool pcr, int stream_id)
	: pid(val), type(type_val), description(desc_val), pkts_per_pids(pkts_per_pids_val), continuity_error_per_pid(continuity_error_per_pid_val),
		last_continuity_counter_per_pid(last_continuity_counter_per_pid_val), packet_per_pid_saved_value(packet_per_pid_saved_value_val),
		contain_pcr(pcr), pes_stream_id(stream_id) {};
	~Pid(){};
	
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