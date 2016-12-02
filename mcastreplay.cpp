#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream> 
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boost/program_options.hpp>

// compilation: g++ mcastreplay.cpp -I ../boost_1_61_0/ ../boost_1_61_0/bin.v2/libs/program_options/build/gcc-4.8.3/release/link-static/threading-multi/libboost_program_options.a


static struct addrinfo* udp_resolve_host( const char *hostname, int port, int type, int family, int flags )
{
    struct addrinfo hints, *res = 0;
    int error;
    char sport[16];
    const char *node = 0, *service = "0";

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

int	main(int argc, char **argv)
{
	struct in_addr localInterface;
    struct sockaddr_in groupSock;
    int sd_out;
    char databuf_out[16384] = "Multicast test message lol!";
    int datalen_out = sizeof(databuf_out);
	struct sockaddr_in localSock;
    struct ip_mreq group;
    int sd_in;
    int datalen_in;
    char databuf_in[16384];
	
	static std::string s_ingroup;
    static int  s_inport;
    static std::string s_inip;
	
	static std::string s_outgroup; 
    static int  s_outport;
    static std::string s_outip;
	
	struct sockaddr_storage my_addr;
    struct addrinfo *res0 = 0;
    int addr_len;
	
	boost::program_options::options_description description("Option");
	boost::program_options::variables_map boost_map;
	
	description.add_options()
	("ingroup", boost::program_options::value<std::string>(&s_ingroup), "Group Entré")
	("inip", boost::program_options::value<std::string>(&s_inip), "Ip Entré")
	("inport", boost::program_options::value<int>(&s_inport), "Port Entré")
	("outgroup", boost::program_options::value<std::string>(&s_outgroup), "Group Sortie")
	("outip", boost::program_options::value<std::string>(&s_outip), "Ip Sortie")
	("outport", boost::program_options::value<int>(&s_outport), "Port Sortie")
	("help", "Help Screen");
	
	boost::program_options::store(boost::program_options::parse_command_line(argc, argv, description), boost_map);
	boost::program_options::notify(boost_map);
	
	if (boost_map.count("help"))
      std::cout << description << std::endl;
	
	res0 = udp_resolve_host( 0, s_outport, SOCK_DGRAM, AF_INET, AI_PASSIVE );
     if( res0 == 0 ) {
		 std::cerr << "udp_resolve_host failed" << std::endl;
		exit(1);
     }
     memcpy( &my_addr, res0->ai_addr, res0->ai_addrlen );
     addr_len = res0->ai_addrlen;
     freeaddrinfo( res0 );


    /* Create a datagram socket on which to send. */
    sd_out = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd_out < 0) {
      std::cerr << "Opening datagram socket error" << std::endl;
      exit(1);
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
       exit(1);
      } 
    }
  /* bind  */
  if( bind( sd_out, (struct sockaddr *)&my_addr, addr_len ) < 0 ) {
      std::cerr << "Error binding out socket" << std::endl;
      exit(1);
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
      exit(1);
    } else {
      std::cout << "Setting the local out interface...OK\n" << std::endl;
    }
    unsigned char ttl = 32;
    if( setsockopt( sd_out, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl) ) < 0 ) {   
      std::cerr << "Setting ttl" << std::endl;
      exit(1);
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
      exit(1);
    } else {
      std::cout << "Opening datagram socket....OK." << std::endl;
      /* Enable SO_REUSEADDR to allow multiple instances of this */
      /* application to receive copies of the multicast datagrams. */
      {
      int reuse = 1;
      if(setsockopt(sd_in, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
       std::cerr << "Setting SO_REUSEADDR error" << std::endl;
       close(sd_in);
       exit(1);
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
      exit(1);
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
      exit(1);
    } else {
      std::cout << "Adding multicast group in...OK." << std::endl;
    }

    /* Read from the socket. */
    datalen_in = sizeof(databuf_in);

    while(1) {
      datalen_out = read(sd_in, databuf_in, datalen_in); 
      if(datalen_out < 0) {
        std::cerr << "Reading datagram im message error" << std::endl;
        close(sd_in);
        close(sd_out);
        exit(1);
      } else {
        //printf("Reading datagram message in ...OK.\n");
        std::cout << "r";
    //    printf("The message from multicast server in is: \"%s\"\n", databuf_in);
      }
      if(sendto(sd_out, databuf_in, datalen_out, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
         std::cerr << "Sending datagram message out error" << std::endl;
      } else {
        //printf("Sending datagram message out...OK\n");
        std::cout << "w";
      }

    }
}