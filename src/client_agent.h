#ifndef BROADCASTER_CLIENT_AGENT_H
#define BROADCASTER_CLIENT_AGENT_H

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

enum StreamType {
	ST_Video =0,
	ST_Audio,
	ST_Data
};

class ClientAgent : public webrtc::PeerConnectionObserver,
                    public webrtc::CreateSessionDescriptionObserver,
                    public rtc::VideoSinkInterface<webrtc::VideoFrame>,
                    public webrtc::AudioTrackSinkInterface {
public:
	static rtc::scoped_refptr<ClientAgent> create();
	virtual ~ClientAgent();

	virtual std::string create_offer();
  virtual bool start_stream(std::string &remote_sdp);
  virtual bool enable_stream(StreamType stype, bool enabled);

protected:
	ClientAgent();

	bool init();
  virtual rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> get_factory();
	virtual rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> create_factory();
  virtual rtc::scoped_refptr<webrtc::AudioTrackInterface> create_audio_track();
  virtual rtc::scoped_refptr<webrtc::VideoTrackInterface> create_video_track();

	std::string merge_ice(std::string &sdp);
	int get_local_tracks();

protected:
	//access private fields
  webrtc::PeerConnectionInterface* pc() const { return pc_.get(); }
  webrtc::PeerConnectionFactoryInterface *factory() const { return factory_.get(); }
  rtc::Thread* signal_thread() const { return signal_thread_; }
	std::future<std::string> sdp_future() { return sdp_promise_.get_future(); }
	std::future<bool> ice_future() { return ice_promise_.get_future(); }
	const std::map<int, std::string>& ice() { return ice_; };


protected:
  // PeerConnectionObserver implementation.
  virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
  virtual void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
  virtual void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
  virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
  virtual void OnRenegotiationNeeded() override;
  virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  virtual void OnIceConnectionReceivingChange(bool receiving) override;

  // CreateSessionDescriptionObserver implementation.
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  virtual void OnFailure(webrtc::RTCError error) override;

	//VideoSinkInterface
  virtual void OnFrame(const webrtc::VideoFrame& frame) override {}
  //AudioTrackSinkInterface
  virtual void OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels, size_t number_of_frames) override {}

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
#endif // BROADCASTER_CLIENT_AGENT_H
