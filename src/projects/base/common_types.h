//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <any>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>

#include "base/ovlibrary/string.h"
#include "base/mediarouter/media_type.h"

#define MAX_FRAG_COUNT 20

enum class StreamSourceType : int8_t
{
	Ovt,
	Rtmp,
	Rtsp,
	RtspPull,
	Mpegts,
	Transcoder,
};

enum class ProviderStreamDirection : int8_t
{
	Pull,
	Push
};

enum class ProviderType : int8_t
{
	Unknown,
	Rtmp,
	Rtsp,
	RtspPull,
	Ovt,
	Mpegts,
};

enum class PublisherType : int8_t
{
	Unknown,
	Webrtc,
	Rtmp,
	RtmpPush,
	Hls,
	Dash,
	LlDash,
	Ovt,
	File,
	NumberOfPublishers,
};

enum class FrameType : int8_t
{
	EmptyFrame,
	AudioFrameKey,
	AudioFrameDelta,
	AudioFrameSpeech,
	AudioFrameCN,  // Comfort Noise, https://tools.ietf.org/html/rfc3389
	VideoFrameKey,
	VideoFrameDelta,
};

struct FragmentationHeader
{
public:
	// Offset of pointer to data for each
	std::vector<size_t> fragmentation_offset;

	// fragmentation
	// Data size for each fragmentation
	std::vector<size_t> fragmentation_length;
	// Timestamp difference relative "now" for each fragmentation
	std::vector<uint16_t> fragmentation_time_diff;
	// Payload type of each fragmentation
	std::vector<uint8_t> fragmentation_pl_type;

	// Currently only used for RTSP Provider only
	bool last_fragment_complete = false;

	size_t GetCount() const
	{
		OV_ASSERT2(fragmentation_offset.size() == fragmentation_length.size());

		return std::min(fragmentation_offset.size(), fragmentation_length.size());
	}

	bool operator==(const FragmentationHeader &other) const
	{
		return fragmentation_offset == other.fragmentation_offset && fragmentation_length == other.fragmentation_length && fragmentation_time_diff == other.fragmentation_time_diff && fragmentation_pl_type == other.fragmentation_pl_type && last_fragment_complete == other.last_fragment_complete;
	}

	bool operator!=(const FragmentationHeader &other)
	{
		return !(other == *this);
	}

	void Clear()
	{
		fragmentation_offset.clear();
		fragmentation_length.clear();
		fragmentation_time_diff.clear();
		fragmentation_pl_type.clear();
		last_fragment_complete = false;
	}

	bool IsEmpty() const
	{
		return fragmentation_offset.empty() && fragmentation_length.empty();
	}

	ov::Data Serialize() const
	{
		ov::Data fragmentation_header_data;
		ov::Serialize(fragmentation_header_data, fragmentation_offset);
		ov::Serialize(fragmentation_header_data, fragmentation_length);
		ov::Serialize(fragmentation_header_data, fragmentation_time_diff);
		ov::Serialize(fragmentation_header_data, fragmentation_pl_type);
		fragmentation_header_data.Append(&last_fragment_complete, sizeof(last_fragment_complete));
		return fragmentation_header_data;
	}

	bool Deserialize(ov::Data &data, size_t &bytes_consumed)
	{
		auto *bytes = reinterpret_cast<const uint8_t *>(data.GetData());
		auto length = data.GetLength();
		bool deserialized = ov::Deserialize(bytes, length, fragmentation_offset, bytes_consumed) && ov::Deserialize(bytes, length, fragmentation_length, bytes_consumed) && ov::Deserialize(bytes, length, fragmentation_time_diff, bytes_consumed) && ov::Deserialize(bytes, length, fragmentation_pl_type, bytes_consumed);
		if (deserialized && length >= sizeof(last_fragment_complete))
		{
			last_fragment_complete = *reinterpret_cast<const decltype(last_fragment_complete) *>(bytes);
			bytes_consumed += sizeof(last_fragment_complete);
			return true;
		}
		return false;
	}
};

struct EncodedFrame
{
public:
	EncodedFrame(std::shared_ptr<ov::Data> buffer, size_t length, size_t size)
		: _buffer(buffer), _length(length), _size(size)
	{
	}

	int32_t _encoded_width = 0;
	int32_t _encoded_height = 0;

	int64_t _time_stamp = 0LL;
	int64_t _duration = 0LL;

	FrameType _frame_type = FrameType::VideoFrameDelta;
	std::shared_ptr<ov::Data> _buffer;
	size_t _length = 0;
	size_t _size = 0;
	bool _complete_frame = false;
};

