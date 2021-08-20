#include "publisher.h"

namespace webrtc {

rtc::scoped_refptr<Publisher> Publisher::create()
{
  rtc::scoped_refptr<Publisher> pub(new rtc::RefCountedObject<Publisher>());
  if(!pub->init()) {
    pub = rtc::scoped_refptr<Publisher>();
  }
  return pub;
}

Publisher::Publisher()
: ClientAgent()
{

}

Publisher::~Publisher()
{

}

std::string Publisher::create_offer()
{
  return ClientAgent::create_offer();
}

bool Publisher::start_stream(std::string &remote_sdp)
{
  return ClientAgent::start_stream(remote_sdp);
}

}