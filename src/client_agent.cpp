#include "client_agent.h"

#include <set>

#include "absl/memory/memory.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_sender_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "p2p/base/port_allocator.h"
#include "pc/video_track_source.h"
#include <pc/test/fake_audio_capture_module.h>
#include <pc/test/fake_periodic_video_track_source.h>
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "test/vcm_capturer.h"

namespace webrtc {

class DummySetSessionDescriptionObserver
  : public webrtc::SetSessionDescriptionObserver {
public:
  static DummySetSessionDescriptionObserver* Create() {
    return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
  }
  virtual void OnSuccess() { RTC_LOG(INFO) << "DummySetSessionDescriptionObserver::"<<__FUNCTION__; }
  virtual void OnFailure(webrtc::RTCError error) {
    RTC_LOG(INFO) << "DummySetSessionDescriptionObserver::"<<__FUNCTION__ << " " << ToString(error.type()) << ": "
                  << error.message();
  }
};

class CapturerTrackSource : public webrtc::VideoTrackSource {
public:
  static rtc::scoped_refptr<CapturerTrackSource> Create() {
    const size_t kWidth = 640;
    const size_t kHeight = 480;
    const size_t kFps = 30;
    RTC_LOG(INFO) << __FUNCTION__<<" CapturerTrackSource::Create";
    std::unique_ptr<webrtc::test::VcmCapturer> capturer;
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
      return nullptr;
    }
    int num_devices = info->NumberOfDevices();
    for (int i = 0; i < num_devices; ++i) {
      capturer = absl::WrapUnique(webrtc::test::VcmCapturer::Create(kWidth, kHeight, kFps, i));
      if (capturer) {
        return new rtc::RefCountedObject<CapturerTrackSource>(std::move(capturer));
      }
    }

    return nullptr;
  }

protected:
  explicit CapturerTrackSource(
    std::unique_ptr<webrtc::test::VcmCapturer> capturer)
    : VideoTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}

private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
    return capturer_.get();
  }
  std::unique_ptr<webrtc::test::VcmCapturer> capturer_;
};


rtc::scoped_refptr<ClientAgent> ClientAgent::create()
{
  rtc::scoped_refptr<ClientAgent> agent(new rtc::RefCountedObject<ClientAgent>());
	if(!agent->init()) {
		agent = rtc::scoped_refptr<ClientAgent>();
	}
	return agent;
}

ClientAgent::ClientAgent()
:pc_(nullptr), signal_thread_(nullptr), srflx_count_(0)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

ClientAgent::~ClientAgent()
{
  RTC_LOG(INFO) <<__FUNCTION__<<" >>>";
	pc_->Close();

  RTC_LOG(INFO) <<__FUNCTION__<<" free pc_";
	if(signal_thread_) {
		if(signal_thread_->IsCurrent()) {
			pc_ = nullptr;
		} else {
      signal_thread_->Invoke<void>(RTC_FROM_HERE, [this] {
				this->pc_ = nullptr;
      });
		}
    RTC_DCHECK(! signal_thread_->IsCurrent());
  } else {
    pc_ = nullptr;
	}

  RTC_LOG(INFO) <<__FUNCTION__<<" free factory_";
  factory_ = nullptr;

  RTC_LOG(INFO) <<__FUNCTION__<<" stop signal_thread_";
  if(signal_thread_) {
    signal_thread_->Quit();
  }

  RTC_LOG(INFO) <<__FUNCTION__<<" free signal_thread_";
  if(signal_thread_) {
    delete signal_thread_;
  }

  RTC_LOG(INFO) <<__FUNCTION__<<" <<<";
}

bool ClientAgent::init()
{
  RTC_LOG(INFO) <<__FUNCTION__;
	if(!factory_.get()) {
    factory_ = this->get_factory();
    if(!factory_.get()) {
      RTC_LOG(INFO) <<__FUNCTION__<<" create factory failed";
      return false;
    }
	}
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  config.enable_dtls_srtp = true;
  webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:stun.l.google.com:19302";
  config.servers.push_back(server);

  pc_ = factory_->CreatePeerConnection(config, nullptr, nullptr, this);
	return true;
}

