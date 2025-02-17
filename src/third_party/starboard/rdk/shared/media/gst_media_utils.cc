//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#include <memory>
#include <map>
#include <type_traits>

#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/common/log.h"
#include "third_party/starboard/rdk/shared/media/gst_media_utils.h"
#include "third_party/starboard/rdk/shared/log_override.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace media {
namespace {

struct FeatureListDeleter {
  void operator()(GList* p) { gst_plugin_feature_list_free(p); }
};

struct CapsDeleter {
  void operator()(GstCaps* p) { gst_caps_unref(p); }
};

struct BufferDeleter {
  void operator()(GstBuffer* p) { gst_buffer_unref(p); }
};

using UniqueFeatureList = std::unique_ptr<GList, FeatureListDeleter>;
using UniqueCaps = std::unique_ptr<GstCaps, CapsDeleter>;
using UniqueBuffer = std::unique_ptr<GstBuffer, BufferDeleter>;

std::vector<UniqueBuffer>
ParseXiphStreamHeaders (const void* codec_data, gsize codec_data_size) {
  // Based on isomp4/matroska demuxers
  std::vector<UniqueBuffer> res;
  const guint8 *p = reinterpret_cast<const guint8*>(codec_data);
  gint i, offset;
  guint last, num_packets;
  std::vector<guint> length;

  if (codec_data == nullptr || codec_data_size == 0)
    return {};

  num_packets = p[0] + 1;
  // unlikely number of packets
  if (num_packets > 16) {
    GST_WARNING("too many packets in Xiph header.");
    return {};
  }

  length.resize(num_packets);
  last = 0;
  offset = 1;

  // first packets, read length values
  for (i = 0; i < num_packets - 1; i++) {
    length[i] = 0;
    while (offset < codec_data_size) {
      length[i] += p[offset];
      if (p[offset++] != 0xff)
        break;
    }
    last += length[i];
  }

  if (offset + last > codec_data_size) {
    GST_WARNING("Xiph header is out of codec data bounds.");
    return { };
  }

  // last packet is the remaining size
  length[i] = codec_data_size - offset - last;

  for (i = 0; i < num_packets; i++) {
    GstBuffer *header;
    if (offset + length[i] > codec_data_size) {
      GST_WARNING("Xiph header is out of codec data bounds.");
      return { };
    }
    header = gst_buffer_new_wrapped (g_memdup (p + offset, length[i]), length[i]);
    GST_BUFFER_FLAG_SET (header, GST_BUFFER_FLAG_HEADER);
    res.push_back(UniqueBuffer { header });
    offset += length[i];
  }

  return res;
}

UniqueFeatureList GetFactoryForCaps(GList* elements,
                                    UniqueCaps&& caps,
                                    GstPadDirection direction) {
  SB_DLOG(INFO) << __FUNCTION__ << ": " << gst_caps_to_string(caps.get());
  SB_DCHECK(direction != GST_PAD_UNKNOWN);
  UniqueFeatureList candidates{
      gst_element_factory_list_filter(elements, caps.get(), direction, false)};
  return candidates;
}

template <typename C>
bool GstRegistryHasElementForCodecImpl(C codec) {
  static_assert(std::is_same<C, SbMediaVideoCodec>::value ||
                std::is_same<C, SbMediaAudioCodec>::value, "Invalid codec");
  auto type = std::is_same<C, SbMediaVideoCodec>::value
                  ? GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO
                  : GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO;
  UniqueFeatureList decoder_factories{gst_element_factory_list_get_elements(
      GST_ELEMENT_FACTORY_TYPE_DECODER | type, GST_RANK_MARGINAL)};

  UniqueFeatureList elements;
  std::vector<std::string> caps;

  caps = CodecToGstCaps(codec);
  if (caps.empty()) {
    SB_DLOG(INFO) << "No caps for codec " << codec;
    return false;
  }

  for (auto single_caps : caps) {
    UniqueCaps gst_caps{gst_caps_from_string(single_caps.c_str())};
    elements = std::move(GetFactoryForCaps(decoder_factories.get(),
                                           std::move(gst_caps), GST_PAD_SINK));
    if (elements) {
      SB_DLOG(INFO) << "Found decoder for " << single_caps;
      break;
    }
  }

  if (elements) {
    // Decoder is there.
    return true;
  }

  UniqueFeatureList parser_factories{gst_element_factory_list_get_elements(
      GST_ELEMENT_FACTORY_TYPE_PARSER | type, GST_RANK_MARGINAL)};

  SB_DLOG(INFO) << "No decoder for codec " << codec << ". Falling back to parsers.";
  // No decoder. Check if there's a parser and a decoder accepting its caps.
  for (auto single_caps : caps) {
    UniqueCaps gst_caps{gst_caps_from_string(single_caps.c_str())};
    elements = std::move(GetFactoryForCaps(parser_factories.get(),
                                           std::move(gst_caps), GST_PAD_SINK));
    if (elements) {
      for (GList* iter = elements.get(); iter; iter = iter->next) {
        GstElementFactory* gst_element_factory =
            static_cast<GstElementFactory*>(iter->data);
        const GList* pad_templates =
            gst_element_factory_get_static_pad_templates(gst_element_factory);
        for (const GList* pad_templates_iter = pad_templates;
             pad_templates_iter;
             pad_templates_iter = pad_templates_iter->next) {
          GstStaticPadTemplate* pad_template =
              static_cast<GstStaticPadTemplate*>(pad_templates_iter->data);
          if (pad_template->direction == GST_PAD_SRC) {
            UniqueCaps pad_caps{gst_static_pad_template_get_caps(pad_template)};
            if (GetFactoryForCaps(decoder_factories.get(), std::move(pad_caps),
                                  GST_PAD_SINK)) {
              SB_DLOG(INFO) << "Found parser for " << single_caps
                            << " and decoder"
                               " accepting parser's src caps.";
              return true;
            }
          }
        }
      }
    }
  }

  SB_LOG(WARNING) << "Can not play codec " << codec;
  return false;
}

template <typename C>
bool GstRegistryHasElementForCodec(C codec) {
  static std::map<C, bool> cache;
  auto it = cache.find(codec);
  if (it != cache.end())
    return it->second;
  bool r = GstRegistryHasElementForCodecImpl(codec);
  cache[codec] = r;
  return r;
}

}  // namespace

