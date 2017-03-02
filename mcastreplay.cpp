#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <iostream> 
#include <string>
#include <fstream>
#include <bitset>
#include <list>
#include <vector>
#include <algorithm>
#include <thread>
#include <stdexcept>
#include <mutex>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#include "pid.hpp"
#include "packets_general_info.hpp"

#define DEFAULT_FILE_CONFIG "mcastreplay.ini"
#define DEFAULT_SLEEP_DURATION 60

void	callback_signal_handler(int sign);
void	timeout_signal_handler(int sign);
void	force_switch_signal_handler(int sign);

static std::map<unsigned short, unsigned short>	dvb_description = { 
	{16,5}, {17,6}, {18,7}, {19,8}, {20,9}, {21,10},
	{22,11}, {23,12}, {24,12},
	{25,12}, {26,12}, {27,12},
	{28,13}, {29,14}, {30,15}, {31,16}
};

static std::map<unsigned short, std::string>	stream_type_possibility = {
	{0,"Reserved"}, {1,"ISO/IEC 11172-2 (MPEG-1 video) in a packetized stream"}, {2,"ITU-T Rec. H.262 and ISO/IEC 13818-2 (MPEG-2 higher rate interlaced video) in a packetized stream"},
	{3,"ISO/IEC 11172-3 (MPEG-1 audio) in a packetized stream"},{4,"ISO/IEC 13818-3 (MPEG-2 halved sample rate audio) in a packetized stream"},
	{5,"ITU-T Rec. H.222 and ISO/IEC 13818-1 (MPEG-2 tabled data) privately defined"},{6,"ITU-T Rec. H.222 and ISO/IEC 13818-1 (MPEG-2 packetized data) privately defined (i.e., DVB subtitles/VBI and AC-3)"},
	{7,"ISO/IEC 13522 (MHEG) in a packetized stream"},{8,"ITU-T Rec. H.222 and ISO/IEC 13818-1 DSM CC in a packetized stream"},
	{9,"ITU-T Rec. H.222 and ISO/IEC 13818-1/11172-1 auxiliary data in a packetized stream"},
	{10,"ISO/IEC 13818-6 DSM CC multiprotocol encapsulation"},{11,"ISO/IEC 13818-6 DSM CC U-N messages"},{12,"ISO/IEC 13818-6 DSM CC stream descriptors"},
	{13,"ISO/IEC 13818-6 DSM CC tabled data"},{14,"ISO/IEC 13818-1 auxiliary data in a packetized stream"},
	{15,"ISO/IEC 13818-7 ADTS AAC (MPEG-2 lower bit-rate audio) in a packetized stream"},
	{16,"ISO/IEC 14496-2 (MPEG-4 H.263 based video) in a packetized stream"},
	{17,"ISO/IEC 14496-3 (MPEG-4 LOAS multi-format framed audio) in a packetized stream"},{18,"ISO/IEC 14496-1 (MPEG-4 FlexMux) in a packetized stream"},
	{19,"ISO/IEC 14496-1 (MPEG-4 FlexMux) in ISO/IEC 14496 tables"},{20,"ISO/IEC 13818-6 DSM CC synchronized download protocol"},
	{21,"Packetized metadata"},{22,"Sectioned metadata"},{23,"ISO/IEC 13818-6 DSM CC Data Carousel metadata"},
	{24,"ISO/IEC 13818-6 DSM CC Object Carousel metadata"},{25,"ISO/IEC 13818-6 Synchronized Download Protocol metadata"},{26,"ISO/IEC 13818-11 IPMP"},
	{27,"ITU-T Rec. H.264 and ISO/IEC 14496-10 (lower bit-rate video) in a packetized stream"}
};

const static std::vector<std::string> Type = 
{
	"PSI", "DVB", "PES", "NUL"
};

const static std::vector<std::string> Description = 
{
	"Audio", "Video", "PAT", "PMT", "Null Packet", /*5*/"NIT", "SDT", "EIT", "RST", "TDT", "network synchronization", "RNT", "reserved for future use", "inband signalling",
	"measurement", "DIT", "SIT"
};

std::atomic<bool>	ask_for_find_the_gop(false);
std::atomic<bool>	ask_force_switch(false);
std::string 		ingroup_main;
int  				inport_main;
std::string 		inip_main;
std::string 		ingroup_second;
int  				inport_second;
std::string 		inip_second;
extern Packet_info	packet_second;
extern Packet_info	packet_main;
bool				debug;

template<std::size_t N>
bool operator<(const std::bitset<N>& x, const std::bitset<N>& y)
{
    for (int i = N-1; i >= 0; i--) {
        if (x[i] ^ y[i]) return y[i];
    }
    return false;
}

template<std::size_t N>
bool operator>(const std::bitset<N>& y, const std::bitset<N>& x)
{
    for (int i = N-1; i >= 0; i--) {
        if (x[i] ^ y[i]) return y[i];
    }
    return false;
}

static struct addrinfo* udp_resolve_host( const char *hostname, int port, int type, int family, int flags )
{
    struct		addrinfo hints, *res = 0;
    int 		error;
    char 		sport[16];
    const char	*node = 0, *service = "0";

    if( port > 0 )
    {   
        snprintf( sport, sizeof(sport), "%d", port );
        service = sport;
    }
    if( (hostname) && (hostname[0] != '\0') && (hostname[0] != '?') )
        node = hostname;

    memset( &hints, 0, sizeof(hints) );
    hints.ai_socktype = type;
    hints.ai_family   = family;
    hints.ai_flags    = flags;
    if( (error = getaddrinfo( node, service, &hints, &res )) )
    {   
        res = NULL;
		std::cerr << "[udp] error: " << gai_strerror( error ) << std::endl;
    }

    return res;
}

