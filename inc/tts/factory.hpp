#pragma once

#include "tts/interfaces/texttovoice.hpp"

#include <memory>

namespace tts
{

class TextToVoiceFactory
{
  public:
    template <typename T, typename C>
    static std::shared_ptr<TextToVoiceIf> create(const C& config)
    {
        return std::shared_ptr<T>(new T(config));
    }
};

} // namespace tts
