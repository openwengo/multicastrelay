#include "pid.hpp"

Pid::Pid(short val, std::string type_val, std::string desc_val, unsigned long long int pkts_per_pids_val,
 unsigned long long int continuity_error_per_pid_val, short last_continuity_counter_per_pid_val, unsigned long long int packet_per_pid_saved_value_val
 , bool pcr, int stream_id)
{
	this->pid = val;
	this->type = type_val;
	this->description = desc_val;
	this->pkts_per_pids = pkts_per_pids_val;
	this->continuity_error_per_pid = continuity_error_per_pid_val;
	this->last_continuity_counter_per_pid = last_continuity_counter_per_pid_val;
	this->packet_per_pid_saved_value = packet_per_pid_saved_value_val;
	this->contain_pcr = pcr;
	this->pes_stream_id = stream_id;
}

Pid::~Pid()
{
	
}

Pid_list::Pid_list()
{

}

Pid_list::~Pid_list()
{
	
}

std::list<Pid>::iterator	Pid_list::find_pid(short pid_val)
{
	std::list<Pid>::iterator	it = this->pid_list.begin();
	
	while (it != this->pid_list.end() && (*it).pid != pid_val)
		it++;
	return (it);
}

void	Pid_list::print_list()
{
	std::list<Pid>::iterator	it = this->pid_list.begin();
	
	std::cout << "******************************" << std::endl;
	std::cout << "PIDS list:" << std::endl;
	while (it != this->pid_list.end())
	{
		std::cout << std::endl << "pid " << (*it).pid << " = " << std::endl;
		std::cout << "type " << (*it).type << std::endl;
		std::cout << "description " << (*it).description << std::endl;
		std::cout << "pkts_per_pids " << (*it).pkts_per_pids << std::endl;
		std::cout << "continuity_error_per_pid " << (*it).continuity_error_per_pid << std::endl;
		std::cout << "last_continuity_counter_per_pid " << (*it).last_continuity_counter_per_pid << std::endl;
		std::cout << "packet_per_pid_saved_value " << (*it).packet_per_pid_saved_value << std::endl;
		std::cout << "contain_pcr " << (*it).contain_pcr << std::endl;
		std::cout << "pes_stream_id " << (*it).pes_stream_id << std::endl;
		it++;
	}
	std::cout << "******************************" << std::endl << std::endl;
}