bool GstRegistryHasElementForMediaType(SbMediaVideoCodec codec) {
  if (kSbMediaVideoCodecVp9 == codec && !kSbHasMediaWebmVp9Support)
    return false;
  return GstRegistryHasElementForCodec(codec);
}

bool GstRegistryHasElementForMediaType(SbMediaAudioCodec codec) {
  return GstRegistryHasElementForCodec(codec);
}

std::vector<std::string> CodecToGstCaps(SbMediaVideoCodec codec) {
  switch (codec) {
    default:
    case kSbMediaVideoCodecNone:
      return {};

    case kSbMediaVideoCodecH264:
      return {{"video/x-h264, stream-format=byte-stream, alignment=nal"}};

    case kSbMediaVideoCodecH265:
      return {{"video/x-h265"}};

    case kSbMediaVideoCodecMpeg2:
      return {{"video/mpeg, mpegversion=(int) 2"}};

    case kSbMediaVideoCodecTheora:
      return {{"video/x-theora"}};

    case kSbMediaVideoCodecVc1:
      return {{"video/x-vc1"}};

    case kSbMediaVideoCodecAv1:
      return {{"video/x-av1"}};

    case kSbMediaVideoCodecVp8:
      return {{"video/x-vp8"}};

    case kSbMediaVideoCodecVp9:
      return {{"video/x-vp9"}};
  }
}

