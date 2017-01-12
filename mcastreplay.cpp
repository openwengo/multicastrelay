#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream> 
#include <string>
#include <fstream>
#include <bitset>
#include <list>
#include <vector>
#include <algorithm>
#include <thread>
#include <stdexcept>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#include "packets_general_info.hpp"
#include "pid.hpp"

#define DEFAULT_FILE_CONFIG "mcastreplay.ini"
#define DEFAULT_SLEEP_DURATION 60

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

void	print(std::string inip, int inport, std::string dest_info_file, int interval, Pid_list &pid, Packet_info &packet, bool *active)
{
	std::ofstream									fd_packets;
	std::ofstream									fd_octets;
	std::ofstream									fd_debit;
	std::ofstream									fd_interval;
	std::ofstream									fd_interval_pcr;
	std::ofstream									fd;
	static unsigned long long int					saved_value;
	unsigned long int								debit;
	std::list<Pid>::iterator						pid_it;
	
	sleep(interval);
	if (*active == true)
	{
		std::cout << "print start" << std::endl;
		for (pid_it = pid.pid_list.begin(); pid_it != pid.pid_list.end(); pid_it++)
		{
			// Ecriture des fichiers "pkts_in_ip_port du flux entrant_pidN" Pour tout N
			try
			{
				fd.open(dest_info_file + "pkts_in_" + inip + "_" + std::to_string(inport) + "_" + std::to_string((*pid_it).pid) + ".txt", std::ofstream::out | std::ofstream::trunc);
				if (fd.fail())
					std::cerr << "Opening " << dest_info_file << "pkts_in_" << inip << "_" << std::to_string(inport) << "_" << std::to_string((*pid_it).pid) << ".txt" << " failed" << std::endl;
				std::cout << "double cacule" << std::endl;
				double packets_per_second = ((*pid_it).pkts_per_pids - (*pid_it).packet_per_pid_saved_value) / (double)interval;
				std::cout << "end calcule" << std::endl;
				(*pid_it).packet_per_pid_saved_value = (*pid_it).pkts_per_pids;
				fd << packets_per_second << std::endl;
				fd.flush();
			}
			catch(std::exception &e)
			{
				std::cerr << "Exception: " << e.what() << std::endl;
			}
			fd.close();
		
			// Ecriture des fichiers "continuity_error_in_ip_port du flux entrant_pidN" Pour tout N
			try
			{
				fd.open(dest_info_file + "continuity_error_in_" + inip + "_" + std::to_string(inport) + "_" + std::to_string((*pid_it).pid) + ".txt", std::ofstream::out | std::ofstream::trunc);
				if (fd.fail())
					std::cerr << "Opening " << dest_info_file << "continuity_error_in_" << inip << "_" << std::to_string(inport) << "_" << std::to_string((*pid_it).pid) << ".txt" << " failed" << std::endl;
				std::cout << "PID: " << (*pid_it).pid << " = " << (*pid_it).continuity_error_per_pid << " continuity errors" << std::endl;
				fd << (*pid_it).continuity_error_per_pid << std::endl;
				fd.flush();
			}
			catch(std::exception &e)
			{
				std::cerr << "Exception: " << e.what() << std::endl;
			}
			fd.close();
		}
		std::cout << "midle start" << std::endl;
		// Ecriture des Fichiers Octects.txt Packets.txt Debit.txt
		debit = ((packet.octets_read - saved_value) * 8) / (int)interval; // get debit = bite per second
		std::cout << "packet.packets_read: " << packet.packets_read << std::endl;
		std::cout << "packet.octets_read: " << packet.octets_read << std::endl;
		std::cout << "debit: " << debit << std::endl;
		std::cout << "interval max between two packets: " << packet.max_interval_value_between_packets << " milliseconds" << std::endl;
		std::cout << "interval max between two packets with pcr: " << packet.max_interval_value_between_pcr << " milliseconds" << std::endl;
		try
		{
			fd_packets.open(dest_info_file + "Packets.txt", std::ofstream::out | std::ofstream::trunc);
			fd_octets.open(dest_info_file + "Octets.txt", std::ofstream::out | std::ofstream::trunc);
			fd_debit.open(dest_info_file + "Debit.txt", std::ofstream::out | std::ofstream::trunc);
			fd_interval.open(dest_info_file + "Millisecond_intervale_max_between_packets.txt", std::ofstream::out | std::ofstream::trunc);
			fd_interval_pcr.open(dest_info_file + "Millisecond_intervale_max_between_packets_pcr.txt", std::ofstream::out | std::ofstream::trunc);
			if (fd_packets.fail())
				std::cerr << "Opening " << dest_info_file << "Packets.txt failed" << std::endl;
			if (fd_octets.fail())
				std::cerr << "Opening " << dest_info_file << "Octets.txt failed" << std::endl;
			if (fd_debit.fail())
				std::cerr << "Opening " << dest_info_file << "Debit.txt failed" << std::endl;
			if (fd_interval.fail())
				std::cerr << "Opening " << dest_info_file << "Millisecond_intervale_max_between_packets.txt failed" << std::endl;
			if (fd_interval_pcr.fail())
				std::cerr << "Opening " << dest_info_file << "Millisecond_intervale_max_between_packets_pcr.txt failed" << std::endl;
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
		}
		catch(std::exception &e)
		{
			std::cerr << "Exception: " << e.what() << std::endl;
		}
		fd_packets.close();
		fd_octets.close();
		fd_debit.close();
		fd_interval.close();
		fd_interval_pcr.close();
		saved_value = packet.octets_read;
	
		std::cout << "packet.counter000 " << packet.counter000 << " packet.counter001 " << packet.counter001 << std::endl;
		std::cout << "end start" << std::endl;
	}
	print(inip, inport, dest_info_file, interval, pid, packet, active);
}


