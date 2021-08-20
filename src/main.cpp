#include <cpr/cpr.h>
#include <csignal> // sigsuspend()
#include <cstdlib>
#include <iostream>
#include <rtc_base/ssl_adapter.h>
#include <string>

#include <stdlib.h>
#include <json.hpp>

#include "publisher.h"
#include "player.h"

using json = nlohmann::json;
using namespace webrtc;

void signalHandler(int signum)
{
	std::cout << "[INFO] interrupt signal (" << signum << ") received" << std::endl;
	std::cout << "[INFO] leaving!" << std::endl;
	std::exit(signum);
}

void start_publish(std::string &server_url, std::string &stream_id)
{
  rtc::scoped_refptr<Publisher> pub = Publisher::create();
  do {
    if(!pub) {
      std::cout<<"create publisher failed"<<std::endl;
      break;
    }
    auto sdp = pub->create_offer();
    std::cout<<"sdp: \n"<< sdp << std::endl;

    json body = {
      { "api",   server_url },
      { "sdp", sdp },
      { "tid", "40b4c8e"},
      { "streamurl",  std::string("webrtc://d.ossrs.net/live/") + stream_id }
    };

    //send to server to get answer
    auto r = cpr::PostAsync(
      cpr::Url{ server_url },
      cpr::Body{ body.dump() },
      cpr::Header{ { "Content-Type", "application/json" } })
      .get();

    if (r.status_code != 200) {
      std::cerr << "[ERROR] unable to create mediasoup recv WebRtcTransport"
                << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;
      break;
    }
    auto response = json::parse(r.text);
    if (response.find("sdp") == response.end()) {
      std::cerr << "[ERROR] 'sdp' missing in response" << std::endl;
      break;
    }
    auto answer_sdp = response["sdp"].get<std::string>();

    std::cout << "[INFO] answer sdp: " <<answer_sdp<< std::endl;
    pub->start_stream(answer_sdp);

    std::cout << "[INFO] press Ctrl+C or Cmd+C to leave..." << std::endl;

    while (true){
      int c = std::cin.get();
      if( c == 'q') {
        break;
      }
    }
  } while(false);
}

void start_player(std::string &server_url, std::string &stream_id)
{
  rtc::scoped_refptr<Player> client = Player::create();
  do {
    if(!client) {
      std::cout<<"create player failed"<<std::endl;
      break;
    }
    auto sdp = client->create_offer();
    std::cout<<"sdp: \n"<< sdp << std::endl;

    json body = {
      { "api",   server_url },
      { "sdp", sdp },
      { "tid", "40b4c8e"},
      { "streamurl",  std::string("webrtc://d.ossrs.net/live/") + stream_id }
    };

    //send to server to get answer
    auto r = cpr::PostAsync(
      cpr::Url{ server_url },
      cpr::Body{ body.dump() },
      cpr::Header{ { "Content-Type", "application/json" } })
      .get();

    if (r.status_code != 200) {
      std::cerr << "[ERROR] unable to create mediasoup recv WebRtcTransport"
                << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;
      break;
    }
    auto response = json::parse(r.text);
    if (response.find("sdp") == response.end()) {
      std::cerr << "[ERROR] 'sdp' missing in response" << std::endl;
      break;
    }
    auto answer_sdp = response["sdp"].get<std::string>();

    std::cout << "[INFO] answer sdp: " <<answer_sdp<< std::endl;
    client->start_stream(answer_sdp);

    std::cout << "[INFO] press Ctrl+C or Cmd+C to leave..." << std::endl;

    while (true){
      int c = std::cin.get();
      if( c == 'q') {
        break;
      }
    }
  } while(false);
}

int main(int /*argc*/, char* /*argv*/[])
{
	// Register signal SIGINT and signal handler.
	signal(SIGINT, signalHandler);

	// Retrieve configuration from environment variables.
	const char* env_server_url    = std::getenv("SERVER_URL");
	const char* env_stream_id       = std::getenv("STREAM_ID");
	const char* env_mode = std::getenv("MODE");

  int mode = mode ? atoi(env_mode) : 0;
  mode = mode == 1 ? 1 : 0;
  std::string server_url = env_server_url ? env_server_url : std::string("http://d.ossrs.net:1985/rtc/v1/") + (mode == 0 ? "publish/" : "play/");
  std::string stream_id = env_stream_id ? env_stream_id : "broadcaster";

	std::cout<<"server    :"<<server_url<< std::endl;
  std::cout<<"stream_id :"<<stream_id<< std::endl;

  rtc::InitializeSSL();
  rtc::InitRandom(rtc::Time());
	std::cout << "[INFO] welcome to mediasoup broadcaster app!\n" << std::endl;
	if(mode == 1) {
		start_player(server_url, stream_id);
	} else {
		start_publish(server_url, stream_id);
	}

	std::cout <<"done" << std::endl;
	return 0;
}
