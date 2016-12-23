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

#define DEFAULT_FILE_CONFIG "mcastreplay.ini"
#define DEFAULT_SLEEP_DURATION 60

std::vector<short>					program_map_pids;
std::list<short> 					pid_list;
std::vector<unsigned long long int>	pkts_per_pids;
std::vector<unsigned long long int> continuity_error_per_pid;
std::vector<int> 					last_continuity_counter_per_pid;
unsigned long long int				packets_read = 0;
unsigned long long int				octets_read = 0;
std::string							dest_info_file;
unsigned long long int				max_interval_value_between_packets = 0;
unsigned long long int				max_interval_value_between_pcr = 0;
static int							interval;

static std::string 		s_ingroup;
static int  			s_inport;
static std::string 		s_inip;

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

void	print()
{
	std::ofstream									fd_packets;
	std::ofstream									fd_octets;
	std::ofstream									fd_debit;
	std::ofstream									fd_interval;
	std::ofstream									fd_interval_pcr;
	std::ofstream									fd;
	static unsigned long long int					saved_value;
	unsigned long int								debit;
	std::list<short>::iterator						pid_it;
	std::vector<unsigned long long int>::iterator	value_it;
	static std::vector<unsigned long long int>		packet_per_pid_saved_value;
	static int 										door;
	
	sleep(interval);
	
	if (door == 0)
	{
		for (pid_it = pid_list.begin(); pid_it != pid_list.end(); pid_it++)
			packet_per_pid_saved_value.push_back(0);
		++door;
	}
	
	// Ecriture des fichiers "pkts_in_ip_port du flux entrant_pidN" Pour tout N
	int position = 0;
	for (pid_it = pid_list.begin(); pid_it != pid_list.end(); pid_it++)
	{
		try
		{
			fd.open(dest_info_file + "pkts_in_" + s_inip + "_" + std::to_string(s_inport) + "_" + std::to_string(*pid_it) + ".txt", std::ofstream::out | std::ofstream::trunc);
			if (fd.fail())
				std::cerr << "Opening " << dest_info_file << "pkts_in_" << s_inip << "_" << std::to_string(s_inport) << "_" << std::to_string(*pid_it) << ".txt" << " failed" << std::endl;
			double packets_per_second = (pkts_per_pids[position] - packet_per_pid_saved_value[position]) / (double)interval;
			packet_per_pid_saved_value[position] = pkts_per_pids[position];
			fd << packets_per_second << std::endl;
			fd.flush();
		}
		catch(std::exception &e)
		{
			std::cerr << "Exception: " << e.what() << std::endl;
		}
		position++;
		fd.close();
	}
	
	// Ecriture des fichiers "continuity_error_in_ip_port du flux entrant_pidN" Pour tout N
	position = 0;
	for (pid_it = pid_list.begin(); pid_it != pid_list.end(); pid_it++)
	{
		try
		{
			fd.open(dest_info_file + "continuity_error_in_" + s_inip + "_" + std::to_string(s_inport) + "_" + std::to_string(*pid_it) + ".txt", std::ofstream::out | std::ofstream::trunc);
			if (fd.fail())
				std::cerr << "Opening " << dest_info_file << "continuity_error_in_" << s_inip << "_" << std::to_string(s_inport) << "_" << std::to_string(*pid_it) << ".txt" << " failed" << std::endl;
			std::cout << "PID: " << *pid_it << " = " << continuity_error_per_pid[position] << " continuity errors" << std::endl;
			fd << continuity_error_per_pid[position] << std::endl;
			fd.flush();
		}
		catch(std::exception &e)
		{
			std::cerr << "Exception: " << e.what() << std::endl;
		}
		position++;
		fd.close();
	}
	
	// Ecriture des Fichiers Octects.txt Packets.txt Debit.txt
	debit = ((octets_read - saved_value) * 8) / interval; // get debit = bite per second
	std::cout << "packets_read: " << packets_read << std::endl;
	std::cout << "octets_read: " << octets_read << std::endl;
	std::cout << "debit: " << debit << std::endl;
	std::cout << "interval max between two packets: " << max_interval_value_between_packets << " milliseconds" << std::endl;
	std::cout << "interval max between two packets with pcr: " << max_interval_value_between_pcr << " milliseconds" << std::endl;
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
		fd_packets << packets_read << std::endl;
		fd_packets.flush();
		fd_octets << octets_read << std::endl;
		fd_octets.flush();
		fd_debit << debit << std::endl;
		fd_debit.flush();
		fd_interval << max_interval_value_between_packets << std::endl;
		fd_interval.flush();
		fd_interval_pcr << max_interval_value_between_pcr << std::endl;
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
	saved_value = octets_read;
	
	
	int x = 0;
	for (std::list<short>::iterator it = pid_list.begin(); it != pid_list.end(); it++)
	{
		std::cout << "PID " << *it << " = " << pkts_per_pids[x] << " packets" << std::endl;
		++x;
	}
	print();
}