std::string ClientAgent::create_offer()
{
  RTC_LOG(INFO) <<__FUNCTION__;
	auto audio_track = create_audio_track();
	pc_->AddTrack(audio_track, {"audio"});
  auto video_track = create_video_track();
	pc_->AddTrack(video_track, {"video"});
  PeerConnectionInterface::RTCOfferAnswerOptions answer_opts;
	auto f = sdp_promise_.get_future();
  pc_->CreateOffer(this,answer_opts);
  srflx_count_ = 0;
  auto ice_f = ice_promise_.get_future();
	auto sdp = f.get();
  RTC_LOG(INFO) <<__FUNCTION__<<" offer sdp: "<<sdp;
  auto result = ice_f.get();
	return merge_ice(sdp);
}

std::string ClientAgent::merge_ice(std::string &sdp)
{
	std::string res = "";
	std::string::size_type pos = sdp.find("m=audio");
	if(pos != std::string::npos) {
		res.append(std::string(sdp.c_str(), pos));
    std::string::size_type pos1 = sdp.find("m=video");
    if(pos1 != std::string::npos) {
			res.append(std::string(sdp.c_str()+pos, pos1-pos));
			std::map<int, std::string>::iterator it = ice_.find(0);
			if(it != ice_.end()) {
				res.append(it->second);

				res.append(sdp.c_str() + pos1, sdp.size() - pos1);
				it = ice_.find(1);
				if(it != ice_.end()) {
					res.append(it->second);
					return res;
				}
			}
		}
	}
	return res;
}

int ClientAgent::get_local_tracks()
{
	std::set<std::string> track_ids;
	auto sends = pc_->GetSenders();
  std::vector<rtc::scoped_refptr<RtpSenderInterface>>::iterator sit = sends.begin();
	for(; sit != sends.end(); sit++) {
    rtc::scoped_refptr<RtpSenderInterface> i = *sit;
    MediaStreamTrackInterface *track = i->track().release();
		if(track) {
      track_ids.insert(track->kind());
		}
	}

  auto recvs = pc_->GetReceivers();
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>>::iterator rit = recvs.begin();
  for(; rit != recvs.end(); rit++) {
    rtc::scoped_refptr<RtpReceiverInterface> i = *rit;
    MediaStreamTrackInterface *track = i->track().release();
    if(track) {
      track_ids.insert(track->kind());
    }
  }

	return track_ids.size();
}

bool ClientAgent::start_stream(std::string &remote_sdp)
{
  webrtc::SdpType type = webrtc::SdpType::kAnswer;
  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
    webrtc::CreateSessionDescription(type, remote_sdp, &error);
  if (!session_description) {
    RTC_LOG(WARNING) << "Can't parse received session description message. "
                        "SdpParseError was: "
                     << error.description;
    return false;
  }
  RTC_LOG(INFO) << __FUNCTION__<<" SetRemoteDescription ";
  pc_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description.release());
  if (type == webrtc::SdpType::kOffer) {
    RTC_LOG(INFO) << __FUNCTION__<<" CreateAnswer ";
    pc_->CreateAnswer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  }
	return true;
}