std::vector<std::string> CodecToGstCaps(SbMediaAudioCodec codec, const SbMediaAudioStreamInfo* info) {
  switch (codec) {
    default:
    case kSbMediaAudioCodecNone:
      return {};

    case kSbMediaAudioCodecAac: {
      std::string primary_caps = "audio/mpeg, mpegversion=4";
      if (info) {
        GstCaps* gst_caps = gst_caps_new_simple("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "channels", G_TYPE_INT, info->number_of_channels,
          "rate", G_TYPE_INT, info->samples_per_second,
          nullptr);
        if (info->audio_specific_config_size >= 2) {
          uint16_t codec_priv_size = info->audio_specific_config_size;
          const guint8* codec_priv = reinterpret_cast<const guint8*>(info->audio_specific_config);
          gst_codec_utils_aac_caps_set_level_and_profile(gst_caps, codec_priv, codec_priv_size);
        }
        gchar* caps_str = gst_caps_to_string (gst_caps);
        primary_caps = caps_str;
        g_free (caps_str);
        gst_caps_unref (gst_caps);

        SB_LOG(INFO) << "AAC audio caps from sample info: " << primary_caps;
      }
      return {{primary_caps}, {"audio/aac"}};
    }

    case kSbMediaAudioCodecAc3:
    case kSbMediaAudioCodecEac3: {
      std::string primary_caps = "audio/x-eac3";
      if (info) {
        primary_caps +=
          ", channels=" + std::to_string(info->number_of_channels) +
          ", rate=" + std::to_string(info->samples_per_second);
        SB_LOG(INFO) << "(E)Ac3 audio caps from sample info: " << primary_caps << ", codec specific info size: " << info->audio_specific_config_size;
      }
      return {{primary_caps}};
    }

    case kSbMediaAudioCodecOpus: {
      std::string primary_caps = "audio/x-opus, channel-mapping-family=0";
      if (info && info->audio_specific_config_size >= 19) {
        uint16_t codec_priv_size = info->audio_specific_config_size;
        const void* codec_priv = info->audio_specific_config;

        GstBuffer *tmp = gst_buffer_new_wrapped (g_memdup (codec_priv, codec_priv_size), codec_priv_size);
        GstCaps* gst_caps = gst_codec_utils_opus_create_caps_from_header (tmp, NULL);
        gchar* caps_str = gst_caps_to_string (gst_caps);

        primary_caps = caps_str;

        g_free (caps_str);
        gst_caps_unref (gst_caps);
        gst_buffer_unref (tmp);

        SB_LOG(INFO) << "Opus audio caps from sample info: " << primary_caps << ", codec specific info size: " << info->audio_specific_config_size;
      }
      return {{primary_caps}};
    }

    case kSbMediaAudioCodecVorbis: {
      std::string primary_caps = "audio/x-vorbis";
      if (info) {
        GstCaps* gst_caps = gst_caps_new_simple("audio/x-vorbis",
          "channels", G_TYPE_INT, info->number_of_channels,
          "rate", G_TYPE_INT, info->samples_per_second,
          nullptr);

        auto headers = ParseXiphStreamHeaders(info->audio_specific_config, info->audio_specific_config_size);
        if (!headers.empty()) {
          GValue array = G_VALUE_INIT;
          GValue value = G_VALUE_INIT;

          g_value_init (&array, GST_TYPE_ARRAY);
          g_value_init (&value, GST_TYPE_BUFFER);

          for (const auto &hdr : headers) {
            gst_value_set_buffer (&value, hdr.get());
            gst_value_array_append_value (&array, &value);
          }

          gst_caps_set_value(gst_caps, "streamheader", &array);
          g_value_reset (&value);
          g_value_reset (&array);
        }

        gchar* caps_str = gst_caps_to_string (gst_caps);
        primary_caps = caps_str;
        g_free (caps_str);
        gst_caps_unref (gst_caps);

        SB_LOG(INFO) << "Vorbis audio caps from sample info: " << primary_caps << ", codec specific info size: " << info->audio_specific_config_size;
      }
      return {{primary_caps}};
    }

    case kSbMediaAudioCodecMp3:
      return {{"audio/mpeg, mpegversion=1, layer=3"}};

    case kSbMediaAudioCodecFlac: {
      std::string primary_caps = "audio/x-flac";
      if (info) {
        GstCaps* gst_caps = gst_caps_new_simple("audio/x-flac",
          "channels", G_TYPE_INT, info->number_of_channels,
          "rate", G_TYPE_INT, info->samples_per_second,
          nullptr);

        // MP4 parser includes stream info block in audio_specific_config,
        // see third_party/chromium/media/formats/mp4/box_definitions.cc:bool FlacSpecificBox::Parse
        const uint16_t kFlacStreaminfoSize = 34;
        if (info->audio_specific_config_size == kFlacStreaminfoSize) {
          GValue array = G_VALUE_INIT;
          GValue value = G_VALUE_INIT;
          g_value_init (&array, GST_TYPE_ARRAY);
          g_value_init (&value, GST_TYPE_BUFFER);

          GstBuffer *block;
          uint16_t block_plus_hdr_size = info->audio_specific_config_size + 4 + 4;
          block = gst_buffer_new_allocate(nullptr, block_plus_hdr_size, nullptr);
          if (block) {
            GstMapInfo write_info;
            gst_buffer_map (block, &write_info, GST_MAP_WRITE);

            memcpy (write_info.data, "fLaC", 4); // marker
            write_info.data[4] = 0x80; // is_last = true, type = 0x0
            write_info.data[5] = 0x00;
            write_info.data[6] = (info->audio_specific_config_size & 0xFF00) >> 8;
            write_info.data[7] = (info->audio_specific_config_size & 0x00FF) >> 0;
            memcpy(&write_info.data[8], info->audio_specific_config, info->audio_specific_config_size);

            gst_buffer_unmap (block, &write_info);

            GST_BUFFER_FLAG_SET (block, GST_BUFFER_FLAG_HEADER);

            gst_value_set_buffer (&value, block);
            gst_value_array_append_value (&array, &value);
            gst_buffer_unref (block);

            gst_caps_set_value(gst_caps, "streamheader", &array);
          }

          g_value_reset (&value);
          g_value_reset (&array);
        }

        gchar* caps_str = gst_caps_to_string (gst_caps);
        primary_caps = caps_str;
        g_free (caps_str);
        gst_caps_unref (gst_caps);

        SB_LOG(INFO) << "Flac audio caps from sample info: " << primary_caps << ", codec specific info size: " << info->audio_specific_config_size;
      }
      return {{primary_caps}};
    }

    case kSbMediaAudioCodecPcm: {
      std::string primary_caps = "audio/x-raw";
      if (info) {
        // Starboard doesn't specify audio format in stream info.
        // There are 2 types defined in SbMediaAudioSampleType, so below is the best guess...
        const char* format = info->bits_per_sample == 32 ? "F32LE" : "S16LE";
        int channels = info->number_of_channels;

        GstCaps* gst_caps = gst_caps_new_simple("audio/x-raw",
          "format", G_TYPE_STRING, format,
          "rate", G_TYPE_INT, info->samples_per_second,
          "channels", G_TYPE_INT, channels,
          "layout", G_TYPE_STRING, "interleaved",
          "channel-mask", GST_TYPE_BITMASK, gst_audio_channel_get_fallback_mask(channels),
          nullptr);

        gchar* caps_str = gst_caps_to_string (gst_caps);
        primary_caps = caps_str;
        g_free (caps_str);
        gst_caps_unref (gst_caps);
      }

      return {{primary_caps}};
    }
  }
}

}  // namespace media
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
