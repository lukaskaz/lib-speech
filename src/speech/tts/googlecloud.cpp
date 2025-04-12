#include "speech/tts/interfaces/googlecloud.hpp"

#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/texttospeech/v1/text_to_speech_client.h"

#include "shell/interfaces/linux/bash/shell.hpp"
#include "speech/helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <source_location>

namespace tts::googlecloud
{

namespace texttospeech = google::cloud::texttospeech::v1;
namespace texttospeech_type = google::cloud::texttospeech_v1;

using namespace speech::helpers;
using namespace std::string_literals;
using ssmlgender = texttospeech::SsmlVoiceGender;

static const std::filesystem::path keyFile = "../conf/key.json";
static const std::filesystem::path audioDirectory = "audio";
static const std::filesystem::path playbackName = "playback.mp3";
static const std::string playAudioCmd =
    "play --no-show-progress " + (audioDirectory / playbackName).native() +
    " --type alsa";
// static constexpr const char* keyEnvVar = "GOOGLE_APPLICATION_CREDENTIALS";

static const std::map<voice_t, std::tuple<std::string, std::string, ssmlgender>>
    voiceMap = {{{language::polish, gender::female, 1},
                 {"pl-PL", "pl-PL-Standard-E", ssmlgender::FEMALE}},
                {{language::polish, gender::female, 2},
                 {"pl-PL", "pl-PL-Standard-A", ssmlgender::FEMALE}},
                {{language::polish, gender::female, 3},
                 {"pl-PL", "pl-PL-Standard-D", ssmlgender::FEMALE}},
                {{language::polish, gender::male, 1},
                 {"pl-PL", "pl-PL-Standard-B", ssmlgender::MALE}},
                {{language::polish, gender::male, 2},
                 {"pl-PL", "pl-PL-Standard-C", ssmlgender::MALE}},
                {{language::english, gender::female, 1},
                 {"en-US", "en-US-Standard-C", ssmlgender::FEMALE}},
                {{language::english, gender::male, 1},
                 {"en-US", "en-US-Standard-D", ssmlgender::MALE}},
                {{language::german, gender::female, 1},
                 {"de-DE", "de-DE-Standard-C", ssmlgender::FEMALE}},
                {{language::german, gender::male, 1},
                 {"de-DE", "de-DE-Standard-B", ssmlgender::MALE}}};

struct TextToVoice::Handler : public std::enable_shared_from_this<Handler>
{
  public:
    explicit Handler(const configmin_t& config) :
        logif{std::get<std::shared_ptr<logs::LogIf>>(config)},
        shell{shell::Factory::create<shell::lnx::bash::Shell>()},
        helpers{speech::helpers::HelpersFactory::create()},
        filesystem{this, audioDirectory / playbackName},
        google{this, keyFile, std::get<voice_t>(config)}
    {}

    explicit Handler(const configall_t& config) :
        logif{std::get<std::shared_ptr<logs::LogIf>>(config)},
        shell{std::get<std::shared_ptr<shell::ShellIf>>(config)},
        helpers{std::get<std::shared_ptr<speech::helpers::HelpersIf>>(config)},
        filesystem{this, audioDirectory / playbackName},
        google{this, keyFile, std::get<voice_t>(config)}
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

    bool waitspoken() const
    {
        return helpers->waitasync();
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
    const std::shared_ptr<speech::helpers::HelpersIf> helpers;
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
        Google(const Handler* handler, const std::filesystem::path& keyfile,
               const voice_t& voice) :
            handler{handler},
            client{texttospeech_type::MakeTextToSpeechConnection(
                google::cloud::Options{}
                    .set<google::cloud::UnifiedCredentialsOption>(
                        google::cloud::MakeServiceAccountCredentials(
                            [](const std::filesystem::path& file) {
                                std::ifstream ifs(file);
                                if (!ifs.is_open())
                                    throw std::runtime_error(
                                        "Cannot open key file for TTS");
                                return std::string(
                                    std::istreambuf_iterator<char>(ifs.rdbuf()),
                                    {});
                            }(keyfile))))}
        {
            setvoice(voice);
            audio.set_audio_encoding(texttospeech::LINEAR16);
            handler->log(logs::level::info,
                         "Created gcloud tts [langcode/langname/gender]: " +
                             getparams());
        }

        ~Google()
        {
            handler->log(logs::level::info,
                         "Released gcloud tts [langcode/langname/gender]: " +
                             getparams());
        }

        std::string getaudio(const std::string& text)
        {
            input.set_text(text);
            return client.SynthesizeSpeech(input, params, audio)
                ->audio_content();
        }

        std::string getaudio(const std::string& text, const voice_t& tmpvoice)
        {
            const auto mainVoice{voice};
            setvoice(tmpvoice);
            auto response = getaudio(text);
            handler->log(logs::level::debug, "Text spoken as " + getparams());
            setvoice(mainVoice);
            return response;
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
            const auto& [code, name, gender] = voiceMap.contains(voice)
                                                   ? voiceMap.at(voice)
                                                   : voiceMap.at(defaultvoice);

            params.set_language_code(code);
            params.set_name(name);
            params.set_ssml_gender(gender);
        }

        std::string getparams() const
        {
            auto gender = params.ssml_gender();
            return params.language_code() + "/" + params.name() + "/" +
                   std::string(gender == ssmlgender::MALE     ? "male"
                               : gender == ssmlgender::FEMALE ? "female"
                                                              : "unknown");
        }

      private:
        const Handler* handler;
        texttospeech_type::TextToSpeechClient client;
        texttospeech::SynthesisInput input;
        texttospeech::VoiceSelectionParams params;
        texttospeech::AudioConfig audio;
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

bool TextToVoice::waitspoken()
{
    return handler->waitspoken();
}

voice_t TextToVoice::getvoice()
{
    return handler->getvoice();
}

void TextToVoice::setvoice(const voice_t& voice)
{
    handler->setvoice(voice);
}

} // namespace tts::googlecloud