void	print(const std::string &inip, const int &inport, const int &interval, std::vector<Pid> &pid_vector, Packet_info &packet)
{
	std::ofstream									fd_packets;
	std::ofstream									fd_octets;
	std::ofstream									fd_debit;
	std::ofstream									fd_interval;
	std::ofstream									fd_interval_pcr;
	std::ofstream									fd_main_pid_info;
	std::ofstream									fd;
	static unsigned long long int					saved_value;
	int												debit;
	std::stringstream 								string_stream;
	bool											door = true;
	while(1)
	{
		boost::posix_time::ptime time = boost::posix_time::microsec_clock::local_time();
		sleep(interval);
		boost::posix_time::time_duration time_elapsed = boost::posix_time::microsec_clock::local_time() - time;
		
		if (packet.is_process_mandatory == true)
		{
			for (int cursor = 0 ; cursor != NBR_PID_MAX; cursor++)
			{
				if (pid_vector[cursor].exist == true)
				{
					// Ecriture des fichiers "pkts_in_ip_port du flux entrant_pidN" Pour tout N
					try
					{
	/*file the stream*/	string_stream << packet.atomic_dest_info_file.load() << "pkts_in_" << inip << "_" << std::to_string(inport) << "_" << std::to_string(pid_vector[cursor].pid) << ".txt";
						fd.open(string_stream.str(), std::ofstream::out | std::ofstream::trunc); //open file with name in stream
						if (fd.fail())
							std::cerr << string_stream.str() << " failed" << std::endl;
						string_stream.str(std::string()); // clear stream
						double packets_per_second = (pid_vector[cursor].pkts_per_pids - pid_vector[cursor].packet_per_pid_saved_value) / (double)time_elapsed.total_seconds();
						pid_vector[cursor].packet_per_pid_saved_value.store(pid_vector[cursor].pkts_per_pids, std::memory_order_relaxed);
						fd << packets_per_second << std::endl;
						fd.flush();
						fd.close();
					}
					catch(std::exception &e)
					{
						std::cerr << "Exception: " << e.what() << std::endl;
					}
					
					// Ecriture des fichiers "continuity_error_in_ip_port du flux entrant_pidN" Pour tout N et ecriture des info des PID du flux principal
					try
					{
						string_stream << packet.atomic_dest_info_file.load() << "continuity_error_in_" << inip << "_" << std::to_string(inport) << "_" << std::to_string(pid_vector[cursor].pid) << ".txt";
						fd.open(string_stream.str(), std::ofstream::out | std::ofstream::trunc);
						string_stream.str(std::string());
						std::cout << "PID: " << pid_vector[cursor].pid << " = " << pid_vector[cursor].continuity_error_per_pid << " continuity errors" << std::endl
						<< "type : " << Type[pid_vector[cursor].type] << " description : " << Description[pid_vector[cursor].description] << std::endl;
						fd << pid_vector[cursor].continuity_error_per_pid << std::endl;
						if (pid_vector[cursor].stream_type > -1)
							std::cout << "stream type : " << stream_type_possibility[pid_vector[cursor].stream_type] << std::endl;
						std::cout << std::endl;
						fd.flush();
						fd.close();
						
						if (door == true)
						{
							string_stream << "Main_Pid_infos.txt";
							fd_main_pid_info.open(string_stream.str(), std::ofstream::out | std::ofstream::trunc);
							door = false;
							string_stream.str(std::string());
						}
						fd_main_pid_info << "PID: " << pid_vector[cursor].pid << " = " << pid_vector[cursor].continuity_error_per_pid << " continuity errors" << std::endl
						<< "type : " << Type[pid_vector[cursor].type] << " description : " << Description[pid_vector[cursor].description] << std::endl;
						if (pid_vector[cursor].stream_type > -1)
							fd_main_pid_info << "stream type : " << stream_type_possibility[pid_vector[cursor].stream_type] << std::endl;
					}
					catch(std::exception &e)
					{
						std::cerr << "Exception: " << e.what() << std::endl;
					}
				}
			}
			// Ecriture des Fichiers Octects.txt Packets.txt Debit.txt
			debit = (int)((packet.octets_read - saved_value) * 8) / time_elapsed.total_seconds(); // get debit = bite per second
			std::cout << "packet.packets_read: " << packet.packets_read << std::endl;
			std::cout << "packet.octets_read: " << packet.octets_read << std::endl;
			std::cout << "debit: " << debit << std::endl;
			std::cout << "interval max between two packets: " << packet.max_interval_value_between_packets << " milliseconds" << std::endl;
			std::cout << "interval max between two packets with pcr: " << packet.max_interval_value_between_pcr << " milliseconds" << std::endl;
			try
			{
				string_stream << packet.atomic_dest_info_file.load() << "Packets.txt";
				fd_packets.open(string_stream.str() , std::ofstream::out | std::ofstream::trunc);
				if (fd_packets.fail())
					std::cerr << "Opening " << string_stream.str() << " failed" << std::endl;
				string_stream.str(std::string());
				
				string_stream << packet.atomic_dest_info_file.load() << "Octets.txt";
				fd_octets.open(string_stream.str(), std::ofstream::out | std::ofstream::trunc);
				if (fd_octets.fail())
					std::cerr << "Opening " << string_stream.str() << " failed" << std::endl;
				string_stream.str(std::string());
				
				string_stream << packet.atomic_dest_info_file.load() << "Debit.txt";
				fd_debit.open(string_stream.str(), std::ofstream::out | std::ofstream::trunc);
				if (fd_debit.fail())
					std::cerr << "Opening " << string_stream.str() << " failed" << std::endl;
				string_stream.str(std::string());
				
				string_stream << packet.atomic_dest_info_file.load() << "Millisecond_intervale_max_between_packets.txt";
				fd_interval.open(string_stream.str(), std::ofstream::out | std::ofstream::trunc);
				if (fd_interval.fail())
					std::cerr << "Opening " << string_stream.str() << " failed" << std::endl;
				string_stream.str(std::string());
				
				string_stream << packet.atomic_dest_info_file.load() << "Millisecond_intervale_max_between_packets_pcr.txt";
				fd_interval_pcr.open(string_stream.str(), std::ofstream::out | std::ofstream::trunc);
				if (fd_interval_pcr.fail())
					std::cerr << "Opening " << string_stream.str() << " failed" << std::endl;			
				string_stream.str(std::string());
				
				fd_packets << packet.packets_read << std::endl;
				fd_packets.flush();
				fd_octets << packet.octets_read << std::endl;
				fd_octets.flush();
				fd_debit << debit << std::endl;
				fd_debit.flush();
				fd_interval << packet.max_interval_value_between_packets << std::endl;
				fd_interval.flush();
				fd_interval_pcr << packet.max_interval_value_between_pcr << std::endl;
				fd_interval_pcr.flush();
				fd_packets.close();
				fd_octets.close();
				fd_debit.close();
				fd_interval.close();
				fd_interval_pcr.close();
				saved_value = packet.octets_read;
			}
			catch(std::exception &e)
			{
				std::cerr << "Exception: " << e.what() << std::endl;
			}
			packet.max_interval_value_between_packets = 0;
			packet.max_interval_value_between_pcr = 0;
			std::cout << ask_for_find_the_gop << std::endl;
			door = true;
		}
	}
}

