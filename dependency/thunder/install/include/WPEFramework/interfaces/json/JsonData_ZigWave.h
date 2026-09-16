// C++ classes for ZigWave API JSON-RPC API.
// Generated automatically from 'IZigWave.h'. DO NOT EDIT.

// Note: This code is inherently not thread safe. If required, proper synchronisation must be added.

#pragma once

#include <core/JSON.h>
#include <interfaces/IZigWave.h>

namespace WPEFramework {

namespace JsonData {

    namespace ZigWave {

        // Common classes
        //

        class BindParamsInfo : public Core::JSON::Container {
        public:
            BindParamsInfo()
                : Core::JSON::Container()
            {
                Add(_T("source"), &Source);
                Add(_T("destination"), &Destination);
            }

            BindParamsInfo(const BindParamsInfo&) = delete;
            BindParamsInfo& operator=(const BindParamsInfo&) = delete;

        public:
            Core::JSON::DecUInt32 Source;
            Core::JSON::DecUInt32 Destination;
        }; // class BindParamsInfo

        // Method params/result classes
        //

        class PermutableData : public Core::JSON::Container {
        public:
            PermutableData()
                : Core::JSON::Container()
            {
                Add(_T("value"), &Value);
            }

            PermutableData(const PermutableData&) = delete;
            PermutableData& operator=(const PermutableData&) = delete;

        public:
            Core::JSON::Boolean Value;
        }; // class PermutableData

    } // namespace ZigWave

} // namespace JsonData

}

