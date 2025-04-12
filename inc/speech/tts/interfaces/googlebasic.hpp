#pragma once

#include "logs/interfaces/logs.hpp"
#include "shell/interfaces/shell.hpp"
#include "speech/helpers.hpp"
#include "speech/tts/factory.hpp"

#include <tuple>
#include <variant>

namespace tts::googlebasic
{

using configmin_t = std::tuple<voice_t, std::shared_ptr<logs::LogIf>>;
using configall_t = std::tuple<voice_t, std::shared_ptr<shell::ShellIf>,
                               std::shared_ptr<helpers::HelpersIf>,
                               std::shared_ptr<logs::LogIf>>;
using config_t = std::variant<std::monostate, configmin_t, configall_t>;

class TextToVoice : public TextToVoiceIf
{
  public:
    ~TextToVoice();
    bool speak(const std::string&) override;
    bool speak(const std::string&, const voice_t&) override;
    bool speakasync(const std::string&) override;
    bool speakasync(const std::string&, const voice_t&) override;
    bool waitspoken() override;
    voice_t getvoice() override;
    void setvoice(const voice_t&) override;

  private:
    friend class tts::TextToVoiceFactory;
    TextToVoice(const config_t&);

    struct Handler;
    std::shared_ptr<Handler> handler;
};

} // namespace tts::googlebasic