int	init (int argc, char **argv, std::string &ingroup_main, int &inport_main, std::string &inip_main, std::string &outgroup_main, 
									int &outport_main, std::string &outip_main, int &ttl_main,
								 std::string &ingroup_second, int &inport_second, std::string &inip_second, std::string &outgroup_second, 
									int &outport_second, std::string &outip_second, int &ttl_second,
									std::string &dest_info_file_main, std::string &dest_info_file_second, int &interval_main, int &interval_second)
{
	// Option from file info recuperation
	boost::program_options::options_description file_description("File Options");
	boost::program_options::variables_map file_boost_map;
	std::string config_file = DEFAULT_FILE_CONFIG;
	
	file_description.add_options()
	("InMain.Group", boost::program_options::value<std::string>(&ingroup_main)->default_value(""), 						"Group Main In")
	("InMain.Ip", boost::program_options::value<std::string>(&inip_main)->default_value(""), 							"Ip Main In")
	("InMain.Port", boost::program_options::value<int>(&inport_main)->default_value(0), 								"Port Main In")
	("OutMain.Group", boost::program_options::value<std::string>(&outgroup_main)->default_value(""), 					"Group Main Out")
	("OutMain.Ip", boost::program_options::value<std::string>(&outip_main)->default_value(""), 							"Ip Main Out")
	("OutMain.Port", boost::program_options::value<int>(&outport_main)->default_value(0), 								"Port Main Out")
	("OutMain.Ttl", boost::program_options::value<int>(&ttl_main)->default_value(0), 									"Ttl Main Out")
	("OutMain.StatsPath", boost::program_options::value<std::string>(&dest_info_file_main)->default_value(""), 			"Main Stats Path")
	("OutMain.Interval", boost::program_options::value<int>(&interval_main)->default_value(DEFAULT_SLEEP_DURATION),		"Main Interval between each print out/file in seconds")
	("InSecond.Group", boost::program_options::value<std::string>(&ingroup_second)->default_value(""), 					"Group Second In")
	("InSecond.Ip", boost::program_options::value<std::string>(&inip_second)->default_value(""), 						"Ip Second In")
	("InSecond.Port", boost::program_options::value<int>(&inport_second)->default_value(0), 							"Port Second In")
	("OutSecond.Group", boost::program_options::value<std::string>(&outgroup_second)->default_value(""), 				"Group Second Out")
	("OutSecond.Ip", boost::program_options::value<std::string>(&outip_second)->default_value(""), 						"Ip Second Out")
	("OutSecond.Port", boost::program_options::value<int>(&outport_second)->default_value(0),							"Port Second Out")
	("OutSecond.Ttl", boost::program_options::value<int>(&ttl_second)->default_value(0),								"Ttl Second Out")
	("OutSecond.StatsPath", boost::program_options::value<std::string>(&dest_info_file_second)->default_value(""), 		"Second Stats Path")
	("OutSecond.Interval", boost::program_options::value<int>(&interval_second)->default_value(DEFAULT_SLEEP_DURATION),	"Second Interval between each print out/file in seconds");
	
	// Option from Command Line recuperation
	boost::program_options::options_description description("Command Line Options");
	boost::program_options::variables_map boost_map;
	
	description.add_options()
	("ingroup_main", boost::program_options::value<std::string>(&ingroup_main), 			"Main Group In")
	("inip_main", boost::program_options::value<std::string>(&inip_main), 					"Main Ip In")
	("inport_main", boost::program_options::value<int>(&inport_main), 						"Main Port In")
	("outgroup_main", boost::program_options::value<std::string>(&outgroup_main), 			"Main Group Out")
	("outip_main", boost::program_options::value<std::string>(&outip_main), 				"MainIp Out")
	("outport_main", boost::program_options::value<int>(&outport_main), 					"Main Port Out")
	("ttl_main", boost::program_options::value<int>(&ttl_main), 							"Main Ttl Out")
	("statspath_main", boost::program_options::value<std::string>(&dest_info_file_main),"Main Stats Path")
	("interval_main", boost::program_options::value<int>(&interval_main), 				"Main Interval between each print out/file in seconds")
	("ingroup_second", boost::program_options::value<std::string>(&ingroup_second), 		"Second Group In")
	("inip_second", boost::program_options::value<std::string>(&inip_second), 				"Second Ip In")
	("inport_second", boost::program_options::value<int>(&inport_second), 					"Second Port In")
	("outgroup_second", boost::program_options::value<std::string>(&outgroup_second), 		"Second Group Out")
	("outip_second", boost::program_options::value<std::string>(&outip_second), 			"Second Ip Out")
	("outport_second", boost::program_options::value<int>(&outport_second), 				"Second Port Out")
	("ttl_second", boost::program_options::value<int>(&ttl_second), 						"Second Ttl Out")
	("statspath_second", boost::program_options::value<std::string>(&dest_info_file_second),"Second Stats Path")
	("interval_second", boost::program_options::value<int>(&interval_second), 				"Second Interval between each print out/file in seconds")
	("config", boost::program_options::value<std::string>(&config_file), 					"Config File Name")
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
		std::cerr << "Group, Ip, Port from In/Out and Ttl Out for Main and Second process needed to run it" << std::endl;
		std::cerr << description << std::endl << file_description << std::endl;
		return (1);
	}
	else if (boost_map.size() == 1 && boost_map.count("help"))
	{
		std::cerr << description << std::endl << file_description << std::endl;
		return (1);
	}
	if (dest_info_file_main != "" ) {
 	   try {
 	      if (!boost::filesystem::is_directory(dest_info_file_main)) {
 	         boost::filesystem::create_directories(dest_info_file_main);
 	      }
 	   } catch (const boost::filesystem::filesystem_error& e) {
 	         std::cerr << "Failed to check or create directory:" << dest_info_file_main << " with error:" << e.what() << std::endl;
 	         return(1) ;
     }
 	}
	if (dest_info_file_second != "" ) {
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

int	packet_size_guessing(char databuf_in[16384], int size_read)
{
	int size_testing[1] = {188};
	
	for(int x = 0; x != 1; x++)
	{
		int index = 0;
		if (databuf_in[size_testing[x] * index++] == 'G' && 
			databuf_in[size_testing[x] * index++] == 'G' &&
			databuf_in[size_testing[x] * index++] == 'G')
			return (size_testing[x]);
	}
	return (-1);
}

// 00000000 00000000 00000001 (1110**** for video or 110***** for audio) = PES start code
int	PES_analysis(int &packets_size, int &x, char databuf_in[16384], short PID, std::list<Pid>::iterator pid_it, Pid_list &pid)
{
	
	// IN test, arrange this function when finished
	
	static unsigned int s_video_packet_nbr;
	static unsigned int s_audio_packet_nbr;
	int					pos;
	
	for (pos = 0; pos != packets_size; pos++)
		{
			if ((((x * packets_size) + pos + 1) <= ((x + 1) * packets_size)) && (((x * packets_size) + pos + 2) <= ((x + 1) * packets_size)))
				if (databuf_in[(x * packets_size) + pos] == 0 && databuf_in[(x * packets_size) + pos + 1] == 0 && databuf_in[(x * packets_size) + pos + 2] == 1)
				{
					//std::cout << "PID: " << PID << " stream id " << std::bitset<8>(databuf_in[(x * packets_size) + pos + 3]) << std::endl;
					short pes_length = ((databuf_in[(x * packets_size) + pos + 4] << 8) | databuf_in[(x * packets_size) + pos + 5]);
					//std::cout << "PES packet length " << pes_length << std::endl;
					short header_pes_len = databuf_in[(x * packets_size) + pos + 8];
					//std::cout << "PES header length " << header_pes_len << std::endl;
					s_video_packet_nbr = 0;
					s_audio_packet_nbr = 0;
					std::string stream_id = std::bitset<8>(databuf_in[(x * packets_size) + pos + 3]).to_string<char,std::string::traits_type,std::string::allocator_type>();
					if (stream_id >= "11000000" && stream_id <= "11011111")
						(*pid_it).description = "Audio";
					else if (stream_id >= "11100000" && stream_id <= "11101111")
						(*pid_it).description = "Video";
					//pid.print_list();
					break;
				}
		}
	if ((*pid_it).description == "Video")
	{
		++s_video_packet_nbr;
		return (1);
	}
	else if ((*pid_it).description == "Audio")
	{
		++s_audio_packet_nbr;
		return (1);
	}
	/*int i = x * packets_size;
	std::cout << "PID: " << PID << std::endl;
	while (i != (x * packets_size) + packets_size)
	{
		std::cout << std::bitset<8>(databuf_in[i]) << " ";
		++i;
	}
	std::cout << std::endl;*/
}

void	PMT_analysis(char databuf_in[16384], int &x, int &packets_size, short &section_length, std::list<Pid>::iterator pid_it, Pid_list &pid)
{
	//octet 13 14 pcr pid
	short pcr_pid = ((databuf_in[(x * packets_size) + 13] & 0x1f) << 8) | (databuf_in[(x * packets_size) + 14] & 0xff);
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
		table_id_extension = (databuf_in[(x * packets_size) + 8] << 8) | (databuf_in[(x * packets_size) + 9] & 0xff);
		// octet 17 = premier stream type boucle jusque fin de section
		stream_type = databuf_in[(x * packets_size) + 17 + boucle_length];
		//octet 18-19 = premier elementary pid
		elementary_pid = ((databuf_in[(x * packets_size) + 18 + boucle_length] & 0x1f) << 8) | (databuf_in[(x * packets_size) + 19 + boucle_length] & 0xff);
		//octet 20-21 = es_info_length_length 
		// es_info_length_length give the number of bytes (right after bytes 20-21) that are used for Descriptor enum (Descriptor section in Wikipedia)
		es_info_length_length = ((databuf_in[(x * packets_size) + 20 + boucle_length] & 0x3) << 8) | (databuf_in[(x * packets_size) + 21 + boucle_length] & 0xff);
		// octet 22 = premier descriptor_tag
		descriptor_tag = databuf_in[(x * packets_size) + 22 + boucle_length];
		// octet 23 = premiere definition du nombre d'octet qui suive l'octet 23 pour la description du descriptor
		descriptor_length = databuf_in[(x * packets_size) + 23 + boucle_length];		
				
		/*std::cout << "stream_type: " << stream_type << " elementary_pid: " << elementary_pid << " es_info_length_length: "
		<< es_info_length_length << " boucle_length: " << boucle_length << std::endl;
		*/
		bool pcr = false;
		if ((pid_it = pid.find_pid(elementary_pid)) == pid.pid_list.end())
		{
			if (pcr_pid == elementary_pid)
				pcr = true;
			pid.pid_list.push_back(Pid(elementary_pid, "PES", "", 0, 0, 99,0, pcr, 0));
			//pid.print_list();
		}
		else
		{
			if (pcr_pid == elementary_pid)
				pcr = true;
			(*pid_it).type = "PES";
			(*pid_it).contain_pcr = pcr;
		}
		es_counter = 2 + descriptor_length; // (2 = bytes used by descriptor_tag and descriptor_length) + bytes used for description of destructor_tag
				
		/*std::cout << "program number: " << table_id_extension << std::endl;
		std::cout << "descriptor_tag: " << descriptor_tag << " descriptor_length: " << descriptor_length;
		std::cout << " descriptor_info: ";
		for (int i = 1; i <= descriptor_length; i++)
		std::cout << " " << std::bitset<8>(databuf_in[(x * packets_size) + 23 + boucle_length + i]);
		std::cout << std::endl;
		*/
		while (es_counter != es_info_length_length)
		{
			descriptor_tag = databuf_in[(x * packets_size) + 22 + boucle_length + es_counter];
			descriptor_length = databuf_in[(x * packets_size) + 23 + boucle_length + es_counter];
	
		/*	std::cout << "descriptor_tag: " << descriptor_tag << " descriptor_length: " << descriptor_length;
			std::cout << " descriptor_info: ";
			for (int i = 1; i <= descriptor_length; i++)
			std::cout << " " << std::bitset<8>(databuf_in[(x * packets_size) + 23 + boucle_length + es_counter + i]);
			std::cout << std::endl << std::endl;
		*/
			es_counter = es_counter + 2 + descriptor_length; // (2 = bytes used by descriptor_tag and descriptor_length) + bytes used for description of destructor_tag
		}
		boucle_length = boucle_length + es_info_length_length + 5;
	}
}