int	init (const int &argc, char **argv, std::string &ingroup_main, int &inport_main, std::string &inip_main, std::string &outgroup_main, 
									int &outport_main, std::string &outip_main, int &ttl_main,
								 std::string &ingroup_second, int &inport_second, std::string &inip_second, std::string &outgroup_second, 
									int &outport_second, std::string &outip_second, int &ttl_second,
									std::string &dest_info_file_main, std::string &dest_info_file_second, int &interval_main, int &interval_second,
									int &main_switch_delay, int &backup_switch_delay)
{
	// Option from file info recuperation
	boost::program_options::options_description file_description("File Options");
	boost::program_options::variables_map file_boost_map;
	std::string config_file = DEFAULT_FILE_CONFIG;
//	std::string 			temp_dest_info_file;
	
	file_description.add_options()
	("InMain.Group", boost::program_options::value<std::string>(&ingroup_main)->default_value(""), 						"Group Main Entry")
	("InMain.Ip", boost::program_options::value<std::string>(&inip_main)->default_value(""), 							"Ip Main Entry")
	("InMain.Port", boost::program_options::value<int>(&inport_main)->default_value(0), 								"Port Main Entry")
	("OutMain.Group", boost::program_options::value<std::string>(&outgroup_main)->default_value(""), 					"Group Main Output")
	("OutMain.Ip", boost::program_options::value<std::string>(&outip_main)->default_value(""), 							"Ip Main Output")
	("OutMain.Port", boost::program_options::value<int>(&outport_main)->default_value(0), 								"Port Main Output")
	("OutMain.Ttl", boost::program_options::value<int>(&ttl_main)->default_value(0), 									"Ttl Main Output")
	("OutMain.StatsPath", boost::program_options::value<std::string>(&dest_info_file_main)->default_value(""), 			"Main Stats Path")
	("OutMain.Interval", boost::program_options::value<int>(&interval_main)->default_value(DEFAULT_SLEEP_DURATION),		"Main Interval between each print out/file in seconds")
	("InSecond.Group", boost::program_options::value<std::string>(&ingroup_second)->default_value(""), 					"Group Second Entry")
	("InSecond.Ip", boost::program_options::value<std::string>(&inip_second)->default_value(""), 						"Ip Second Entry")
	("InSecond.Port", boost::program_options::value<int>(&inport_second)->default_value(0), 							"Port Second Entry")
	("OutSecond.Group", boost::program_options::value<std::string>(&outgroup_second)->default_value(""), 				"Group Second Output")
	("OutSecond.Ip", boost::program_options::value<std::string>(&outip_second)->default_value(""), 						"Ip Second Output")
	("OutSecond.Port", boost::program_options::value<int>(&outport_second)->default_value(0),							"Port Second Output")
	("OutSecond.Ttl", boost::program_options::value<int>(&ttl_second)->default_value(0),								"Ttl Second Output")
	("OutSecond.StatsPath", boost::program_options::value<std::string>(&dest_info_file_second)->default_value(""), 		"Second Stats Path")
	("OutSecond.Interval", boost::program_options::value<int>(&interval_second)->default_value(DEFAULT_SLEEP_DURATION),	"Second Interval between each print out/file in seconds")
	("InMain.DelaySwitch", boost::program_options::value<int>(&main_switch_delay)->default_value(1),					"Delay for timeout switch, Normal to Backup")
	("InSecond.DelaySwitch", boost::program_options::value<int>(&backup_switch_delay)->default_value(1),				"Delay for timeout switch, Backup to Normal")
	("Debug.DebugFlag", boost::program_options::value<bool>(&debug)->default_value(false),								"Debug Flag");
	
	// Option from Command Line recuperation
	boost::program_options::options_description description("Command Line Options");
	boost::program_options::variables_map boost_map;
	
	description.add_options()
	("ingroup_main", boost::program_options::value<std::string>(&ingroup_main), 			"Main Group Entry")
	("inip_main", boost::program_options::value<std::string>(&inip_main), 					"Main Ip Entry")
	("inport_main", boost::program_options::value<int>(&inport_main), 						"Main Port Entry")
	("outgroup_main", boost::program_options::value<std::string>(&outgroup_main), 			"Main Group Output")
	("outip_main", boost::program_options::value<std::string>(&outip_main), 				"MainIp Output")
	("outport_main", boost::program_options::value<int>(&outport_main), 					"Main Port Output")
	("ttl_main", boost::program_options::value<int>(&ttl_main), 							"Main Ttl Output")
	("statspath_main", boost::program_options::value<std::string>(&dest_info_file_main),"Main Stats Path")
	("interval_main", boost::program_options::value<int>(&interval_main), 				"Main Interval between each print out/file in seconds")
	("ingroup_second", boost::program_options::value<std::string>(&ingroup_second), 		"Second Group Entry")
	("inip_second", boost::program_options::value<std::string>(&inip_second), 				"Second Ip Entry")
	("inport_second", boost::program_options::value<int>(&inport_second), 					"Second Port Entry")
	("outgroup_second", boost::program_options::value<std::string>(&outgroup_second), 		"Second Group Output")
	("outip_second", boost::program_options::value<std::string>(&outip_second), 			"Second Ip Output")
	("outport_second", boost::program_options::value<int>(&outport_second), 				"Second Port Output")
	("ttl_second", boost::program_options::value<int>(&ttl_second), 						"Second Ttl Output")
	("statspath_second", boost::program_options::value<std::string>(&dest_info_file_second),"Second Stats Path")
	("interval_second", boost::program_options::value<int>(&interval_second), 				"Second Interval between each print out/file in seconds")
	("config", boost::program_options::value<std::string>(&config_file), 					"Config File Name")
	("main-switch-delay", boost::program_options::value<int>(&main_switch_delay),			"Delay for timeout switch, Normal to Backup")
	("second-switch-delay", boost::program_options::value<int>(&backup_switch_delay),		"Delay for timeout switch, Backup to Normal")
	("debug", boost::program_options::value<bool>(&debug),									"Debug Flag")
	("help", "Help Screen");
	
	try
	{
		boost::program_options::store(boost::program_options::parse_command_line(argc, argv, description), boost_map);
		boost::program_options::notify(boost_map);
		std::ifstream file(config_file.c_str(), std::ifstream::in);
	
		if (file.fail())
		{
			std::cerr << "Opening " << config_file << " failed" << std::endl;
			return (1);
		}
		
		boost::program_options::store(boost::program_options::parse_config_file(file, file_description), file_boost_map);
		file.close();
		boost::program_options::notify(file_boost_map);
		boost::program_options::store(boost::program_options::parse_command_line(argc, argv, description), boost_map);
		boost::program_options::notify(boost_map);
	}
	catch (const boost::program_options::error &e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
		return (1);
	}
	
	if (ingroup_main.empty() == true || inip_main.empty() == true || inport_main == 0 || outgroup_main.empty() == true || 
		outip_main.empty() == true || outport_main == 0 || ttl_main == 0 || 
		ingroup_second.empty() == true || inip_second.empty() == true || inport_second == 0 || outgroup_second.empty() == true || 
		outip_second.empty() == true || outport_second == 0 || ttl_second == 0)
	{
		std::cerr << "Group, Ip, Port from In/Out and Ttl Out needed to run it" << std::endl;
		std::cerr << description << std::endl << file_description << std::endl;
		return (1);
	}
	else if (boost_map.size() == 1 && boost_map.count("help"))
	{
		std::cerr << description << std::endl << file_description << std::endl;
		return (1);
	}
	if ( dest_info_file_main != "" ) {
	   try {
	      if (!boost::filesystem::is_directory(dest_info_file_main)) {
	         boost::filesystem::create_directories(dest_info_file_main);
	      }
	   } catch (const boost::filesystem::filesystem_error& e) {
	         std::cerr << "Failed to check or create directory:" << dest_info_file_main << " with error:" << e.what() << std::endl;
	         return(1) ;
	   }
	}
	if ( dest_info_file_second != "" ) {
	   try {
	      if (!boost::filesystem::is_directory(dest_info_file_second)) {
	         boost::filesystem::create_directories(dest_info_file_second);
	      }
	   } catch (const boost::filesystem::filesystem_error& e) {
	         std::cerr << "Failed to check or create directory:" << dest_info_file_second << " with error:" << e.what() << std::endl;
	         return(1) ;
	   }
	}
	return (0);
}

