#include "tts/interfaces/googlebasic.hpp"

#include "shell/interfaces/linux/bash/shell.hpp"
#include "tts/helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <mutex>
#include <source_location>

namespace tts::googlebasic
{

using namespace helpers;
using namespace std::string_literals;

static const std::filesystem::path audioDirectory = "audio";
static const std::filesystem::path playbackName = "playback.mp3";
static const std::string playAudioCmd =
    "play --no-show-progress " + (audioDirectory / playbackName).native() +
    " --type alsa";
static constexpr const char* convUri =
    "https://translate.google.com/translate_tts?client=tw-ob";

static const std::map<voice_t, std::string> voiceMap = {
    {{language::polish, gender::male, 1}, "pl"},
    {{language::polish, gender::female, 1}, "pl"},
    {{language::english, gender::male, 1}, "en"},
    {{language::english, gender::female, 1}, "en"},
    {{language::german, gender::male, 1}, "de"},
    {{language::german, gender::female, 1}, "de"}};

struct TextToVoice::Handler : public std::enable_shared_from_this<Handler>
{
  public:
    explicit Handler(const configmin_t& config) :
        logif{std::get<std::shared_ptr<logs::LogIf>>(config)},
        shell{shell::Factory::create<shell::lnx::bash::Shell>()},
        helpers{helpers::HelpersFactory::create()},
        filesystem{this, audioDirectory / playbackName},
        google{this, std::get<voice_t>(config), audioDirectory / playbackName}
    {}

    explicit Handler(const configall_t& config) :
        logif{std::get<std::shared_ptr<logs::LogIf>>(config)},
        shell{std::get<std::shared_ptr<shell::ShellIf>>(config)},
        helpers{std::get<std::shared_ptr<helpers::HelpersIf>>(config)},
        filesystem{this, audioDirectory / playbackName},
        google{this, std::get<voice_t>(config), audioDirectory / playbackName}
    {}

    bool speak(const std::string& text)
    {
        if (std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
            lock.try_lock())
        {
            log(logs::level::debug, "Requested text to speak: '" + text + "'");
            google.getaudio(text);
            shell->run(playAudioCmd);
            return true;
        }
        log(logs::level::warning,
            "Cannot speak text: '" + text + "', tts in use");
        return false;
    }

    bool speak(const std::string& text, const voice_t& voice)
    {
        if (std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
            lock.try_lock())
        {
            log(logs::level::debug, "Requested text to speak: '" + text + "'");
            google.getaudio(text, voice);
            shell->run(playAudioCmd);
            return true;
        }
        log(logs::level::warning,
            "Cannot speak text: '" + text + "', tts in use");
        return false;
    }

    bool speakasync(const std::string& text)
    {
        return helpers->createasync([this, weak = weak_from_this(), text]() {
            if (auto self = weak.lock())
                self->speak(text);
        });
    }

    bool speakasync(const std::string& text, const voice_t& voice)
    {
        return helpers->createasync(
            [this, weak = weak_from_this(), text, voice]() {
                if (auto self = weak.lock())
                    self->speak(text, voice);
            });
    }

    void setvoice(const voice_t& voice)
    {
        google.setvoice(voice);
        log(logs::level::debug, "Setting voice to: " + google.getparams());
    }

    voice_t getvoice() const
    {
        return google.getvoice();
    }

  private:
    const std::shared_ptr<logs::LogIf> logif;
    const std::shared_ptr<shell::ShellIf> shell;
    const std::shared_ptr<helpers::HelpersIf> helpers;
    std::mutex mtx;
    class Filesystem
    {
      public:
        explicit Filesystem(const Handler* handler,
                            const std::filesystem::path& path) :
            handler{handler},
            path{path}
        {
            createdirectory();
        }

        ~Filesystem()
        {
            removedirectory();
        }