void	PAT_analysis(char databuf_in[16384], int &x, int &packets_size, short &section_length, std::list<Pid>::iterator pid_it, Pid_list &pid)
{
	short pat_repetition_size = 4;
	short repetition = 0;
	// Program num
	/* 16bit value = (((databuf_in[(x * packets_size) + 13] & 0xff) << 8) | (databuf_in[(x * packets_size) + 14] & 0xff));
	*/
	if (pid_it == pid.pid_list.end())
		pid.pid_list.push_back(Pid(0, "PSI", "PAT", 0, 0, 99,0, false, 0));
	// PMT PID / Program Map PID, begin at byte 15-16
	while (15 + (repetition * pat_repetition_size) < section_length + 6) // 6 = byte number of section length begining 
	{
		short program_map_pid = ((databuf_in[(x * packets_size) + 15 + (repetition * pat_repetition_size)] & 0x1f) << 8) | 
			(databuf_in[(x * packets_size) + 16] & 0xff);
		
		if ((pid_it = pid.find_pid(program_map_pid)) == pid.pid_list.end())
		{
			pid.pid_list.push_back(Pid(program_map_pid, "PSI", "PMT", 0, 0, 99,0, false, 0));
			//pid.print_list();
		}
		else
		{
			(*pid_it).type = "PSI";
			(*pid_it).description = "PMT";
		}
		++repetition;
	}
}

