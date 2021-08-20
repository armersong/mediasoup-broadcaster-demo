#ifndef BROADCASTER_PLAYER_H
#define BROADCASTER_PLAYER_H

#include "client_agent.h"

namespace webrtc {

class Player: public ClientAgent {
public:
  static rtc::scoped_refptr<Player> create();
  virtual ~Player();

  virtual std::string create_offer();
  virtual bool start_stream(std::string &remote_sdp);

protected:
  Player();

protected:
  virtual rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> create_factory() override;

protected:
  // PeerConnectionObserver implementation.
  virtual void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
                          const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
  virtual void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
  virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;

	// VideoSinkInterface
	virtual void OnFrame(const webrtc::VideoFrame& frame) override;

  // audio
  virtual void OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels, size_t number_of_frames) override;

private:
	unsigned long video_frames_;
  unsigned long audio_frames_;
};

}
#endif // BROADCASTER_PLAYER_H
