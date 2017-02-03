#include <string>
#include <iostream>
#include <list>
#include <atomic>

typedef enum
{
	Psi,
	Dvb,
	Pes,
	Nul,
}PidType;

struct Pid
{
	Pid(): exist(false), pid(0), type(Nul), description(4), pkts_per_pids(0), continuity_error_per_pid(0),
		last_continuity_counter_per_pid(99), packet_per_pid_saved_value(0),
		contain_pcr(false), stream_type(-1) {};
	~Pid(){};
	std::atomic<bool>					exist;
	std::atomic<unsigned short>			pid;
	std::atomic<PidType>				type;
	std::atomic<unsigned short>			description;
	std::atomic<unsigned long long int>	pkts_per_pids;
	std::atomic<unsigned long long int>	continuity_error_per_pid;
	unsigned short						last_continuity_counter_per_pid;
	std::atomic<unsigned long long int>	packet_per_pid_saved_value;
	std::atomic<bool>					contain_pcr;
	std::atomic<short>					stream_type;
};