#pragma once

#include <cstdint>
#include <string>
#include <tuple>

namespace tts
{

enum class language
{
    polish,
    english,
    german
};

enum class gender
{
    male,
    female
};

using index = uint8_t;
using voice_t = std::tuple<language, gender, index>;

class TextToVoiceIf
{
  public:
    virtual ~TextToVoiceIf() = default;
    virtual bool speak(const std::string&) = 0;
    virtual bool speak(const std::string&, const voice_t&) = 0;
    virtual bool speakasync(const std::string&) = 0;
    virtual bool speakasync(const std::string&, const voice_t&) = 0;
    virtual bool waitspoken() = 0;
    virtual voice_t getvoice() = 0;
    virtual void setvoice(const voice_t&) = 0;
    static void kill();
};

} // namespace tts