int	packet_size_guessing(const char (*databuf_in)[16384])
{
	int size_testing[1] = {188};
	
	for(int x = 0; x != 1; x++)
	{
		int index = 0;
		if ((*databuf_in)[size_testing[x] * index++] == 'G' && 
			(*databuf_in)[size_testing[x] * index++] == 'G' &&
			(*databuf_in)[size_testing[x] * index++] == 'G')
			return (size_testing[x]);
	}
	return (-1);
}

// 00000000 00000000 00000001 (1110**** for video or 110***** for audio) = PES start code
int	PES_analysis(const int &packets_size, int &x, char (*databuf_in)[16384], const short &PID, std::vector<Pid>	&pid_vector, Packet_info &packet)
{
	
	// IN test, arrange this function when finished
	
	static unsigned int s_video_packet_nbr;
	static unsigned int s_audio_packet_nbr;
	int					pos;
	std::bitset<8> start_interval_audio(191); // real start 192
	std::bitset<8> end_interval_audio(224); // real end 223
	std::bitset<8> start_interval_video(223); // real start 224
	std::bitset<8> end_interval_video(240); // real start 239
	
	for (pos = 0; pos != packets_size; pos++)
		{
				if ((*databuf_in)[(x * packets_size) + pos] == 0 && (*databuf_in)[(x * packets_size) + pos + 1] == 0 && (*databuf_in)[(x * packets_size) + pos + 2] == 1)
				{
					// prefix start code 00000000 00000000 00000001
					//std::cout << "PID: " << PID << " stream id " << std::bitset<8>(databuf_in[(x * packets_size) + pos + 3]) << std::endl;
					short pes_length = (((*databuf_in)[(x * packets_size) + pos + 4] << 8) | ((*databuf_in)[(x * packets_size) + pos + 5] & 0xff));
					//std::cout << "PES packet length " << pes_length << std::endl;
					short header_pes_len = (*databuf_in)[(x * packets_size) + pos + 8]& 0xff;
					//std::cout << "PES header length " << header_pes_len << std::endl;
					s_video_packet_nbr = 0;
					s_audio_packet_nbr = 0;
					std::bitset<8> stream_id((*databuf_in)[(x * packets_size) + pos + 3]);
					if (stream_id > start_interval_audio && stream_id < end_interval_audio)
					{
						pid_vector[PID].description = 0;
						++s_audio_packet_nbr;
					}
					else if (stream_id > start_interval_video && stream_id < end_interval_video)
					{
						pid_vector[PID].description = 1;
						++s_video_packet_nbr;
						if (packet.is_process_mandatory == false && ask_for_find_the_gop == true)
						{
							while (pos != packets_size)
							{
								if ((*databuf_in)[(x * packets_size) + pos] == 0 && (*databuf_in)[(x * packets_size) + pos + 1] == 0 && (*databuf_in)[(x * packets_size) + pos + 2] == 1)
								{	
							//std::cout << "NAL unit (after start code 00 00 01) " << std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 3]) << " " << std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 4]) << std::endl;
									pes_length = (((*databuf_in)[(x * packets_size) + pos + 4] << 8) | ((*databuf_in)[(x * packets_size) + pos + 5] & 0xff));
									header_pes_len = (*databuf_in)[(x * packets_size) + pos + 8]& 0xff;
									if (std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 3]) == std::bitset<8>(103) /*0x67 mpeg4*/
										|| std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 3]) == std::bitset<8>(184) /*0xb8 mpeg2*/)
									{
										int i =  x * packets_size;
										std::cout << std::endl;
										while (i != (x * packets_size) + packets_size)
										{
											std::cout << std::bitset<8>((*databuf_in)[i]) << " ";
											++i;
										}
										std::cout << std::endl;
										std::cout << "x: " << x << " pos: " << pos << " " << std::bitset<8>((*databuf_in)[(x * packets_size) + pos]) << " " << std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 1]) << " " << std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 2])
										<< " " << std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 3]) << " " << std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 4])
										<< " " << std::bitset<8>((*databuf_in)[(x * packets_size) + pos + 5]) << " pes_length: " << pes_length << " header_pes_len: " << header_pes_len ;
										std::cout << std::endl << std::endl;
										
										std::string From_I_Image;
										std::cout << "FROM_I_IMAGE" << std::endl;
										i =  x * packets_size;
										int index = 0;
										//std::cout << "PID: " << PID << std::endl;
										while (i != packet.packets_per_read * packets_size)
										{	
											From_I_Image.append(1,(*databuf_in)[i]);
											++i;
											++index;
										}
										std::cout << std::endl << std::endl << "size " << From_I_Image.size();
										
										std::cout << std::endl;
										std::cout << "*** Start read packets with I IMAGE ***" << std::endl;
										int len = From_I_Image.size();
										/* puting I frame and the rest at begining of databuf_in
										for (i = 0; i != len; i++)
										{
											(*databuf_in)[i] = From_I_Image[i];
											std::cout << std::bitset<8>((*databuf_in)[i]) << " ";
										}
										//packet.packets_per_read = packet.packets_per_read - x;
										//x = 0;
										packet.datalen_out = packets_size * (packet.packets_per_read - x);
										*/std::cout << std::endl << "*** End read packets with I IMAGE ***" << std::endl;
										std::cout << std::bitset<8>((*databuf_in)[5]) << " ";
										//(*databuf_in)[5] = (*databuf_in)[5] | 0x80; // discontinuity indicator set to 1 
										std::cout << std::bitset<8>((*databuf_in)[5]) << "\n";
										std::cout << "ALL PACKET :\n";
										len = 0;
										for(i=0; i!= packets_size * packet.packets_per_read;i++)
										{
											if (len == 188)
											{
												len = 0;
												std::cout << std::endl << std::endl;;
											}
											std::cout << std::bitset<8>((*databuf_in)[i]) << " ";
											++len;
										}
										std::cout << "\n\n";
										ask_for_find_the_gop = false;
										ask_force_switch = true;
										raise(SIGUSR2);
									}
								}
								++pos;
							}
							break;
						}
					}
				}
		}
	return (0);
}

