// C++ classes for WatchDog API JSON-RPC API.
// Generated automatically from 'IWatchDog.h'. DO NOT EDIT.

// Note: This code is inherently not thread safe. If required, proper synchronisation must be added.

#pragma once

#include <core/JSON.h>
#include <interfaces/IWatchDog.h>

namespace WPEFramework {

namespace JsonData {

    namespace WatchDog {

        // Method params/result classes
        //

        class TouchParamsData : public Core::JSON::Container {
        public:
            TouchParamsData()
                : Core::JSON::Container()
            {
                Add(_T("callsign"), &Callsign);
            }

            TouchParamsData(const TouchParamsData&) = delete;
            TouchParamsData& operator=(const TouchParamsData&) = delete;

        public:
            Core::JSON::String Callsign; // In case a specific watchdog needs to be padded pass
        }; // class TouchParamsData

    } // namespace WatchDog

} // namespace JsonData

}