int	packet_monitoring(char databuf_in[16384], int &datalen_out, boost::posix_time::ptime &last_time_pcr, Pid_list &pid, Packet_info &packet)
{
	std::list<Pid>::iterator pid_it;
	int packets_size;
	if ((packets_size = packet_size_guessing(databuf_in, datalen_out)) == -1)
		return (1);
	int packets_per_read = datalen_out / packets_size;
		
	packet.packets_read = packet.packets_read + packets_per_read;
	packet.octets_read = packet.octets_read + datalen_out;
	
	// découpage packets lu par chaque read, creation list de pid et un vector permetant de compter packet par pid		
	for (int x = 0; x != packets_per_read; x++)
	{
		if (databuf_in[(x * packets_size)] == 'G') // mpeg ts packet begins with octet 0 = G
		{
			short PID; // PID
			std::string type = "";
			std::string description = "";
			
			PID = ((databuf_in[(x * packets_size) + 1] << 8) | databuf_in[(x * packets_size) + 2]) & 0x1fff;
			// octet 6-7 section length, "These bytes must not exceed a value of 1021"
			short section_length;
			if ((section_length= ((databuf_in[(x * packets_size) + 6] & 0x7) << 8) | (databuf_in[(x * packets_size) + 7] & 0xff)) > 1021)
				section_length = 0;
			
			pid_it = pid.find_pid(PID);
			// null packet
			if (PID == 8191 && pid_it == pid.pid_list.end())
				pid.pid_list.push_back(Pid(PID, "NUL", "Nul packet", 0, 0, 99,0, false, 0));
			// DVB
			else if (PID >= 16 && PID <= 31)
				{
					type = "DVB";
					//check table id
					if (databuf_in[(x * packets_size) + 5] == 66 || databuf_in[(x * packets_size) + 5] == 70)
						description = "SDT";
					if (pid_it == pid.pid_list.end())
						pid.pid_list.push_back(Pid(PID, type, description, 0, 0, 99,0, false, 0));
					else
					{
						(*pid_it).type = type;
						(*pid_it).description = description;
					}
				}
			//PSI
			else if (PID == 0) // PAT
				PAT_analysis(databuf_in, x, packets_size, section_length, pid_it, pid);
			// PMT
			else if (pid_it != pid.pid_list.end() && (*pid_it).description == "PMT")		
				PMT_analysis(databuf_in, x, packets_size, section_length, pid_it, pid);
			// PES
			else if (pid_it != pid.pid_list.end() && (*pid_it).type == "PES")
				PES_analysis(packets_size, x, databuf_in, PID, pid_it, pid);

			//pid_it = pid.find_pid(PID);
			(*pid_it).pkts_per_pids = (*pid_it).pkts_per_pids + 1;
			int continuity = databuf_in[(x * packets_size) + 3] & 0x0F;
		
			// Continuity Error Check
			if (PID != 8191 && (*pid_it).last_continuity_counter_per_pid != 99) // PID 8191 n'a pas de continuity counter, On evite la comparaison de l'initialisation 99
			{
				if ((databuf_in[(x * packets_size) + 3] & (1u << 5))) // **1* **** Adaptation Field Control = 10 or 11
				{
					if ((databuf_in[(x * packets_size) + 3] & (1u << 4))) // **11 **** Adaptation Field Control = 11
					{
						if ((databuf_in[(x * packets_size) + 5] & (1u << 7))) // **11 1*** Adaptation Field Control = 11 and Discontinuity Indicator = 1
						{
							if ((*pid_it).last_continuity_counter_per_pid != continuity) // verif =
								std::cout << "Adaptation field control = 11 and Discontinuity Indicator = 1" <<std::endl;
							}
						else // **11 0*** Adaptation Field Control = 11 and Discontinuity Indicator = 0
						{	
						
							//verif cc
							if ((((*pid_it).last_continuity_counter_per_pid + 1 != continuity) && (*pid_it).last_continuity_counter_per_pid != 15) || ((*pid_it).last_continuity_counter_per_pid == 15 && continuity != 0))
							{
								(*pid_it).continuity_error_per_pid = (*pid_it).continuity_error_per_pid + 1;
								std::cout << "Adaptation field control  = 11 and Discontinuity Indicator = 0"<<std::endl;
							}
						}
					}
					else // **10 **** Adaptation Field Control = 10
					{
						if ((*pid_it).last_continuity_counter_per_pid != continuity) // verif =
						{
							(*pid_it).continuity_error_per_pid = (*pid_it).continuity_error_per_pid + 1;
							std::cout << "Adaptation field control = 10 and continuity counter not equal to precedent continuity counter"<<std::endl;
						}
						if ((databuf_in[(x * packets_size) + 5] & (1u << 7))) // **10 1*** Adaptation Field Control = 10 and Discontinuity Indicator = 1
						{
						
						}
						else // **10 0*** Adaptation Field Control = 10 and Discontinuity Indicator = 0
						{
						
						}
					}
				}
				else // **0* **** Adaptation Field Control = 01 or 00				
				{
					if ((databuf_in[(x * packets_size) + 3] & (1u << 4))) // **01 **** Adaptation Field Control = 01 
					{
						// verif cc
						if ((((*pid_it).last_continuity_counter_per_pid + 1 != continuity) && (*pid_it).last_continuity_counter_per_pid != 15) || ((*pid_it).last_continuity_counter_per_pid == 15 && continuity != 0))
						{
							(*pid_it).continuity_error_per_pid = (*pid_it).continuity_error_per_pid + 1;
							std::cout << "Adaptation field control = 01 and continuity error detected " << std::endl;
						}
					}
					else // **00 **** Adaptation Field Control = 00
					{
						if ((databuf_in[(x * packets_size) + 5] & (1u << 7))) // **00 1*** Adaptation Field Control = 00 and Discontinuity Indicator = 1
						{
							++packet.counter001;
						}
						else // **00 0*** Adaptation Field Control = 00 and Discontinuity Indicator = 0
						{
							++packet.counter000;
						}
					}
				}
			}
			(*pid_it).last_continuity_counter_per_pid = continuity;
		
			// Check if its a packet with PCR field For interval calculation between 2 packets with pcr field
			if ((*pid_it).contain_pcr == true && databuf_in[(x * packets_size) + 5] & (1u << 4)) // Checking if PCR flag = 1
			{
				boost::posix_time::ptime actual_time_pcr = boost::posix_time::microsec_clock::local_time();
				boost::posix_time::time_duration diff = actual_time_pcr - last_time_pcr;
				if (diff.total_milliseconds() > packet.max_interval_value_between_pcr)
					packet.max_interval_value_between_pcr = diff.total_milliseconds();
				last_time_pcr = actual_time_pcr;
			}
		}
	}
	return (0);
}