void	PMT_analysis(const char (*databuf_in)[16384], const int &x, const int &packets_size, const short &section_length, std::vector<Pid> &pid_vector, Packet_info &packet)
{
	//octet 13 14 pcr pid
	short pcr_pid = (((*databuf_in)[(x * packets_size) + 13] & 0x1f) << 8) | ((*databuf_in)[(x * packets_size) + 14] & 0xff);
	//std::cout << "pcr_pid: " << pcr_pid << std::endl;
	short stream_type;
	short elementary_pid;
	short es_info_length_length;
	short descriptor_tag;
	short descriptor_length;
	short boucle_length;
	// octet 8 9 table id extension, "Informational only identifier. The PAT uses this for the transport stream identifier and the PMT uses this for the Program number."
	short table_id_extension;
	
	// counter to iterate through ES info
	short es_counter;// = 2 + descriptor_length; 
	
	// skip this number of bytes (boucle_length) to get an other stream_type
	boucle_length = 0;
	
	while (17 + boucle_length < section_length)
	{
		table_id_extension = ((*databuf_in)[(x * packets_size) + 8] << 8) | ((*databuf_in)[(x * packets_size) + 9] & 0xff);
		// octet 17 = premier stream type boucle jusque fin de section
		stream_type = (*databuf_in)[(x * packets_size) + 17 + boucle_length] & 0xff;
		//octet 18-19 = premier elementary pid
		elementary_pid = (((*databuf_in)[(x * packets_size) + 18 + boucle_length] & 0x1f) << 8) | ((*databuf_in)[(x * packets_size) + 19 + boucle_length] & 0xff);
		//octet 20-21 = es_info_length_length 
		// es_info_length_length give the number of bytes (right after bytes 20-21) that are used for Descriptor enum (Descriptor section in Wikipedia)
		es_info_length_length = (((*databuf_in)[(x * packets_size) + 20 + boucle_length] & 0x3) << 8) | ((*databuf_in)[(x * packets_size) + 21 + boucle_length] & 0xff);
		// octet 22 = premier descriptor_tag
		descriptor_tag = (*databuf_in)[(x * packets_size) + 22 + boucle_length] & 0xff;
		// octet 23 = premiere definition du nombre d'octet qui suive l'octet 23 pour la description du descriptor
		descriptor_length = (*databuf_in)[(x * packets_size) + 23 + boucle_length] & 0xff;
		bool pcr = false;
		if (pid_vector[elementary_pid].exist == false)
			pid_vector[elementary_pid].exist = true;
		pid_vector[elementary_pid].pid = elementary_pid;
		if (pcr_pid == elementary_pid && pid_vector[elementary_pid].contain_pcr == false)
				pid_vector[elementary_pid].contain_pcr = true;
		pid_vector[elementary_pid].type = Pes;
		pid_vector[elementary_pid].stream_type = stream_type;
		es_counter = 2 + descriptor_length; // (2 = bytes used by descriptor_tag and descriptor_length) + bytes used for description of destructor_tag
		
		/*std::cout << "program number: " << table_id_extension << std::endl;
		std::cout << "descriptor_tag: " << descriptor_tag << " descriptor_length: " << descriptor_length;
		std::cout << " descriptor_info: ";
		for (int i = 1; i <= descriptor_length; i++)
		std::cout << " " << std::bitset<8>(databuf_in[(x * packets_size) + 23 + boucle_length + i]);
		std::cout << std::endl;
		*/
		while (es_counter < es_info_length_length)
		{
			descriptor_tag = (*databuf_in)[(x * packets_size) + 22 + boucle_length + es_counter];
			descriptor_length = (*databuf_in)[(x * packets_size) + 23 + boucle_length + es_counter];
				/*std::cout << "descriptor_tag: " << descriptor_tag << " descriptor_length: " << descriptor_length;
				std::cout << " descriptor_info: ";
				for (int i = 1; i <= descriptor_length; i++)
				std::cout << " " << std::bitset<8>(databuf_in[(x * packets_size) + 23 + boucle_length + es_counter + i]);
				std::cout << std::endl << std::endl;*/
			es_counter = es_counter + 2 + descriptor_length; // (2 = bytes used by descriptor_tag and descriptor_length) + bytes used for description of destructor_tag
		}
		boucle_length = boucle_length + es_info_length_length + 5;
	}
}

void	PAT_analysis(const char (*databuf_in)[16384], const int &x, const int &packets_size, const short &section_length, const short &PID, std::vector<Pid> &pid_vector, Packet_info &packet)
{
	short pat_repetition_size = 4;
	short repetition = 0;
	short program_num;
	short program_map_pid;
	
	if (pid_vector[PID].exist == false)
	{
		pid_vector[PID].exist = true;
		pid_vector[PID].pid = PID;
		pid_vector[PID].type = Psi;
		pid_vector[PID].description = 2;
	}
	// PMT PID / Program Map PID, at byte 15-16
	while (15 + (repetition * pat_repetition_size) < section_length + 6) // 6 = byte number for "section length" start (between 6-7) 
	{
		program_num = ((((*databuf_in)[(x * packets_size) + 13 + (repetition * pat_repetition_size)] & 0xff) << 8) | ((*databuf_in)[(x * packets_size) + 14 + (repetition * pat_repetition_size)] & 0xff));
		program_map_pid = (((*databuf_in)[(x * packets_size) + 15 + (repetition * pat_repetition_size)] & 0x1f) << 8) | 
			((*databuf_in)[(x * packets_size) + 16 + (repetition * pat_repetition_size)] & 0xff);
		if (program_num == 0)
			pid_vector[program_map_pid].description = dvb_description[program_map_pid];
		else
		{
			if (pid_vector[program_map_pid].exist == false)
			{
				pid_vector[program_map_pid].exist = true;
				pid_vector[program_map_pid].pid = program_map_pid;
				pid_vector[program_map_pid].type = Psi;
				pid_vector[program_map_pid].description = 3;
			}
			else
			{
				pid_vector[program_map_pid].type = Psi;
				pid_vector[program_map_pid].description = 3;
			}
		}
		++repetition;
	}
}

