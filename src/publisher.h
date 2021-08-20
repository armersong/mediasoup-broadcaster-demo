#ifndef BROADCASTER_PUBLISHER_H
#define BROADCASTER_PUBLISHER_H

#include "client_agent.h"

namespace webrtc {

class Publisher: public ClientAgent {
public:
  static rtc::scoped_refptr<Publisher> create();
  virtual ~Publisher();

  virtual std::string create_offer();
  virtual bool start_stream(std::string &remote_sdp);

protected:
  Publisher();

};

}

#endif // BROADCASTER_PUBLISHER_H
