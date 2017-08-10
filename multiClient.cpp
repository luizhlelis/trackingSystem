#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/log/trivial.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::tcp;

namespace util
{
	static std::random_device rd;
	static std::mt19937 gen(rd());


	struct position_info
	{
    time_t datetime;
		double latitude;
		double longitude;
		int speed;
		bool status;
	};

	void initialize_position(position_info& pos)
	{
		static double initial_latitude = -19.9191;
		static double initial_longitude = -43.9386;
		static std::normal_distribution<> lat_dis(initial_latitude, 0.1);
		static std::normal_distribution<> long_dis(initial_longitude, 0.1);

    pos.datetime = time(NULL);
		pos.speed = 0;
		pos.latitude = lat_dis(gen);
		pos.longitude = long_dis(gen);
	}

	void update_position(position_info& pos)
	{
		static std::uniform_real_distribution<> status_dis(0.0,1.0);
		static std::normal_distribution<> speed_dis(0,1);

    pos.datetime = time(NULL);
		if (status_dis(gen) > 0.05)
		{
			if (pos.status == false)
			{
				pos.speed = 10;
				pos.status = true;
			}
			else
			{
				double delta_speed = speed_dis(gen);
				pos.speed += (unsigned)delta_speed;
				if (pos.speed < 0)
				{
					pos.speed = 0;
					delta_speed = 0;
				}
				pos.latitude += 0.0001*pos.speed;
				pos.longitude += 0.0001*pos.speed;
			}			
		}
		else
		{
			pos.speed = 0;
		}
	}

	std::string generate_random_imei()
	{
		static std::unordered_set<std::string> imeis;
		static std::uniform_int_distribution<> imei_num_dis(1,9);
		std::string imei;
		do
		{
			std::ostringstream oss;
			for (unsigned i =0; i < 6; ++i)
			{
				oss << imei_num_dis(gen);
			}
			imei = oss.str();
		} while(imeis.find(imei) != imeis.end());
		imeis.insert(imei);		
		return(imei);
	}


}

class client :  private boost::noncopyable
{
public:
	client(boost::asio::io_service& io_service, const std::string& ip, const unsigned short port, unsigned id) 
		: socket_(io_service), 
		ip_(ip),
		port_(port),
		id_(id),
		pos_update_timer_(io_service)
	{
		util::initialize_position(pos_);
		imei_ = util::generate_random_imei();
	}
	~client()
	{
		socket_.close();
	}
	void start()
	{
		socket_.async_connect(tcp::endpoint(boost::asio::ip::address::from_string(ip_), port_), 
			boost::bind(&client::handle_connect, this, 
			boost::asio::placeholders::error));
	}
private:
	void handle_connect(boost::system::error_code error)
	{
		if (!error)
		{
			//send_id_msg();
		}	
	}

	void handle_write(boost::system::error_code error, size_t bytes_transferred)
	{
		if (!error)
		{
			start_pos_update_timer();
		}
	}


	void send_pos_msg()
	{
		std::ostringstream msg;
    msg << "GET /?id=" << imei_ 
      << "&timestamp=" << pos_.datetime 
      << "&lat=" << pos_.latitude
      << "&lon=" << pos_.longitude
      << "&speed=" << pos_.speed
      << "&bearing=" << 0
      << "&altitude=" << 840
      << "&batt=" << 100 << "\r\n"

      << "User-Agent: C++ Client\r\n"
      << "Host: 150.164.35.70:9001\r\n"
      << "Connection: Keep-Alive\r\n"
      << "Accept-Encoding: gzip\r\n\r\n";

		BOOST_LOG_TRIVIAL(info) << "Client id: " << imei_ << " - POS Msg: " << msg.str();

		std::ostream os(&buffer_);
		os << msg.str() << "\n";
		boost::asio::async_write(socket_, buffer_, boost::asio::transfer_all(), 
				boost::bind(&client::handle_write, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
	}

	void handle_timeout(boost::system::error_code error)
	{
		if (!error)
		{
			util::update_position(pos_);
			send_pos_msg();
		}
	}

	void start_pos_update_timer()
	{
		pos_update_timer_.expires_from_now(boost::posix_time::seconds(5));
		pos_update_timer_.async_wait(boost::bind(&client::handle_timeout, this,
			boost::asio::placeholders::error));
	}


	tcp::socket socket_;
	const std::string ip_;
	const unsigned short port_;
	const unsigned id_;
	std::string imei_;
	boost::asio::streambuf buffer_;
	util::position_info pos_;
	boost::asio::deadline_timer pos_update_timer_;
};

typedef std::shared_ptr<client> client_ptr_type;

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 4)
		{
			std::cerr << "Usage: " << argv[0] << " <ip> <port> <num_clients>" << std::endl;
			return(1);
		}
		std::string gateway_ip = argv[1];
		unsigned short gateway_port = (unsigned short)atoi(argv[2]);
		unsigned num_clients = (unsigned)atoi(argv[3]);

		boost::asio::io_service io_service;
		std::vector<client*> clients;
		for (unsigned i =0; i < num_clients; ++i)
		{
			clients.push_back(new client(io_service, gateway_ip, gateway_port, i+1));
			clients.back()->start();
		}
		io_service.run();

		for (unsigned i =0; i < num_clients; ++i)
		{
			delete clients[i];
		}
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << "\n";
	}
	return 0;
}