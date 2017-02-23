#include <iostream>
#include "packets_general_info.hpp"

Packet_info					packet_second;
Packet_info					packet_main;
extern std::atomic<bool>	ask_for_find_the_gop;
extern std::atomic<bool>	ask_force_switch;

void	callback_signal_handler(int sign)
{
	
	std::cout << "callback_signal_handler" << std::endl;
	ask_for_find_the_gop = true;
}

void	timeout_signal_handler(int sign)
{
	std::cout << "Aucun changement d'état détecter x sec " << std::endl;
	ask_for_find_the_gop = true; 
}

void	force_switch_signal_handler(int sign)
{
	packet_main.is_process_mandatory = !packet_main.is_process_mandatory;
	packet_second.is_process_mandatory = !packet_second.is_process_mandatory;
	ask_force_switch = false;
}