#pragma once

#include "logs/interfaces/logs.hpp"
#include "shell/interfaces/shell.hpp"
#include "speech/stt/factory.hpp"

#include <tuple>
#include <variant>

namespace stt::v2::googlecloud
{

using configmin_t =
    std::tuple<language, std::string, std::shared_ptr<logs::LogIf>>;
using configall_t =
    std::tuple<language, std::string, std::shared_ptr<shell::ShellIf>,
               std::shared_ptr<logs::LogIf>>;
using config_t = std::variant<std::monostate, configmin_t, configall_t>;

class TextFromVoice : public TextFromVoiceIf
{
  public:
    virtual ~TextFromVoice();

    transcript_t listen() override;
    transcript_t listen(language) override;

  private:
    friend class stt::TextFromVoiceFactory;
    TextFromVoice(const config_t&);

    struct Handler;
    std::unique_ptr<Handler> handler;
};

} // namespace stt::v2::googlecloud
