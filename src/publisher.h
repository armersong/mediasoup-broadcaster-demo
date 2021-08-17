#ifndef BROADCASTER_PUBLISHER_H
#define BROADCASTER_PUBLISHER_H

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <future>

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/create_peerconnection_factory.h"
#include "api/scoped_refptr.h"

namespace webrtc {

class Publisher: public webrtc::PeerConnectionObserver,
                 public webrtc::CreateSessionDescriptionObserver,
                 public webrtc::AudioTrackSinkInterface {
public:
	static rtc::scoped_refptr<Publisher> create();
	virtual ~Publisher();

	std::string create_offer();
	bool start_stream(std::string &remote_sdp);

protected:
	Publisher();

	bool init();
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> get_factory();
  rtc::scoped_refptr<webrtc::AudioTrackInterface> create_audio_track();
  rtc::scoped_refptr<webrtc::VideoTrackInterface> create_video_track();

	std::string merge_ice(std::string &sdp);

protected:
  //
  // PeerConnectionObserver implementation.
  //

  void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
  void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
  void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override;

  // CreateSessionDescriptionObserver implementation.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;

  // audio
  virtual void OnData(const void* audio_data,
                      int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames) override;

private:
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;
  rtc::Thread* signal_thread_;

  std::promise<std::string> sdp_promise_;
  std::promise<bool> ice_promise_;
	std::map<int, std::string> ice_;
	int srflx_count_;
};

}
#endif // BROADCASTER_PUBLISHER_H