int	packet_monitoring(char (*databuf_in)[16384], boost::posix_time::ptime &last_time_pcr, std::vector<Pid> &pid_vector, Packet_info &packet)
{
	std::list<Pid>::iterator pid_it;
	int packets_size;
	if ((packets_size = packet_size_guessing(databuf_in)) == -1)
		return (1);
	packet.packets_per_read = packet.datalen_out / packets_size;
		
	packet.packets_read = packet.packets_read + packet.packets_per_read;
	packet.octets_read = packet.octets_read + packet.datalen_out;

	// découpage packets lu par chaque read, creation list de pid et un vector permetant de compter packet par pid	
	for (int x = 0; x != packet.packets_per_read; x++)
	{
		if ((*databuf_in)[(x * packets_size)] == 'G') // mpeg ts packet begins with octet 0 = G
		{
			short PID; // PID
			std::string type = "";
			std::string description = "";
			
			PID = (((*databuf_in)[(x * packets_size) + 1] << 8) | (*databuf_in)[(x * packets_size) + 2]) & 0x1fff;
			// octet 6-7 section length, "These bytes must not exceed a value of 1021"
			short section_length;// = (((*databuf_in)[(x * packets_size) + 6] & 0x3) << 8) | ((*databuf_in)[(x * packets_size) + 7] & 0xff);
			section_length= (((*databuf_in)[(x * packets_size) + 6] & 0x3) << 8) | ((*databuf_in)[(x * packets_size) + 7] & 0xff);

			// null packet
			if (PID == 8191 && pid_vector[PID].exist == false)
			{
				pid_vector[PID].pid = PID;
				pid_vector[PID].type = Nul;
				pid_vector[PID].description = 4;
				pid_vector[PID].exist = true;
			}
			// DVB
			else if (PID >= 16 && PID <= 31)
				{
					//check table id
					if (pid_vector[PID].exist == false)
					{
						pid_vector[PID].pid = PID;
						pid_vector[PID].type = Dvb;
						pid_vector[PID].exist = true;
						pid_vector[PID].description = dvb_description[PID];
					}
					else
					{
						pid_vector[PID].type = Dvb;
						pid_vector[PID].description = dvb_description[PID];
					}
				}
			//PSI
			else if (packet.is_process_mandatory == true && PID == 16)
				std::cout << "PID : " << PID <<  " description : " << Description[pid_vector[PID].description] <<std::endl;
			else if (PID == 0) // PAT
				PAT_analysis(databuf_in, x, packets_size, section_length, PID, pid_vector, packet);
			// PMT
			else if (pid_vector[PID].exist == true && pid_vector[PID].description == 3)
				PMT_analysis(databuf_in, x, packets_size, section_length, pid_vector, packet);
			// PES
			else if (pid_vector[PID].exist == true && pid_vector[PID].type == Pes)
				PES_analysis(packets_size, x, databuf_in, PID, pid_vector, packet);
			
			pid_vector[PID].pkts_per_pids = pid_vector[PID].pkts_per_pids + 1;
			
			int continuity = (*databuf_in)[(x * packets_size) + 3] & 0x0F;
		
			// Continuity Error Check
			if (PID != 8191 && pid_vector[PID].last_continuity_counter_per_pid != 99) // PID 8191 n'a pas de continuity counter, On evite la comparaison de l'initialisation 99
			{
				if (((*databuf_in)[(x * packets_size) + 3] & (1u << 5))) // **1* **** Adaptation Field Control = 10 or 11
				{
					if (((*databuf_in)[(x * packets_size) + 3] & (1u << 4))) // **11 **** Adaptation Field Control = 11
					{
						if (((*databuf_in)[(x * packets_size) + 5] & (1u << 7))) // **11 1*** Adaptation Field Control = 11 and Discontinuity Indicator = 1
						{
							if (pid_vector[PID].last_continuity_counter_per_pid != continuity) // verif =
								std::cout << "Adaptation field control = 11 and Discontinuity Indicator = 1" <<std::endl;
							}
						else // **11 0*** Adaptation Field Control = 11 and Discontinuity Indicator = 0
						{	
						
							//verif cc
							if (((pid_vector[PID].last_continuity_counter_per_pid + 1 != continuity) && pid_vector[PID].last_continuity_counter_per_pid != 15) || (pid_vector[PID].last_continuity_counter_per_pid == 15 && continuity != 0))
							{
								pid_vector[PID].continuity_error_per_pid = pid_vector[PID].continuity_error_per_pid + 1;
								std::cout << "Adaptation field control  = 11 and Discontinuity Indicator = 0"<<std::endl;
							}
						}
					}
					else // **10 **** Adaptation Field Control = 10
					{
						if (pid_vector[PID].last_continuity_counter_per_pid != continuity) // verif =
						{
							pid_vector[PID].continuity_error_per_pid = pid_vector[PID].continuity_error_per_pid + 1;
							std::cout << "Adaptation field control = 10 and continuity counter not equal to precedent continuity counter"<<std::endl;
						}
						if (((*databuf_in)[(x * packets_size) + 5] & (1u << 7))) // **10 1*** Adaptation Field Control = 10 and Discontinuity Indicator = 1
						{
						
						}
						else // **10 0*** Adaptation Field Control = 10 and Discontinuity Indicator = 0
						{
						
						}
					}
					
					// Check if its a packet with PCR field For interval calculation between 2 packets with pcr field
					if (pid_vector[PID].contain_pcr == true && (*databuf_in)[(x * packets_size) + 5] & (1u << 4)) // Checking if PCR flag = 1
					{
						boost::posix_time::ptime actual_time_pcr = boost::posix_time::microsec_clock::local_time();
						boost::posix_time::time_duration diff = actual_time_pcr - last_time_pcr;
						if (diff.total_milliseconds() > packet.max_interval_value_between_pcr)
							packet.max_interval_value_between_pcr = diff.total_milliseconds();
						last_time_pcr = actual_time_pcr;
					}
				}
				else // **0* **** Adaptation Field Control = 01 or 00				
				{
					if (((*databuf_in)[(x * packets_size) + 3] & (1u << 4))) // **01 **** Adaptation Field Control = 01 
					{
						// verif cc
						if (((pid_vector[PID].last_continuity_counter_per_pid + 1 != continuity) && pid_vector[PID].last_continuity_counter_per_pid != 15) || (pid_vector[PID].last_continuity_counter_per_pid == 15 && continuity != 0))
						{
							pid_vector[PID].continuity_error_per_pid = pid_vector[PID].continuity_error_per_pid + 1;
							std::cout << "Adaptation field control = 01 and continuity error detected " << std::endl;
						}
					}
					else // **00 **** Adaptation Field Control = 00
					{
						if (((*databuf_in)[(x * packets_size) + 5] & (1u << 7))) // **00 1*** Adaptation Field Control = 00 and Discontinuity Indicator = 1
						{
							
						}
						else // **00 0*** Adaptation Field Control = 00 and Discontinuity Indicator = 0
						{
							
						}
					}
				}
			}
			pid_vector[PID].last_continuity_counter_per_pid = continuity;
			
			if (pid_vector[PID].switch_correction == true)
			{
				std::cout << "SWITCH CORRECTION PID : " << PID << std::endl;
				(*databuf_in)[(x * packets_size) + 5] = (*databuf_in)[(x * packets_size) + 5] | 0x80;// discontinuity indicator set to 1 
				pid_vector[PID].switch_correction = false;
			}
		}
	}
	return (0);
}