struct CodecSpecificInfoGeneric
{
	uint8_t simulcast_idx = 0;
};

enum class H26XPacketizationMode
{
	NonInterleaved = 0,	 // Mode 1 - STAP-A, FU-A is allowed
	SingleNalUnit		 // Mode 0 - only single NALU allowed
};

struct CodecSpecificInfoH26X
{
	H26XPacketizationMode packetization_mode = H26XPacketizationMode::NonInterleaved;
	uint8_t simulcast_idx = 0;
};

struct CodecSpecificInfoVp8
{
	int16_t picture_id = -1;  // Negative value to skip pictureId.
	bool non_reference = false;
	uint8_t simulcast_idx = 0;
	uint8_t temporal_idx = 0;
	bool layer_sync = false;
	int tl0_pic_idx = -1;  // Negative value to skip tl0PicIdx.
	int8_t key_idx = -1;   // Negative value to skip keyIdx.
};

struct CodecSpecificInfoOpus
{
	int sample_rate_hz = 0;
	size_t num_channels = 0;
	int default_bitrate_bps = 0;
	int min_bitrate_bps = 0;
	int max_bitrate_bps = 0;
};

union CodecSpecificInfoUnion
{
	CodecSpecificInfoGeneric generic;
	CodecSpecificInfoVp8 vp8;
	CodecSpecificInfoH26X h26X;
	// In the future
	// RTPVideoHeaderVP9 vp9;
	CodecSpecificInfoOpus opus;
};

struct CodecSpecificInfo
{
	common::MediaCodecId codec_type = common::MediaCodecId::None;
	const char *codec_name = nullptr;
	CodecSpecificInfoUnion codec_specific = {0};
};

static ov::String StringFromStreamSourceType(const StreamSourceType &type)
{
	switch (type)
	{
		case StreamSourceType::Ovt:
			return "Ovt";
		case StreamSourceType::Rtmp:
			return "Rtmp";
		case StreamSourceType::Rtsp:
			return "Rtsp";
		case StreamSourceType::RtspPull:
			return "RtspPull";
		case StreamSourceType::Transcoder:
			return "Transcoder";
		default:
			return "Unknown";
	}
}

static ov::String StringFromProviderType(const ProviderType &type)
{
	switch (type)
	{
		case ProviderType::Unknown:
			return "Unknown";
		case ProviderType::Rtmp:
			return "RTMP";
		case ProviderType::Rtsp:
			return "RTSP";
		case ProviderType::RtspPull:
			return "RTSP Pull";
		case ProviderType::Ovt:
			return "OVT";
		case ProviderType::Mpegts:
			return "MPEG-TS";
	}

	return "Unknown";
}

static ov::String StringFromPublisherType(const PublisherType &type)
{
	switch (type)
	{
		case PublisherType::Unknown:
		case PublisherType::NumberOfPublishers:
			return "Unknown";
		case PublisherType::Webrtc:
			return "WebRTC";
		case PublisherType::Rtmp:
			return "RTMP";
		case PublisherType::RtmpPush:
			return "RTMPPush";
		case PublisherType::Hls:
			return "HLS";
		case PublisherType::Dash:
			return "DASH";
		case PublisherType::LlDash:
			return "LLDASH";
		case PublisherType::Ovt:
			return "Ovt";
		case PublisherType::File:
			return "File";
	}

	return "Unknown";
}

static ov::String StringFromMediaCodecId(const common::MediaCodecId &type)
{
	switch (type)
	{
		case common::MediaCodecId::H264:
			return "H264";
		case common::MediaCodecId::H265:
			return "H265";
		case common::MediaCodecId::Vp8:
			return "VP8";
		case common::MediaCodecId::Vp9:
			return "VP9";
		case common::MediaCodecId::Flv:
			return "FLV";
		case common::MediaCodecId::Aac:
			return "AAC";
		case common::MediaCodecId::Mp3:
			return "MP3";
		case common::MediaCodecId::Opus:
			return "OPUS";
		case common::MediaCodecId::Jpeg:
			return "JPEG";
		case common::MediaCodecId::Png:
			return "PNG";
		case common::MediaCodecId::None:
		default:
			return "Unknwon";
	}
}

static ov::String StringFromMediaType(const common::MediaType &type)
{
	switch (type)
	{
		case common::MediaType::Video:
			return "Video";
		case common::MediaType::Audio:
			return "Audio";
		case common::MediaType::Data:
			return "Data";
		case common::MediaType::Subtitle:
			return "Subtitle";
		case common::MediaType::Attachment:
			return "Attachment";
		case common::MediaType::Unknown:
		default:
			return "Unknown";
	}
}