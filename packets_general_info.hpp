
class Packet_info
{
	public:
	Packet_info()
	{
		this->packets_read = 0;
		this->octets_read = 0;
		this->max_interval_value_between_packets = 0;
		this->max_interval_value_between_pcr = 0;
		this->counter000 = 0;
		this->counter001 = 0;
	};
	~Packet_info(){};
	
	unsigned long long int	packets_read;
	unsigned long long int	octets_read;
	unsigned long long int	max_interval_value_between_packets;
	unsigned long long int	max_interval_value_between_pcr;
	int counter000;
	int counter001;
};