        void createdirectory()
        {
            if ((direxist =
                     !std::filesystem::create_directories(path.parent_path())))
                handler->log(logs::level::warning,
                             "Cannot create already existing directory: '" +
                                 path.parent_path().native() + "'");
            else
                handler->log(logs::level::debug,
                             "Created directory: '" +
                                 path.parent_path().native() + "'");
        }
        void removedirectory() const
        {
            direxist ? false : std::filesystem::remove_all(path.parent_path());
            if (direxist)
                handler->log(logs::level::warning,
                             "Not removing previously existed directory: '" +
                                 path.parent_path().native() + "'");
            else
                handler->log(logs::level::debug,
                             "Removed directory: '" +
                                 path.parent_path().native() + "'");
        }

      private:
        const Handler* handler;
        const std::filesystem::path path;
        bool direxist;
    } filesystem;

    class Google
    {
      public:
        Google(const Handler* handler, const voice_t& voice,
               const std::string& audiopath) :
            handler{handler},
            audiopath{audiopath}
        {
            setvoice(voice);
            handler->log(logs::level::info,
                         "Created gbasic tts [lang/gender/idx]: " +
                             getparams());
        }

        ~Google()
        {
            handler->log(logs::level::info,
                         "Released gbasic tts [lang/gender/idx]: " +
                             getparams());
        }

        void getaudio(const std::string& text)
        {
            handler->helpers->downloadFile(audiourl, text, audiopath);
        }

        void getaudio(const std::string& text, const voice_t& tmpvoice)
        {
            auto mainVoice{voice};
            setvoice(tmpvoice);
            getaudio(text);
            handler->log(logs::level::debug, "Text spoken as " + getparams());
            setvoice(mainVoice);
        }

        voice_t getvoice() const
        {
            return voice;
        }

        void setvoice(const voice_t& voice)
        {
            this->voice = voice;
            decltype(voice) defaultvoice = {std::get<language>(voice),
                                            std::get<gender>(voice), 1};
            const auto& uselang = voiceMap.contains(voice)
                                      ? voiceMap.at(voice)
                                      : voiceMap.at(defaultvoice);
            audiourl = std::string(convUri) + "&tl=" + uselang + "&q=";
        }

        std::string getparams() const
        {
            decltype(voice) defaultvoice = {std::get<language>(voice),
                                            std::get<gender>(voice), 1};
            const auto& uselang = voiceMap.contains(voice)
                                      ? voiceMap.at(voice)
                                      : voiceMap.at(defaultvoice);
            auto genderid = std::get<gender>(voice);
            return uselang + "/" +
                   std::string(genderid == gender::male     ? "male"
                               : genderid == gender::female ? "female"
                                                            : "unknown") +
                   "/" + str(std::get<index>(voice));
        }

      private:
        const Handler* handler;
        const std::string audiopath;
        std::string audiourl;
        voice_t voice;
    } google;

    void log(
        logs::level level, const std::string& msg,
        const std::source_location loc = std::source_location::current()) const
    {
        if (logif)
            logif->log(level, std::string{loc.function_name()}, msg);
    }
};

TextToVoice::TextToVoice(const config_t& config)
{
    handler = std::visit(
        [](const auto& config) -> decltype(TextToVoice::handler) {
            if constexpr (!std::is_same<const std::monostate&,
                                        decltype(config)>())
            {
                return std::make_shared<TextToVoice::Handler>(config);
            }
            throw std::runtime_error(
                std::source_location::current().function_name() +
                "-> config not supported"s);
        },
        config);
}
TextToVoice::~TextToVoice() = default;

bool TextToVoice::speak(const std::string& text)
{
    return handler->speak(text);
}

bool TextToVoice::speak(const std::string& text, const voice_t& voice)
{
    return handler->speak(text, voice);
}

bool TextToVoice::speakasync(const std::string& text)
{
    return handler->speakasync(text);
}

bool TextToVoice::speakasync(const std::string& text, const voice_t& voice)
{
    return handler->speakasync(text, voice);
}

voice_t TextToVoice::getvoice()
{
    return handler->getvoice();
}

void TextToVoice::setvoice(const voice_t& voice)
{
    handler->setvoice(voice);
}

} // namespace tts::googlebasic
