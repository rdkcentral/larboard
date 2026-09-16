// Generated automatically from 'IDolby.h'. DO NOT EDIT.

#pragma once

#include "Module.h"
#include "JsonData_DolbyOutput.h"
#include <interfaces/IDolby.h>

namespace WPEFramework {

namespace Exchange {

    namespace Dolby {

        namespace JOutput {

            namespace Version {

                constexpr uint8_t Major = 1;
                constexpr uint8_t Minor = 0;
                constexpr uint8_t Patch = 0;

            } // namespace Version

            using JSONRPC = PluginHost::JSONRPC;

            static void Register(JSONRPC& _module_, Dolby::IOutput* _impl_)
            {
                ASSERT(_impl_ != nullptr);

                _module_.RegisterVersion(_T("JOutput"), Version::Major, Version::Minor, Version::Patch);

                // Register methods and properties...

                // Property: 'dolby_atmosmetadata' - Atmos capabilities of Sink (r/o)
                _module_.Register<void, Core::JSON::Boolean>(_T("dolby_atmosmetadata"), 
                    [_impl_](Core::JSON::Boolean& result) -> uint32_t {
                        uint32_t _errorCode;

                        // read-only property get
                        bool _result{};

                        _errorCode = _impl_->AtmosMetadata(_result);

                        if (_errorCode == Core::ERROR_NONE) {
                            result = _result;
                        }

                        return (_errorCode);
                    });

                // Property: 'dolby_soundmode' - Sound Mode - Mono/Stereo/Surround (r/o)
                _module_.Register<void, Core::JSON::EnumType<Exchange::Dolby::IOutput::SoundModes>>(_T("dolby_soundmode"), 
                    [_impl_](Core::JSON::EnumType<Exchange::Dolby::IOutput::SoundModes>& result) -> uint32_t {
                        uint32_t _errorCode;

                        // read-only property get
                        Exchange::Dolby::IOutput::SoundModes _result{};

                        _errorCode = _impl_->SoundMode(_result);

                        if (_errorCode == Core::ERROR_NONE) {
                            result = _result;
                        }

                        return (_errorCode);
                    });

                // Property: 'dolby_enableatmosoutput' - Enable Atmos Audio Output (w/o)
                _module_.Register<Core::JSON::Boolean, void>(_T("dolby_enableatmosoutput"), 
                    [_impl_](const Core::JSON::Boolean& params) -> uint32_t {
                        uint32_t _errorCode;

                        // write-only property set
                        const bool _params{params};

                        _errorCode = _impl_->EnableAtmosOutput(_params);

                        return (_errorCode);
                    });

                // Property: 'dolby_mode' - Dolby Mode
                _module_.Register<Core::JSON::EnumType<Exchange::Dolby::IOutput::Type>, Core::JSON::EnumType<Exchange::Dolby::IOutput::Type>>(_T("dolby_mode"), 
                    [_impl_](const Core::JSON::EnumType<Exchange::Dolby::IOutput::Type>& params,
                             Core::JSON::EnumType<Exchange::Dolby::IOutput::Type>& result) -> uint32_t {
                        uint32_t _errorCode;

                        if (params.IsSet() == false) {
                            // property get
                            Exchange::Dolby::IOutput::Type _result{};

                            _errorCode = (static_cast<const Dolby::IOutput*>(_impl_))->Mode(_result);

                            if (_errorCode == Core::ERROR_NONE) {
                                result = _result;
                            }

                        } else {
                            // property set
                            const Exchange::Dolby::IOutput::Type _params{params};

                            _errorCode = _impl_->Mode(_params);

                            result.Null(true);
                        }
                        return (_errorCode);
                    });

            }

            static void Unregister(JSONRPC& _module_)
            {
                // Unregister methods and properties...
                _module_.Unregister(_T("dolby_atmosmetadata"));
                _module_.Unregister(_T("dolby_soundmode"));
                _module_.Unregister(_T("dolby_enableatmosoutput"));
                _module_.Unregister(_T("dolby_mode"));
            }

            namespace Event {

                PUSH_WARNING(DISABLE_WARNING_UNUSED_FUNCTIONS)

                // Event: 'dolby_audiomodechanged'
                static void AudioModeChanged(const JSONRPC& _module_, const JsonData::Dolby::Output::AudioModeChangedParamsData& params)
                {
                    _module_.Notify(_T("dolby_audiomodechanged"), params);
                }

                // Event: 'dolby_audiomodechanged'
                static void AudioModeChanged(const JSONRPC& _module_, const Core::JSON::EnumType<Exchange::Dolby::IOutput::SoundModes>& mode,
                         const Core::JSON::Boolean& enabled)
                {
                    JsonData::Dolby::Output::AudioModeChangedParamsData _params_;
                    _params_.Mode = mode;
                    _params_.Enabled = enabled;

                    AudioModeChanged(_module_, _params_);
                }

                // Event: 'dolby_audiomodechanged'
                static void AudioModeChanged(const JSONRPC& _module_, const Exchange::Dolby::IOutput::SoundModes& mode, const bool& enabled)
                {
                    JsonData::Dolby::Output::AudioModeChangedParamsData _params_;
                    _params_.Mode = mode;
                    _params_.Enabled = enabled;

                    AudioModeChanged(_module_, _params_);
                }

                POP_WARNING()

            } // namespace Event

        } // namespace JOutput

    } // namespace Dolby

} // namespace Exchange

} // namespace WPEFramework