int	init (int argc, char **argv, std::string &s_ingroup, int &s_inport, std::string &s_inip, std::string &s_outgroup, int &s_outport, std::string &s_outip, int &ttl)
{
	// Option from file info recuperation
	boost::program_options::options_description file_description("File Options");
	boost::program_options::variables_map file_boost_map;
	std::string config_file = DEFAULT_FILE_CONFIG;
	
	file_description.add_options()
	("In.Group", boost::program_options::value<std::string>(&s_ingroup)->default_value(""), "Group In")
	("In.Ip", boost::program_options::value<std::string>(&s_inip)->default_value(""), "Ip In")
	("In.Port", boost::program_options::value<int>(&s_inport)->default_value(0), "Port In")
	("Out.Group", boost::program_options::value<std::string>(&s_outgroup)->default_value(""), "Group Out")
	("Out.Ip", boost::program_options::value<std::string>(&s_outip)->default_value(""), "Ip Out")
	("Out.Port", boost::program_options::value<int>(&s_outport)->default_value(0), "Port Out")
	("Out.Ttl", boost::program_options::value<int>(&ttl)->default_value(0), "Ttl Out")
	("Out.StatsPath", boost::program_options::value<std::string>(&dest_info_file)->default_value(""), "Stats Path")
	("Out.Interval", boost::program_options::value<int>(&interval)->default_value(DEFAULT_SLEEP_DURATION), "Interval between each print out/file in seconds");
	
	// Option from Command Line recuperation
	boost::program_options::options_description description("Command Line Options");
	boost::program_options::variables_map boost_map;
	
	description.add_options()
	("ingroup", boost::program_options::value<std::string>(&s_ingroup), "Group In")
	("inip", boost::program_options::value<std::string>(&s_inip), "Ip In")
	("inport", boost::program_options::value<int>(&s_inport), "Port In")
	("outgroup", boost::program_options::value<std::string>(&s_outgroup), "Group Out")
	("outip", boost::program_options::value<std::string>(&s_outip), "Ip Out")
	("outport", boost::program_options::value<int>(&s_outport), "Port Out")
	("statspath", boost::program_options::value<std::string>(&dest_info_file), "Stats Path")
	("config", boost::program_options::value<std::string>(&config_file), "Config File Name")
	("ttl", boost::program_options::value<int>(&ttl), "Ttl Out")
	("interval", boost::program_options::value<int>(&interval), "Interval between each print out/file in seconds")
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
	
	if (s_ingroup.empty() == true || s_inip.empty() == true || s_inport == 0 || s_outgroup.empty() == true || s_outip.empty() == true || s_outport == 0 || ttl == 0)
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

void	packet_monitoring(char databuf_in[16384], int &datalen_out, boost::posix_time::ptime &last_time_pcr)
{
	std::list<short>::iterator index;
	int packets_size = packet_size_guessing(databuf_in, datalen_out);
	int packets_per_read = datalen_out / packets_size;
		
	packets_read = packets_read + packets_per_read;
	octets_read = octets_read + datalen_out;
		
		
	// découpage packets lu par chaque read, creation list de pid et un vector permetant de compter packet par pid		
	for (int x = 0; x != packets_per_read; x++)
	{
		short PID; // PID
			
		PID = ((databuf_in[(x * packets_size) + 1] << 8) | databuf_in[(x * packets_size) + 2]) & 0x1fff;
		if ((index = std::find(pid_list.begin(), pid_list.end(), PID)) == pid_list.end())
		{
			pid_list.push_back(PID);
			pkts_per_pids.push_back(0);
			last_continuity_counter_per_pid.push_back(99); // initialisation , continuity counter ne va pas au dessus de 15, 99 sert de verification pour les premier passages
			continuity_error_per_pid.push_back(0); // init
			std::cout << "PIDS list:" << std::endl;
			for (std::list<short>::iterator pid_it = pid_list.begin(); pid_it != pid_list.end(); pid_it++)
				std::cout << *pid_it << std::endl;
		}
		int position = std::distance(pid_list.begin(), index);
		int continuity = databuf_in[(x * packets_size) + 3] & 0x0F;
		pkts_per_pids[position] = pkts_per_pids[position] + 1;
		
		// Continuity Error Check
		if (PID != 8191 && last_continuity_counter_per_pid[position] != 99) // PID 8191 n'a pas de continuity counter, On evite la comparaison de l'initialisation 99
		{
			if ((databuf_in[(x * packets_size) + 3] & (1u << 5))) // **1* **** Adaptation Field Control = 10 or 11
			{
				if ((databuf_in[(x * packets_size) + 3] & (1u << 4))) // **11 **** Adaptation Field Control = 11
				{
					if ((databuf_in[(x * packets_size) + 5] & (1u << 7))) // **11 1*** Adaptation Field Control = 11 and Discontinuity Indicator = 1
					{
						if (last_continuity_counter_per_pid[position] != continuity) // verif =
							std::cout << "Adaptation field control = 11 and Discontinuity Indicator = 1" <<std::endl;
						}
					else // **11 0*** Adaptation Field Control = 11 and Discontinuity Indicator = 0
					{	
					
						//verif cc
						if (((last_continuity_counter_per_pid[position] + 1 != continuity) && last_continuity_counter_per_pid[position] != 15) || (last_continuity_counter_per_pid[position] == 15 && continuity != 0))
						{
							continuity_error_per_pid[position] = continuity_error_per_pid[position] + 1;
							std::cout << "Adaptation field control  = 11 and Discontinuity Indicator = 0"<<std::endl;
						}
					}
				}
				else // **10 **** Adaptation Field Control = 10
				{
					if (last_continuity_counter_per_pid[position] != continuity) // verif =
					{
						continuity_error_per_pid[position] = continuity_error_per_pid[position] + 1;
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
					if (((last_continuity_counter_per_pid[position] + 1 != continuity) && last_continuity_counter_per_pid[position] != 15) || (last_continuity_counter_per_pid[position] == 15 && continuity != 0))
					{
						continuity_error_per_pid[position] = continuity_error_per_pid[position] + 1;
						std::cout << "Adaptation field control = 01 and continuity error detected " << std::endl;
					}
				}
				else // **00 **** Adaptation Field Control = 00
				{
					if ((databuf_in[(x * packets_size) + 5] & (1u << 7))) // **00 1*** Adaptation Field Control = 00 and Discontinuity Indicator = 1
					{
						
					}
					else // **00 0*** Adaptation Field Control = 00 and Discontinuity Indicator = 0
					{
						
					}
				}
			}
		}
		last_continuity_counter_per_pid[position] = continuity;
		
		// Check if its a packet with PCR field For interval calculation between 2 packets with pcr field
		if (databuf_in[(x * packets_size) + 5] & (1u << 4)) // Checking if PCR flag = 1
		{
			boost::posix_time::ptime actual_time_pcr = boost::posix_time::microsec_clock::local_time();
			boost::posix_time::time_duration diff = actual_time_pcr - last_time_pcr;
			if (diff.total_milliseconds() > max_interval_value_between_pcr)
				max_interval_value_between_pcr = diff.total_milliseconds();
			last_time_pcr = actual_time_pcr;
		}
		
		// octet 6-7 section length, "These bytes must not exceed a value of 1021"
		short section_length;
		if ((section_length= ((databuf_in[(x * packets_size) + 6] & 0x7) << 8) | (databuf_in[(x * packets_size) + 7] & 0xff)) > 1021)
			section_length = 0;
		//PSI
		if (PID == 0) // PAT
		{
			short pat_repetition_size = 4;
			short repetition = 0;
		    // Program num
			/* 16bit value = (((databuf_in[(x * packets_size) + 13] & 0xff) << 8) | (databuf_in[(x * packets_size) + 14] & 0xff));
			*/
			
			// PMT PID / Program Map PID, begin at byte 15-16
			while (15 + (repetition * pat_repetition_size) < section_length + 6) // 6 = byte number of section length begining 
			{
				short program_map_pid = ((databuf_in[(x * packets_size) + 15 + (repetition * pat_repetition_size)] & 0x1f) << 8) | 
					(databuf_in[(x * packets_size) + 16] & 0xff);
				if (std::find(program_map_pids.begin(), program_map_pids.end(), program_map_pid) == program_map_pids.end())
					program_map_pids.push_back(program_map_pid);
				std::cout << "PID: " << PID << " Program Map Pid: " << program_map_pid << std::endl;
				++repetition;
			}
		}
		// PMT
		else if (std::find(program_map_pids.begin(), program_map_pids.end(), PID) != program_map_pids.end()) // Checking if this PMT has been mentioned in a PAT packet
		{
			int i = x * packets_size;
			std::cout << "PID: " << PID << std::endl;
			while (i != (x * packets_size) + packets_size)
			{
				std::cout << std::bitset<8>(databuf_in[i]) << " ";
				++i;
			}
			std::cout << std::endl;
			std::cout << "section length: " << section_length << std::endl;
			
			//octet 13 14 pcr pid
			short pcr_pid = ((databuf_in[(x * packets_size) + 13] & 0x1f) << 8) | (databuf_in[(x * packets_size) + 14] & 0xff);
			std::cout << "pcr_pid: " << pcr_pid << std::endl;
			short stream_type;
			short elementary_pid;
			short es_info_length_length;
			short descriptor_tag;
			short descriptor_length;
			short boucle_length;
			
			// counter to iterate through ES info
			short es_counter;// = 2 + descriptor_length; 

			// skip this number of bytes (boucle_length) to get an other stream_type
			boucle_length = 0;
			while (17 + boucle_length < section_length)
			{
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
				
				std::cout << "stream_type: " << stream_type << " elementary_pid: " << elementary_pid << " es_info_length_length: "
				<< es_info_length_length << " boucle_length: " << boucle_length << std::endl;
				
				es_counter = 2 + descriptor_length; // (2 = bytes used by descriptor_tag and descriptor_length) + bytes used for description of destructor_tag
				
				std::cout << "descriptor_tag: " << descriptor_tag << " descriptor_length: " << descriptor_length;
				std::cout << " descriptor_info: ";
				for (int i = 1; i <= descriptor_length; i++)
					std::cout << " " << std::bitset<8>(databuf_in[(x * packets_size) + 23 + boucle_length + i]);
				std::cout << std::endl;
				
				while (es_counter != es_info_length_length)
				{
					descriptor_tag = databuf_in[(x * packets_size) + 22 + boucle_length + es_counter];
					descriptor_length = databuf_in[(x * packets_size) + 23 + boucle_length + es_counter];
					
					std::cout << "descriptor_tag: " << descriptor_tag << " descriptor_length: " << descriptor_length;
					std::cout << " descriptor_info: ";
					for (int i = 1; i <= descriptor_length; i++)
						std::cout << " " << std::bitset<8>(databuf_in[(x * packets_size) + 23 + boucle_length + es_counter + i]);
					std::cout << std::endl << std::endl;
				
					es_counter = es_counter + 2 + descriptor_length; // (2 = bytes used by descriptor_tag and descriptor_length) + bytes used for description of destructor_tag
				}
				boucle_length = boucle_length + es_info_length_length + 5;
			}
		}
	}
}

int	main(int argc, char **argv)
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
	
	static std::string		s_outgroup; 
    static int				s_outport;
    static std::string 		s_outip;
	
	struct sockaddr_storage my_addr;
    struct addrinfo 		*res0 = 0;
    int 					addr_len;
	
	int ttl = 0;
	
	if (init(argc, argv, s_ingroup, s_inport, s_inip, s_outgroup, s_outport, s_outip, ttl) == 1)
		return (1);
	
	res0 = udp_resolve_host( 0, s_outport, SOCK_DGRAM, AF_INET, AI_PASSIVE );
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
    groupSock.sin_addr.s_addr = inet_addr(s_outgroup.c_str());
    groupSock.sin_port = htons(s_outport);

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

    localInterface.s_addr = inet_addr(s_outip.c_str());
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
    localSock.sin_port = htons(s_inport);
    //localSock.sin_addr.s_addr = INADDR_ANY; Dangereux! Pas de filtrage!
    localSock.sin_addr.s_addr = inet_addr(s_ingroup.c_str());
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
    group.imr_multiaddr.s_addr = inet_addr(s_ingroup.c_str());
    group.imr_interface.s_addr = inet_addr(s_inip.c_str());
    if(setsockopt(sd_in, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
      std::cerr << "Adding multicast group in error" << std::endl;
      close(sd_in);
      return(1);
    } else {
      std::cout << "Adding multicast group in...OK." << std::endl;
    }

    /* Read from the socket. */
    datalen_in = sizeof(databuf_in);
	
	std::thread t(print);
	
	//First time getter
	boost::posix_time::ptime last_time = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::ptime last_time_pcr = last_time;
	
    while(1) {
      datalen_out = read(sd_in, databuf_in, datalen_in);
	  
		// Calcule en milliseconds de l'intervale de reception de chaque packets
		boost::posix_time::ptime actual_time  = boost::posix_time::microsec_clock::local_time();
		boost::posix_time::time_duration diff = actual_time - last_time;
		if (diff.total_milliseconds() > max_interval_value_between_packets)
			max_interval_value_between_packets = diff.total_milliseconds();
		last_time = actual_time;
	  
      if(datalen_out < 0) {
        std::cerr << "Reading datagram im message error" << std::endl;
        close(sd_in);
        close(sd_out);
        return(1);
      } else {
        //printf("Reading datagram message in ...OK.\n");
		//std::cout << " **r: " << databuf_in << "  " <<strlen(databuf_in) << "**";
    //    printf("The message from multicast server in is: \"%s\"\n", databuf_in);
      }
	  
      if(sendto(sd_out, databuf_in, datalen_out, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
         std::cerr << "Sending datagram message out error" << std::endl;
      } else {
        //printf("Sending datagram message out...OK\n");
		//std::cout << " w ";
		/*for (int i = 0; i != strlen(databuf_in); i++)
			std::cout << std::bitset<8>(databuf_in[i]) << " ";*/
		//std::cout << " w: " << databuf_in << " size: " << strlen(databuf_in);
		/*int len = strlen(databuf_in);
		if (len > 3)
		{*/
	
		packet_monitoring(databuf_in, datalen_out, last_time_pcr);
      }
    }
	return (0);
}