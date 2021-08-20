#include "player.h"

#include <memory>

#include <absl/strings/match.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include <modules/video_coding/codecs/vp8/include/vp8.h>
#include <modules/video_coding/codecs/vp9/include/vp9.h>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class AudioDecoderFactoryForPlayer{
public:
	static rtc::scoped_refptr<AudioDecoderFactory> create() {
    return webrtc::CreateBuiltinAudioDecoderFactory();
	}
};

class H264VideoBuffer : public webrtc::VideoFrameBuffer {
public:
  H264VideoBuffer(const uint8_t *data, int len)
	: width_(640), height_(480), data_len_(len){
    data_ = new uint8_t[len];
		memcpy(data_, data, len);
	}

protected:
	virtual ~H264VideoBuffer() {
    if(data_) {
			delete[] data_;
		}
	}

public:
	virtual uint8_t* data() const { return data_; }
	virtual int size() const { return data_len_; }

public:
	//inherit VideoFrameBuffer
  virtual Type type() const { return VideoFrameBuffer::Type::kNative; }

  // The resolution of the frame in pixels. For formats where some planes are
  // subsampled, this is the highest-resolution plane.
  virtual int width() const {	return width_; }

  virtual int height() const { return height_; }

  // Returns a memory-backed frame buffer in I420 format. If the pixel data is
  // in another format, a conversion will take place. All implementations must
  // provide a fallback to I420 for compatibility with e.g. the internal WebRTC
  // software encoders.
  virtual rtc::scoped_refptr<I420BufferInterface> ToI420() { return nullptr; }

  // GetI420() methods should return I420 buffer if conversion is trivial, i.e
  // no change for binary data is needed. Otherwise these methods should return
  // nullptr. One example of buffer with that property is
  // WebrtcVideoFrameAdapter in Chrome - it's I420 buffer backed by a shared
  // memory buffer. Therefore it must have type kNative. Yet, ToI420()
  // doesn't affect binary data at all. Another example is any I420A buffer.
  virtual const I420BufferInterface* GetI420() { return nullptr; }

  // These functions should only be called if type() is of the correct type.
  // Calling with a different type will result in a crash.
  const I420ABufferInterface* GetI420A() const { return nullptr; }
  const I444BufferInterface* GetI444() const { return nullptr; }
  const I010BufferInterface* GetI010() const { return nullptr; }

private:
  uint8_t * data_;
	int data_len_;
	int width_;
	int height_;
};

class DummyVideoDecoder : public webrtc::VideoDecoder {

public:
  DummyVideoDecoder() : callback_(nullptr) {

	}
	virtual ~DummyVideoDecoder() {

	}
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings, int32_t number_of_cores) override {
    RTC_LOG(LS_WARNING) << "Can't initialize DummyVideoDecoder.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Decode(const webrtc::EncodedImage& input_image, bool missing_frames,int64_t render_time_ms) override {
//    RTC_LOG(LS_WARNING) << "The DummyVideoDecoder doesn't support decoding.";
		if(callback_) {
      rtc::scoped_refptr<H264VideoBuffer> img_buffer = rtc::scoped_refptr<H264VideoBuffer>(
        new rtc::RefCountedObject<H264VideoBuffer>(input_image.data(), input_image.size()));

      auto builder = VideoFrame::Builder()
        .set_video_frame_buffer(img_buffer)
        .set_timestamp_rtp(input_image.Timestamp())
        .set_color_space(input_image.ColorSpace());
      VideoFrame decoded_image = builder.build();
			int qp = 25;  //@TODO
      callback_->Decoded(decoded_image, absl::nullopt, qp);
		}
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override {
//    RTC_LOG(LS_WARNING)<< "Can't register decode complete callback on DummyVideoDecoder.";
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  const char* ImplementationName() const override { return "DummyVideoDecoder"; }

private:
  webrtc::DecodedImageCallback* callback_;
};

class VideoDecoderFactoryForPlayer : public VideoDecoderFactory {

public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<SdpVideoFormat> formats;
    formats.push_back(SdpVideoFormat(cricket::kVp8CodecName));
    for (const SdpVideoFormat& format : SupportedVP9Codecs())
      formats.push_back(format);
    for (const SdpVideoFormat& h264_format : SupportedH264Codecs())
      formats.push_back(h264_format);
    return formats;
	}

  std::unique_ptr<VideoDecoder> CreateVideoDecoder(const SdpVideoFormat& format) override {
    if (!IsFormatSupported(GetSupportedFormats(), format)) {
      RTC_LOG(LS_ERROR) << "Trying to create decoder for unsupported format";
      return nullptr;
    }

    if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName))
      return VP8Decoder::Create();
    if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName))
      return VP9Decoder::Create();
    if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName))
			return create_h264_decoder();
//      return H264Decoder::Create();
//    if (absl::EqualsIgnoreCase(format.name, cricket::kAv1CodecName))
//      return CreateLibaomAv1Decoder();

    return nullptr;
	}

  static std::unique_ptr<VideoDecoderFactory> create() {
    return std::make_unique<VideoDecoderFactoryForPlayer>();
	}

