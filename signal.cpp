#include <vector>
#include "pid.hpp"
#include "packets_general_info.hpp"

void	write_on_file_flux_diffuse(std::atomic<bool> &is_main_process_mandatory);

Packet_info					packet_second;
Packet_info					packet_main;
std::vector<Pid>			pid_vector_main(NBR_PID_MAX); // pid 0 to 8191 = vector of size 8192
std::vector<Pid>			pid_vector_second(NBR_PID_MAX);
extern std::atomic<bool>	ask_for_find_the_gop;
extern std::atomic<bool>	ask_force_switch;

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
}

/*void	timeout_signal_handler(int sign)
{
	std::cout << "Aucun changement d'état détecter x sec " << std::endl;
	ask_for_find_the_gop = true; 
}*/

void	force_switch_signal_handler(int sign)
{
	packet_main.is_process_mandatory = !packet_main.is_process_mandatory;
	packet_second.is_process_mandatory = !packet_second.is_process_mandatory;
	write_on_file_flux_diffuse(packet_main.is_process_mandatory);
	if (packet_main.is_process_mandatory == true)
		set_correction_switch(pid_vector_main);
	else
		set_correction_switch(pid_vector_second);
	ask_force_switch = false;
}