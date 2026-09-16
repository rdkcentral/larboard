// C++ classes for Player Info API JSON-RPC API.
// Generated automatically from 'PlayerInfo.json'. DO NOT EDIT.

// Note: This code is inherently not thread safe. If required, proper synchronisation must be added.

#pragma once

#include <core/JSON.h>
#include <core/Enumerate.h>

namespace WPEFramework {

namespace JsonData {

    namespace PlayerInfo {

        // Method params/result classes
        //

        class DolbymodeData : public Core::JSON::Container {
        public:
            enum class DolbyType : uint8_t {
                DIGITAL_PCM = 0,
                DIGITAL_PLUS = 3,
                DIGITAL_AC3 = 4,
                AUTO = 5
            };

            DolbymodeData()
                : Core::JSON::Container()
            {
                Add(_T("value"), &Value);
            }

            DolbymodeData(const DolbymodeData&) = delete;
            DolbymodeData& operator=(const DolbymodeData&) = delete;

        public:
            Core::JSON::EnumType<DolbymodeData::DolbyType> Value;
        }; // class DolbymodeData

        class CodecsData : public Core::JSON::Container {
        public:
            // Audio Codec supported by the platform
            enum class AudiocodecsType : uint8_t {
                UNDEFINED,
                AAC,
                AC3,
                AC3PLUS,
                DTS,
                MPEG1,
                MPEG2,
                MPEG3,
                MPEG4,
                OPUS,
                VORBISOGG,
                WAV
            };

            // Video Codec supported by the platform
            enum class VideocodecsType : uint8_t {
                UNDEFINED,
                H263,
                H264,
                H265,
                H26510,
                MPEG,
                VP8,
                VP9,
                VP10
            };

            CodecsData()
                : Core::JSON::Container()
            {
                Add(_T("audio"), &Audio);
                Add(_T("video"), &Video);
            }

            CodecsData(const CodecsData&) = delete;
            CodecsData& operator=(const CodecsData&) = delete;

        public:
            Core::JSON::ArrayType<Core::JSON::EnumType<CodecsData::AudiocodecsType>> Audio;
            Core::JSON::ArrayType<Core::JSON::EnumType<CodecsData::VideocodecsType>> Video;
        }; // class CodecsData

    } // namespace PlayerInfo

} // namespace JsonData

// Enum conversion handlers
ENUM_CONVERSION_HANDLER(JsonData::PlayerInfo::CodecsData::AudiocodecsType)
ENUM_CONVERSION_HANDLER(JsonData::PlayerInfo::CodecsData::VideocodecsType)
ENUM_CONVERSION_HANDLER(JsonData::PlayerInfo::DolbymodeData::DolbyType)

}

