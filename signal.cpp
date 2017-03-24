#include <vector>
#include <string>
#include <sstream>

// #include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
// #include <boost/log/expressions.hpp>
// #include <boost/log/utility/setup/file.hpp>

#include "pid.hpp"
#include "packets_general_info.hpp"

std::string	src1_or_src2(const Packet_info &p);
void		write_on_file_flux_diffuse(std::atomic<bool> &is_main_process_mandatory);

Packet_info					packet_second;
Packet_info					packet_main;
std::vector<Pid>			pid_vector_main(NBR_PID_MAX); // pid 0 to 8191 = vector of size 8192
std::vector<Pid>			pid_vector_second(NBR_PID_MAX);
extern std::atomic<bool>	ask_for_find_the_gop;
extern std::atomic<bool>	ask_force_switch;
extern const std::string	ingroup_main;
extern const std::string	ingroup_second;

void	set_correction_switch(std::vector<Pid> &pid_vector)
{
	for (int i = 0; i != NBR_PID_MAX; i++)
		if (pid_vector[i].exist == true)
			pid_vector[i].switch_correction = true;
}

void	callback_signal_handler(int sign)
{
	std::cout << "callback_signal_handler" << std::endl;
	ask_for_find_the_gop = true;
	std::stringstream ss;
	ss << "Asking for switch to ";
	ss << src1_or_src2(packet_second);
	BOOST_LOG_TRIVIAL(info) << "[ON AIR: " << src1_or_src2(packet_main) << "] # " << ss.str();
}

void	force_switch_signal_handler(int sign)
{
	std::stringstream ss;
	ss << "Switch from ";
	ss << src1_or_src2(packet_main);
	packet_main.is_process_mandatory = !packet_main.is_process_mandatory;
	packet_second.is_process_mandatory = !packet_second.is_process_mandatory;
	ss << " to ";
	ss << src1_or_src2(packet_main);
	BOOST_LOG_TRIVIAL(info) << "[ON AIR: " << src1_or_src2(packet_main) << "] # " << ss.str();
	
	write_on_file_flux_diffuse(packet_main.is_process_mandatory);
	ask_force_switch = false;
}