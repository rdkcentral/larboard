// C++ classes for ScriptEngine API JSON-RPC API.
// Generated automatically from 'IScriptEngine.h'. DO NOT EDIT.

// Note: This code is inherently not thread safe. If required, proper synchronisation must be added.

#pragma once

#include <core/JSON.h>
#include <interfaces/IScriptEngine.h>

namespace WPEFramework {

namespace JsonData {

    namespace ScriptEngine {

        // Method params/result classes
        //

        class URLChangedParamsData : public Core::JSON::Container {
        public:
            URLChangedParamsData()
                : Core::JSON::Container()
            {
                Add(_T("url"), &URL);
            }

            URLChangedParamsData(const URLChangedParamsData&) = delete;
            URLChangedParamsData& operator=(const URLChangedParamsData&) = delete;

        public:
            Core::JSON::String URL;
        }; // class URLChangedParamsData

        class URLData : public Core::JSON::Container {
        public:
            URLData()
                : Core::JSON::Container()
            {
                Add(_T("value"), &Value);
            }

            URLData(const URLData&) = delete;
            URLData& operator=(const URLData&) = delete;

        public:
            Core::JSON::String Value;
        }; // class URLData

    } // namespace ScriptEngine

} // namespace JsonData

}