protected:
  std::unique_ptr<VideoDecoder> create_h264_decoder() {
    return std::make_unique<DummyVideoDecoder>();
	}

  static bool IsFormatSupported(
    const std::vector<webrtc::SdpVideoFormat>& supported_formats,
    const webrtc::SdpVideoFormat& format) {
    for (const webrtc::SdpVideoFormat& supported_format : supported_formats) {
      if (cricket::IsSameCodec(format.name, format.parameters,supported_format.name,supported_format.parameters)) {
        return true;
      }
    }
    return false;
  }
};

rtc::scoped_refptr<Player> Player::create()
{
  rtc::scoped_refptr<Player> pub(new rtc::RefCountedObject<Player>());
  if(!pub->init()) {
    pub = rtc::scoped_refptr<Player>();
  }
  return pub;
}

Player::Player()
  : ClientAgent(), video_frames_(0), audio_frames_(0)
{

}

Player::~Player()
{

}

std::string Player::create_offer()
{
  RTC_LOG(INFO) <<__FUNCTION__;
	auto res = pc()->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
	if(!res.ok()) {
    RTC_LOG(INFO) <<__FUNCTION__<<" create audio transceiver failed";
		return "";
	}
  res = pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
	if(!res.ok()) {
    RTC_LOG(INFO) <<__FUNCTION__<<" create video transceiver failed";
    return "";
	}
  PeerConnectionInterface::RTCOfferAnswerOptions answer_opts;
  auto f = sdp_future();
  pc()->CreateOffer(this,answer_opts);
  auto ice_f = ice_future();
  auto sdp = f.get();
  RTC_LOG(INFO) <<__FUNCTION__<<" offer sdp: "<<sdp;
  ice_f.get();
  return merge_ice(sdp);
}

bool Player::start_stream(std::string &remote_sdp)
{
	return ClientAgent::start_stream(remote_sdp);
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> Player::create_factory()
{
  webrtc::PeerConnectionInterface::RTCConfiguration config;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory = webrtc::CreatePeerConnectionFactory(
    nullptr,
    nullptr,
    signal_thread(),
    nullptr,
    webrtc::CreateBuiltinAudioEncoderFactory(),
    AudioDecoderFactoryForPlayer::create(),
    webrtc::CreateBuiltinVideoEncoderFactory(),
    VideoDecoderFactoryForPlayer::create(),
    nullptr /*audio_mixer*/,
    nullptr /*audio_processing*/);

  if (!factory){
    RTC_LOG(INFO) <<__FUNCTION__<<" error ocurred creating peerconnection factory";
  }
  return factory;

}

void Player::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
                             const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
{
  RTC_LOG(INFO) <<__FUNCTION__;
	if(receiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
    RTC_LOG(INFO) <<__FUNCTION__<<" add audio";
    auto* audio_track = static_cast<webrtc::AudioTrackInterface*>(receiver->track().release());
    audio_track->AddSink(this);
	} else if(receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
    RTC_LOG(INFO) <<__FUNCTION__<<" add video";
    auto* video_track = static_cast<webrtc::VideoTrackInterface*>(receiver->track().release());
    video_track->AddOrUpdateSink(this, rtc::VideoSinkWants());
	}
}

void Player::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

void Player::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
{
  RTC_LOG(INFO) <<__FUNCTION__;
}

void Player::OnFrame(const webrtc::VideoFrame& video_frame)
{
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = video_frame.video_frame_buffer();
//  RTC_LOG(INFO) <<__FUNCTION__<<" type "<<buffer->type()<<" size "<<video_frame.size();
	if(buffer->type() == VideoFrameBuffer::Type::kNative) {
    H264VideoBuffer *vb = static_cast<H264VideoBuffer*>(buffer.get());
		const uint8_t* data = vb->data();
#if 0
		static FILE *f = nullptr;
		if(!f) {
			f = fopen("/tmp/a.h264", "wb");
		}
    if(f && video_frames_ < 25 * 100 ) {
      RTC_LOG(INFO) <<__FUNCTION__<<" type "<<buffer->type()<<" frame size "<<video_frame.size()<<" buffer size "<<vb->size()<<" frames "<<video_frames_;
      fwrite(data, 1, vb->size(), f);
    }
#endif
	}
	if(video_frames_ % 25*1 == 0) {
    RTC_LOG(INFO) <<__FUNCTION__<<" type "<<buffer->type()<<" width "<<video_frame.width()<<" height "<<video_frame.height();
	}
  video_frames_++;
}

void Player::OnData(const void* audio_data, int bits_per_sample, int sample_rate,
                         size_t number_of_channels, size_t number_of_frames) {
//  RTC_LOG(INFO) <<__FUNCTION__;
  if(audio_frames_ % 50*1 == 0) {
    RTC_LOG(INFO) <<__FUNCTION__<<" bits "<<bits_per_sample<<" sample rate "<<sample_rate<<" channels "<<number_of_channels<<" number of frames "<<number_of_frames;
	}
  audio_frames_++;
}

}