#include <atomic>

#define NBR_PID_MAX 8192

struct Packet_info
{
	Packet_info() : packets_read(0), octets_read(0), atomic_dest_info_file(0), max_interval_value_between_packets(0), max_interval_value_between_pcr(0), sd_in(0), sd_out(0),
	is_flux_absent(false){};
	~Packet_info(){};
	
	std::atomic<unsigned long long int>	packets_read;
	std::atomic<unsigned long long int>	octets_read;
	std::atomic<char*>					atomic_dest_info_file;
	std::atomic<unsigned long long int> max_interval_value_between_packets;
	std::atomic<unsigned long long int>	max_interval_value_between_pcr;
	std::atomic<int> 					sd_in;
	std::atomic<int> 					sd_out;
	std::atomic<bool>					is_flux_absent;
	std::atomic<int>					datalen_out;
	std::atomic<int>					packets_per_read;
	std::atomic<bool>					is_process_mandatory;
	int									multiplicateur_interval;
};