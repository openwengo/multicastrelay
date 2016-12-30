#include <string>
#include <iostream>
#include <list>

class Pid
{
	public:
	Pid(short, std::string, std::string, unsigned long long int, unsigned long long int, short, unsigned long long int, bool, int);
	~Pid();
	
	short					pid;
	std::string				type;
	std::string				description;
	unsigned long long int	pkts_per_pids;
	unsigned long long int	continuity_error_per_pid;
	short					last_continuity_counter_per_pid;
	unsigned long long int	packet_per_pid_saved_value;
	bool					contain_pcr;
	int						pes_stream_id;
};

class Pid_list
{
	public:
	Pid_list();
	~Pid_list();
	std::list<Pid>::iterator	find_pid(short);
	void						print_list();
	
	std::list<Pid>				pid_list;
	
};