int	flux_start(bool *active, std::string ingroup, int inport, std::string inip, std::string outgroup, int outport,
				std::string outip, int ttl, std::string dest_info_file, int interval)
{
	struct 					in_addr localInterface;
    struct 					sockaddr_in groupSock;
    int 					sd_out;
    char 					databuf_out[16384] = "Multicast test message lol!";
    int 					datalen_out;// = sizeof(databuf_out);
	struct 					sockaddr_in localSock;
    struct 					ip_mreq group;
    int 					sd_in;
    int 					datalen_in;
    char 					databuf_in[16384];

	struct sockaddr_storage my_addr;
    struct addrinfo 		*res0 = 0;
    int 					addr_len;
	
	Pid_list	pid;	
	Packet_info	packet;
	
	std::cout << "flux start" << std::endl;
	res0 = udp_resolve_host( 0, outport, SOCK_DGRAM, AF_INET, AI_PASSIVE );
     if( res0 == 0 ) {
		 std::cerr << "udp_resolve_host failed" << std::endl;
		return(1);
     }
     memcpy( &my_addr, res0->ai_addr, res0->ai_addrlen );
     addr_len = res0->ai_addrlen;
     freeaddrinfo( res0 );


    /* Create a datagram socket on which to send. */
    sd_out = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd_out < 0) {
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
      if(setsockopt(sd_out, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
       std::cerr << "Setting SO_REUSEADDR error on sd_out" << std::endl;
       close(sd_out);
       return(1);
      } 
    }
  /* bind  */
  if( bind( sd_out, (struct sockaddr *)&my_addr, addr_len ) < 0 ) {
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
    if(setsockopt(sd_out, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0) {
      std::cerr << "Setting local out interface error" << std::endl;
      return(1);
    } else {
      std::cout << "Setting the local out interface...OK\n" << std::endl;
    }
    
    if( setsockopt( sd_out, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl) ) < 0 ) {   
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

    sd_in = socket(AF_INET, SOCK_DGRAM, 0);

    if(sd_in < 0) {
      std::cerr << "Opening datagram socket error" << std::endl;
      return(1);
    } else {
      std::cout << "Opening datagram socket....OK." << std::endl;
      /* Enable SO_REUSEADDR to allow multiple instances of this */
      /* application to receive copies of the multicast datagrams. */
      {
      int reuse = 1;
      if(setsockopt(sd_in, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
       std::cerr << "Setting SO_REUSEADDR error" << std::endl;
       close(sd_in);
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
    if(bind(sd_in, (struct sockaddr*)&localSock, sizeof(localSock))) {
      std::cerr << "Binding datagram socket in error" << std::endl;
      close(sd_in);
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
    if(setsockopt(sd_in, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
      std::cerr << "Adding multicast group in error" << std::endl;
      close(sd_in);
      return(1);
    } else {
      std::cout << "Adding multicast group in...OK." << std::endl;
    }

    /* Read from the socket. */
    datalen_in = sizeof(databuf_in);
	
	/*if (active == true)
	{*/
		std::thread t(print, inip, inport, dest_info_file, interval, std::ref(pid), std::ref(packet), active);
		t.detach();
	//}
	//First time getter
	boost::posix_time::ptime last_time = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::ptime last_time_pcr = last_time;
	
	/*fd_set	fd_input;
	struct timeval	tv;
	int retval;
	
	FD_ZERO(&fd_input);
	FD_SET(sd_in, &fd_input);
	tv.tv_sec = 5;
	tv.tv_usec = 0;*/
	
    while(1) {
		/*retval = select(sd_in + 1, &fd_input, NULL, NULL, &tv);
		if (retval) // changement d'état détecter
		{
			std::cout << "Changement d'état détecter" << std::endl;
			//exit(0);
		}
		else // select time out
		{
			std::cout << "Aucun changement d'état détecter depuis 5 sec" << std::endl;
			//exit(0);
		}*/
      
		datalen_out = read(sd_in, databuf_in, datalen_in);
		// Calcule en milliseconds de l'intervale de reception de chaque packets
		boost::posix_time::ptime actual_time  = boost::posix_time::microsec_clock::local_time();
		boost::posix_time::time_duration diff = actual_time - last_time;
		if (diff.total_milliseconds() > packet.max_interval_value_between_packets)
			packet.max_interval_value_between_packets = diff.total_milliseconds();
		last_time = actual_time;
	  
		if(datalen_out < 0) 
		{
			std::cerr << "Reading datagram im message error" << std::endl;
			close(sd_in);
			close(sd_out);
			return(1);
		} else {
        //printf("Reading datagram message in ...OK.\n");
		//std::cout << " **r: " << databuf_in << "  " <<strlen(databuf_in) << "**";
    //    printf("The message from multicast server in is: \"%s\"\n", databuf_in);
      }
		if (*active == true)
			if(sendto(sd_out, databuf_in, datalen_out, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0)
				std::cerr << "Sending datagram message out error" << std::endl;
			//printf("Sending datagram message out...OK\n");
			//std::cout << " w ";
			/*for (int i = 0; i != strlen(databuf_in); i++)
				std::cout << std::bitset<8>(databuf_in[i]) << " ";*/
			//std::cout << " w: " << databuf_in << " size: " << strlen(databuf_in);
			/*int len = strlen(databuf_in);
			if (len > 3)
			{*/
		packet_monitoring(databuf_in, datalen_out, last_time_pcr, pid, packet);
    }
	return (0);
}

int	main(int argc, char **argv)
{
	std::string 		ingroup_main;
	int  				inport_main;
	std::string 		inip_main;
	std::string			outgroup_main; 
    int					outport_main;
    std::string 		outip_main;
	int 				ttl_main = 0;
	std::string			dest_info_file_main;	
	int					interval_main;
	
	std::string 		ingroup_second;
	int  				inport_second;
	std::string 		inip_second;
	std::string			outgroup_second; 
    int					outport_second;
    std::string 		outip_second;
	int 				ttl_second = 0;
	std::string			dest_info_file_second;
	int					interval_second;
	bool				Main = true;
	bool				Second = false;
	
	if (init(argc, argv, ingroup_main, inport_main, inip_main, outgroup_main, outport_main, outip_main, ttl_main,
						ingroup_second, inport_second, inip_second, outgroup_second, outport_second, outip_second, ttl_second,
						dest_info_file_main, dest_info_file_second, interval_main, interval_second) == 1)
		return (1);
	std::thread primary(flux_start, &Main, ingroup_main, inport_main, inip_main, outgroup_main, outport_main, outip_main, ttl_main, dest_info_file_main, interval_main);
	std::thread secondary(flux_start, &Second, ingroup_second, inport_second, inip_second, outgroup_second, outport_second, outip_second, ttl_second, dest_info_file_second, interval_second);
	
	
	fd_set	fd_input;
	struct timeval	tv;
	int retval;
	
	FD_ZERO(&fd_input);
	FD_SET(0, &fd_input);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	
	retval = select(0 + 1, &fd_input, NULL, NULL, &tv);
	if (retval) // changement d'état détecter
	{
		std::cout << "Changement d'état détecter" << std::endl;
		//exit(0);
	}
	else // select time out
	{
		std::cout << "Aucun changement d'état détecter depuis 5 sec" << std::endl;
		bool temporaire = Main;
		Main = Second;
		Second = temporaire;
		//exit(0);
	}
	primary.join();
	secondary.join();
	return (0);
}