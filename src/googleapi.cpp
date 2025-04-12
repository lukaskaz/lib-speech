#include "tts/interfaces/googleapi.hpp"

#include "shell/interfaces/linux/bash/shell.hpp"
#include "tts/helpers.hpp"

#include <boost/beast/core/detail/base64.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <source_location>

namespace tts::googleapi
{

using namespace helpers;
using namespace std::string_literals;
using json = nlohmann::json;

static const std::filesystem::path configFile = "../conf/init.json";
static const std::filesystem::path audioDirectory = "audio";
static const std::filesystem::path playbackName = "playback.mp3";
static const std::string playAudioCmd =
    "play --no-show-progress " + (audioDirectory / playbackName).native() +
    " --type alsa";
static const std::string convUri =
    "https://texttospeech.googleapis.com/v1/text:synthesize";

static const std::map<voice_t,
                      std::tuple<std::string, std::string, std::string>>
    voiceMap = {{{language::polish, gender::female, 1},
                 {"pl-PL", "pl-PL-Standard-E", "FEMALE"}},
                {{language::polish, gender::female, 2},
                 {"pl-PL", "pl-PL-Standard-A", "FEMALE"}},
                {{language::polish, gender::female, 3},
                 {"pl-PL", "pl-PL-Standard-D", "FEMALE"}},
                {{language::polish, gender::male, 1},
                 {"pl-PL", "pl-PL-Standard-B", "MALE"}},
                {{language::polish, gender::male, 2},
                 {"pl-PL", "pl-PL-Standard-C", "MALE"}},
                {{language::english, gender::female, 1},
                 {"en-US", "en-US-Standard-C", "FEMALE"}},
                {{language::english, gender::male, 1},
                 {"en-US", "en-US-Standard-A", "MALE"}},
                {{language::german, gender::female, 1},
                 {"de-DE", "de-DE-Standard-C", "FEMALE"}},
                {{language::german, gender::male, 1},
                 {"de-DE", "de-DE-Standard-B", "MALE"}}};

struct TextToVoice::Handler : public std::enable_shared_from_this<Handler>
{
  public:
    explicit Handler(const configmin_t& config) :
        logif{std::get<std::shared_ptr<logs::LogIf>>(config)},
        shell{shell::Factory::create<shell::lnx::bash::Shell>()},
        helpers{helpers::HelpersFactory::create()},
        filesystem{this, audioDirectory / playbackName},
        google{this, configFile, std::get<voice_t>(config)}
    {}

    explicit Handler(const configall_t& config) :
        logif{std::get<std::shared_ptr<logs::LogIf>>(config)},
        shell{std::get<std::shared_ptr<shell::ShellIf>>(config)},
        helpers{std::get<std::shared_ptr<helpers::HelpersIf>>(config)},
        filesystem{this, audioDirectory / playbackName},
        google{this, configFile, std::get<voice_t>(config)}
    {}

    bool speak(const std::string& text)
    {
        if (std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
            lock.try_lock())
        {
            log(logs::level::debug, "Requested text to speak: '" + text + "'");
            auto audio = google.getaudio(text);
            filesystem.savetofile(audio);
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
            auto audio = google.getaudio(text, voice);
            filesystem.savetofile(audio);
            shell->run(playAudioCmd);
            return true;
        }
        log(logs::level::warning,
            "Cannot speak text: '" + text + "', tts in use");
        return false;
    }

    bool speakasync(const std::string& text)
    {
        return helpers->createasync([weak = weak_from_this(), text]() {
            if (auto self = weak.lock())
                self->speak(text);
        });
    }

    bool speakasync(const std::string& text, const voice_t& voice)
    {
        return helpers->createasync([weak = weak_from_this(), text, voice]() {
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
        Filesystem(const Handler* handler, const std::filesystem::path& path) :
            handler{handler}, path{path}
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

        void savetofile(const std::string& data) const
        {
            std::ofstream ofs(path, std::ios::binary);
            ofs << data;
            handler->log(logs::level::debug,
                         "Written data of size: " + str(data.size()) +
                             ", to file: '" + path.native() + "'");
        }

      private:
        const Handler* handler;
        const std::filesystem::path path;
        bool direxist;
    } filesystem;
    class Google
    {
      public:
        Google(const Handler* handler, const std::filesystem::path& configfile,
               const voice_t& voice) :
            handler{handler},
            audiourl{[](const std::filesystem::path& file) {
                std::ifstream ifs(file);
                if (!ifs.is_open())
                    throw std::runtime_error("Cannot open config file for TTS");
                auto content = std::string(
                    std::istreambuf_iterator<char>(ifs.rdbuf()), {});
                json ttsConfig = json::parse(content)["tts"];
                if (ttsConfig["key"].is_null() ||
                    ttsConfig["key"].get<std::string>().empty())
                    throw std::runtime_error(
                        "Cannot get TTS key from config file");
                return convUri + "?key=" + ttsConfig["key"].get<std::string>();
            }(configfile)},
            voice{voice}
        {
            handler->log(logs::level::info,
                         "Created gapi tts [langcode/langname/gender]: " +
                             getparams());
        }

        ~Google()
        {
            handler->log(logs::level::info,
                         "Released gapi tts [langcode/langname/gender]: " +
                             getparams());
        }

        std::string getaudio(const std::string& text)
        {
            const auto& [code, name, gender] = getmappedvoice();
            const std::string config =
                "{'input':{'text':'" + text + "'},'voice':{'languageCode':'" +
                code + "','name':'" + name + "','ssmlGender':'" + gender +
                "'},'audioConfig':{'audioEncoding':'MP3'}}";
            std::string audioData;
            handler->helpers->uploadData(audiourl, config, audioData);
            auto rawaudio = json::parse(std::move(audioData))["audioContent"]
                                .get<std::string>();
            return decode(rawaudio);
        }

        std::string getaudio(const std::string& text, const voice_t& tmpvoice)
        {
            const auto mainVoice{voice};
            voice = tmpvoice;
            auto audio = getaudio(text);
            handler->log(logs::level::debug, "Text spoken as " + getparams());
            voice = mainVoice;
            return audio;
        }

        voice_t getvoice() const
        {
            return voice;
        }

        void setvoice(const voice_t& voice)
        {
            this->voice = voice;
        }

        std::string getparams() const
        {
            const auto& [code, name, gender] = getmappedvoice();
            return code + "/" + name + "/" + gender;
        }

      private:
        const Handler* handler;
        const std::string audiourl;
        voice_t voice;

        decltype(voiceMap)::mapped_type getmappedvoice() const
        {
            decltype(voice) defaultvoice = {std::get<language>(voice),
                                            std::get<gender>(voice), 1};
            return voiceMap.contains(voice) ? voiceMap.at(voice)
                                            : voiceMap.at(defaultvoice);
        }

        std::string decode(const std::string& encoded)
        {
            using namespace boost::beast::detail;
            std::string decoded(encoded.size(), '\0');
            // decoded.resize(base64::encoded_size(encoded.size()));
            base64::decode(&decoded[0], encoded.c_str(), decoded.size());
            return decoded;
        }
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

} // namespace tts::googleapi
