#include "external_video_encoder_factory.h"
#include "absl/memory/memory.h"
#include "media/engine/internal_decoder_factory.h"
#include "rtc_base/logging.h"
#include "absl/strings/match.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"

#include "nv_encoder.h"
#include "qsv_encoder.h"

namespace webrtc {

	class ExternalEncoderFactory : public webrtc::VideoEncoderFactory
	{
	public:
		std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override
		{
			std::vector<webrtc::SdpVideoFormat> video_formats;
			
#if 0 // Disabld default SdpVideoFormat
			for (const webrtc::SdpVideoFormat& h264_format : webrtc::SupportedH264Codecs())
			{
				video_formats.push_back(h264_format);
			}
#else
			webrtc::SdpVideoFormat bframes_sdp_format = CreateH264Format(H264Profile::kProfileConstrainedBaseline, H264Level::kLevel3_1, "1", false);
			bframes_sdp_format.bframe_enabled = true;
			video_formats.push_back(bframes_sdp_format);
#endif
			return video_formats;
		}

		CodecSupport QueryCodecSupport(const SdpVideoFormat& format, absl::optional<std::string> scalability_mode) const override 
		{
			CodecSupport codec_support;
			if (!scalability_mode) 
			{
				codec_support.is_supported = format.IsCodecInList(GetSupportedFormats());
			}
			return codec_support;
		}

		std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override
		{		
			if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName))
			{
				if (webrtc::H264Encoder::IsSupported())
				{
					if (xop::NvidiaD3D11Encoder::IsSupported())
					{
						return absl::make_unique<webrtc::NvEncoder>(cricket::CreateVideoCodec(format));
					}
					else
					{
						return webrtc::H264Encoder::Create(cricket::CreateVideoCodec(format));
					}
				}
			}

			return nullptr;
		}
	};

	std::unique_ptr<webrtc::VideoEncoderFactory> CreateBuiltinExternalVideoEncoderFactory()
	{
		return absl::make_unique<ExternalEncoderFactory>();
	}

}