int	flux_start(std::vector<Pid>	&pid_vector, Packet_info &packet, std::string &ingroup, int &inport, std::string &inip, std::string &outgroup, int &outport,
				std::string &outip, int &ttl, std::string &dest_info_file, int &interval, int &timeout_delay)
{
	struct 					in_addr localInterface;
    struct 					sockaddr_in groupSock;
    char 					databuf_out[16384] = "Multicast test message lol!";
	struct 					sockaddr_in localSock;
    struct 					ip_mreq group;
    int 					datalen_in;
    char 					databuf_in[16384];
	
	struct sockaddr_storage my_addr;
    struct addrinfo 		*res0 = 0;
    int 					addr_len;
	
	packet.atomic_dest_info_file.store((char*)dest_info_file.c_str(), std::memory_order_relaxed);
	
	res0 = udp_resolve_host( 0, outport, SOCK_DGRAM, AF_INET, AI_PASSIVE );
     if( res0 == 0 ) {
		 std::cerr << "udp_resolve_host failed" << std::endl;
		return(1);
     }
     memcpy( &my_addr, res0->ai_addr, res0->ai_addrlen );
     addr_len = res0->ai_addrlen;
     freeaddrinfo( res0 );


    /* Create a datagram socket on which to send. */
    packet.sd_out = socket(AF_INET, SOCK_DGRAM, 0);
    if(packet.sd_out < 0) {
      std::cerr << "Opening datagram socket error" << std::endl;
      return(1);
    } else {
      std::cout << "Opening the datagram socket...OK." << std::endl;
    }
	
	/* Initialize the group sockaddr structure with a */
    /* group address of 225.1.1.1 and port 5555. */
    memset((char *) &groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = inet_addr(outgroup.c_str());
    groupSock.sin_port = htons(outport);

    {
      int reuse = 1;
      if(setsockopt(packet.sd_out, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
       std::cerr << "Setting SO_REUSEADDR error on packet.sd_out" << std::endl;
       close(packet.sd_out);
       return(1);
      } 
    }
  /* bind  */
  if( bind( packet.sd_out.load(), (struct sockaddr *)&my_addr, addr_len ) < 0 ) {
      std::cerr << "Error binding out socket" << std::endl;
      return(1);
  }

    /* Disable loopback so you do not receive your own datagrams.
    {
    char loopch = 0;
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
      perror("Setting IP_MULTICAST_LOOP error");
      close(sd);
      exit(1);
    } else {
      printf("Disabling the loopback...OK.\n");
    }

    }

    */

     
    /* Set local interface for outbound multicast datagrams. */
    /* The IP address specified must be associated with a local, */
    /* multicast capable interface. */

    localInterface.s_addr = inet_addr(outip.c_str());
    if(setsockopt(packet.sd_out, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0) {
      std::cerr << "Setting local out interface error" << std::endl;
      return(1);
    } else {
      std::cout << "Setting the local out interface...OK\n" << std::endl;
    }
    
    if( setsockopt( packet.sd_out, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl) ) < 0 ) {   
      std::cerr << "Setting ttl" << std::endl;
      return(1);
    }

    /* Send a message to the multicast group specified by the*/
    /* groupSock sockaddr structure. */
    /*int datalen = 1024;*/


    /* Try the re-read from the socket if the loopback is not disable
    if(read(sd, databuf, datalen) < 0) {
      perror("Reading datagram message error\n");
      close(sd);
      exit(1);
    } else {
      printf("Reading datagram message from client...OK\n");
      printf("The message is: %s\n", databuf);
    }
    */

    /* Receiver/client multicast Datagram example. */

    /* Create a datagram socket on which to receive. */

	packet.sd_in = socket(AF_INET, SOCK_DGRAM, 0);
	struct timeval tv; 
	tv.tv_sec = timeout_delay; 
	tv.tv_usec = 0;
	if (setsockopt(packet.sd_in, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof tv))
		{ 
			perror("setsockopt"); 
			return -1; 
		}
    if(packet.sd_in < 0) {
      std::cerr << "Opening datagram socket error" << std::endl;
      return(1);
    } else {
      std::cout << "Opening datagram socket....OK." << std::endl;
      /* Enable SO_REUSEADDR to allow multiple instances of this */
      /* application to receive copies of the multicast datagrams. */
      {
      int reuse = 1;
      if(setsockopt(packet.sd_in, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
       std::cerr << "Setting SO_REUSEADDR error" << std::endl;
       close(packet.sd_in);
       return(1);
      } else {
       std::cout << "Setting SO_REUSEADDR...OK." << std::endl;
      }
      }
    }

    /* Bind to the proper port number with the IP address */
    /* specified as INADDR_ANY. */
    memset((char *) &localSock, 0, sizeof(localSock));
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(inport);
    //localSock.sin_addr.s_addr = INADDR_ANY; Dangereux! Pas de filtrage!
    localSock.sin_addr.s_addr = inet_addr(ingroup.c_str());
    if(bind(packet.sd_in.load(), (struct sockaddr*)&localSock, sizeof(localSock))) {
      std::cerr << "Binding datagram socket in error" << std::endl;
      close(packet.sd_in);
      return(1);
    } else {
      std::cout << "Binding datagram socket in...OK." << std::endl;
    }


    /* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
    /* interface. Note that this IP_ADD_MEMBERSHIP option must be */
    /* called for each local interface over which the multicast */
    /* datagrams are to be received. */
    group.imr_multiaddr.s_addr = inet_addr(ingroup.c_str());
    group.imr_interface.s_addr = inet_addr(inip.c_str());
    if(setsockopt(packet.sd_in, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
      std::cerr << "Adding multicast group in error" << std::endl;
      close(packet.sd_in);
      return(1);
    } else {
      std::cout << "Adding multicast group in...OK." << std::endl;
    }

    /* Read from the socket. */
    datalen_in = sizeof(databuf_in);
	
	std::thread t(print, std::ref(inip), std::ref(inport), std::ref(interval), std::ref(pid_vector), std::ref(packet));
	
	//First time getter
	boost::posix_time::ptime last_time = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::ptime last_time_pcr = last_time;
    
	while(1) {
      packet.datalen_out = read(packet.sd_in, databuf_in, datalen_in);
		// Calcule en milliseconds de l'intervale de reception de chaque packets
		boost::posix_time::ptime actual_time  = boost::posix_time::microsec_clock::local_time();
		boost::posix_time::time_duration diff = actual_time - last_time;
		if (diff.total_milliseconds() > packet.max_interval_value_between_packets)
			packet.max_interval_value_between_packets = diff.total_milliseconds();
		last_time = actual_time;
	  
	  if (packet.datalen_out == -1 && packet.is_process_mandatory == true)
		  raise(SIGALRM);
      /*if(packet.datalen_out < 0) {
        std::cerr << "Reading datagram im message error" << std::endl;
        close(packet.sd_in);
        close(packet.sd_out);
        return(1);
      } else {
        //printf("Reading datagram message in ...OK.\n");
		//std::cout << " **r: " << databuf_in << "  " <<strlen(databuf_in) << "**";
    //    printf("The message from multicast server in is: \"%s\"\n", databuf_in);
      }*/
	  if (packet_monitoring(&databuf_in, last_time_pcr, pid_vector, packet) == 0)
	  {
		if ((packet.is_process_mandatory == true && ask_force_switch == false && debug == false)
			|| (packet.is_process_mandatory == true && ask_force_switch == false && debug == true && ask_for_find_the_gop == false))
		{
			/*std::cout << "SEND : " << packet.is_process_mandatory << ask_force_switch << std::endl;
			std::cout << "DATALEN OUT : " << packet.datalen_out << std::endl;
			std::cout << "adaptation field control need maybe to be **11**** " << std::bitset<8>(databuf_in[3]) << "\n";
			int x = 0;
			int i = 0;// x * 188;
			//std::cout << "PID: " << PID << std::endl;
			while (i != 188)
			{
				std::cout << std::bitset<8>(databuf_in[i]) << " ";
				++i;
			}
			std::cout << "\n";*/
			if(sendto(packet.sd_out, databuf_in, packet.datalen_out, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0)
				std::cerr << "Sending datagram message out error" << std::endl;
		}
	  }
        //printf("Sending datagram message out...OK\n");
		//std::cout << " w ";
		/*for (int i = 0; i != strlen(databuf_in); i++)
			std::cout << std::bitset<8>(databuf_in[i]) << " ";*/
		//std::cout << " w: " << databuf_in << " size: " << strlen(databuf_in);
		/*int len = strlen(databuf_in);
		if (len > 3)
		{*/
      }
	return (0);
}

void	write_on_file_flux_diffuse(std::atomic<bool> &is_main_process_mandatory)
{
	std::ofstream	fd;
	
	fd.open("flux_diffuse.txt", std::ofstream::out | std::ofstream::trunc);
	fd << "Flux Diffuse:\n";
	if (is_main_process_mandatory == true)
		fd << "Group: " << ingroup_main << "\nIp: " << inip_main << "\nPort: " << inport_main << std::endl;
	else
		fd << "Group: " << ingroup_second << "\nIp: " << inip_second << "\nPort: " << inport_second << std::endl;
	fd.flush();
	fd.close();
}

int	main(int argc, char **argv)
{
	std::string			outgroup_main; 
    int					outport_main;
    std::string 		outip_main;
	int 				ttl_main = 0;
	std::string			dest_info_file_main;	
	int					interval_main;
	
	std::string			outgroup_second; 
    int					outport_second;
    std::string 		outip_second;
	int 				ttl_second = 0;
	std::string			dest_info_file_second;
	int					interval_second;
	int					main_switch_delay;
	int					backup_switch_delay;
	
	extern std::vector<Pid>	pid_vector_main; // pid 0 to 8191 = vector of size 8192
	extern std::vector<Pid>	pid_vector_second; // pid 0 to 8191 = vector of size 8192
	
	packet_main.is_process_mandatory.store(true, std::memory_order_relaxed);
	packet_second.is_process_mandatory.store(false, std::memory_order_relaxed);
	
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGUSR1);
	sigprocmask(SIG_BLOCK, &signal_set, NULL); // block l'ecoute SIGUSR1 pour le main thread, pour qu'ensuite les process fils n'ecoute pas USR1 non plus
	if (init(argc, argv, ingroup_main, inport_main, inip_main, outgroup_main, outport_main, outip_main, ttl_main,
						ingroup_second, inport_second, inip_second, outgroup_second, outport_second, outip_second, ttl_second,
						dest_info_file_main, dest_info_file_second, interval_main, interval_second, main_switch_delay, backup_switch_delay) == 1)
		return (1);
	
	std::thread primary(flux_start, std::ref(pid_vector_main), std::ref(packet_main), std::ref(ingroup_main), std::ref(inport_main), std::ref(inip_main), std::ref(outgroup_main), std::ref(outport_main), std::ref(outip_main), std::ref(ttl_main), std::ref(dest_info_file_main), std::ref(interval_main), std::ref(main_switch_delay));
	std::thread secondary(flux_start, std::ref(pid_vector_second), std::ref(packet_second), std::ref(ingroup_second), std::ref(inport_second), std::ref(inip_second), std::ref(outgroup_second), std::ref(outport_second), std::ref(outip_second), std::ref(ttl_second), std::ref(dest_info_file_second), std::ref(interval_second), std::ref(backup_switch_delay));
	
	sigprocmask(SIG_UNBLOCK, &signal_set, NULL); // laisse main thread ecouter SIGUSR1
	
	signal(SIGUSR1, callback_signal_handler);
	signal(SIGUSR2, force_switch_signal_handler);
	signal(SIGALRM, timeout_signal_handler);
	primary.join();
	secondary.join();
	return (0);
}