bool ClientAgent::enable_stream(StreamType stype, bool enabled)
{
	bool ret = false;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> receivers = pc_->GetReceivers();
  for (const auto& receiver : receivers) {
    rtc::scoped_refptr<MediaStreamTrackInterface> track = receiver->track();
    if(!track) {
			continue;
		}
		if(stype == ST_Video && receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track.get());
			video_track->RemoveSink(this);
			if(enabled) {
        video_track->AddOrUpdateSink(this, rtc::VideoSinkWants());
			}
		} else if(stype == ST_Audio && receiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      auto* audio_track = static_cast<webrtc::AudioTrackInterface*>(track.get());
			audio_track->RemoveSink(this);
			if(enabled) {
				audio_track->AddSink(this);
			}

		} else if(stype == ST_Data && receiver->media_type() == cricket::MEDIA_TYPE_DATA) {
			continue;
		} else {
			continue;
		}

    track->set_enabled(enabled);
    ret = true;
		break;
  }
	return ret;
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> ClientAgent::get_factory()
{
	if(!signal_thread_) {
    signal_thread_   = rtc::Thread::Create().release();
    signal_thread_->SetName("signaling_thread", nullptr);
    if (!signal_thread_->Start()) {
      RTC_LOG(INFO) <<__FUNCTION__<<" thread start errored";
    }
	}
	return create_factory();
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> ClientAgent::create_factory()
{
  webrtc::PeerConnectionInterface::RTCConfiguration config;

  auto fakeAudioCaptureModule = FakeAudioCaptureModule::Create();
  if (!fakeAudioCaptureModule)
  {
    RTC_LOG(INFO) <<__FUNCTION__<<" audio capture module creation errored";
  }

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory = webrtc::CreatePeerConnectionFactory(
    nullptr,
    nullptr,
    signal_thread_,
    fakeAudioCaptureModule,
    webrtc::CreateBuiltinAudioEncoderFactory(),
    webrtc::CreateBuiltinAudioDecoderFactory(),
    webrtc::CreateBuiltinVideoEncoderFactory(),
    webrtc::CreateBuiltinVideoDecoderFactory(),
    nullptr /*audio_mixer*/,
    nullptr /*audio_processing*/);

  if (!factory){
    RTC_LOG(INFO) <<__FUNCTION__<<" error ocurred creating peerconnection factory";
  }
  return factory;
}

rtc::scoped_refptr<webrtc::AudioTrackInterface> ClientAgent::create_audio_track()
{
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
    factory_->CreateAudioTrack("audio",factory_->CreateAudioSource(
        cricket::AudioOptions())));
	return audio_track;

//  cricket::AudioOptions options;
//  options.highpass_filter = false;
//  rtc::scoped_refptr<webrtc::AudioSourceInterface> source = factory_->CreateAudioSource(options);
//  return factory_->CreateAudioTrack("audio", source);
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> ClientAgent::create_video_track()
{
  auto video_device = signal_thread_->Invoke<rtc::scoped_refptr<CapturerTrackSource>>(RTC_FROM_HERE, [this] {
    return CapturerTrackSource::Create();
  });

//  rtc::scoped_refptr<CapturerTrackSource> video_device = CapturerTrackSource::Create();
	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(factory_->CreateVideoTrack("video", video_device));
	return video_track_;

//  auto* videoTrackSource =
//    new rtc::RefCountedObject<webrtc::FakePeriodicVideoTrackSource>(false /* remote */);
//  return factory_->CreateVideoTrack(rtc::CreateRandomUuid(), videoTrackSource);
//  return factory_->CreateVideoTrack("video", videoTrackSource);
}

void ClientAgent::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

void ClientAgent::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
                const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

void ClientAgent::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

void ClientAgent::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

void ClientAgent::OnRenegotiationNeeded()
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

void ClientAgent::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

void ClientAgent::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
  RTC_LOG(INFO) <<__FUNCTION__<<" new_state "<<new_state;
  if(new_state == PeerConnectionInterface::kIceGatheringNew) {
    ice_.clear();
  } else if (new_state == PeerConnectionInterface::kIceGatheringComplete) {
		if(srflx_count_ >= 0) {
      ice_promise_.set_value(true);
		}
    ice_.clear();
  }
}

void ClientAgent::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
  std::string candidateStr;
  candidate->ToString(&candidateStr);
  RTC_LOG(INFO) <<__FUNCTION__<<" candidate "<<candidate->sdp_mline_index()<<":"<<candidateStr;
	std::string sdp = std::string("a=")+ candidateStr + "\r\n";
	std::map<int, std::string>::iterator it = ice_.find(candidate->sdp_mline_index());
	if(it != ice_.end()) {
		it->second.append(sdp);
	} else {
		ice_[candidate->sdp_mline_index()] = sdp;
	}
	if(srflx_count_ >=0 && candidate->candidate().type() == "stun") {
		srflx_count_ ++;
    RTC_LOG(INFO) <<__FUNCTION__<<" stun address "<<srflx_count_;
	}
	if(srflx_count_ >= get_local_tracks()) {
    ice_promise_.set_value(true);
		srflx_count_ = -1;
	}
}

void ClientAgent::OnIceConnectionReceivingChange(bool receiving)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

// CreateSessionDescriptionObserver implementation.
void ClientAgent::OnSuccess(webrtc::SessionDescriptionInterface* desc)
{
  pc_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);
  std::string sdp;
  desc->ToString(&sdp);
  sdp_promise_.set_value(sdp);
}

void ClientAgent::OnFailure(webrtc::RTCError error)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

}