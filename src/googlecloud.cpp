#include "tts/interfaces/googlecloud.hpp"

#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/project.h"
#include "google/cloud/speech/v2/speech_client.h"
#include "google/cloud/texttospeech/v1/text_to_speech_client.h"

#include "shell/interfaces/linux/bash/shell.hpp"
#include "tts/helpers.hpp"

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

using namespace helpers;
using namespace std::string_literals;
using ssmlgender = texttospeech::SsmlVoiceGender;
using recognizer_t =
    std::tuple<std::string, std::string, std::string, std::string>;

namespace speech = google::cloud::speech::v2;
namespace speech_type = google::cloud::speech_v2;

std::string getrecordingcmd(const std::string& file,
                            const std::string& interval)
{
    // "sox --no-show-progress --type alsa default --rate 16k --channels 1
    // #file# silence -l 1 1 2.0% 1 2.0t 1.0% pad 0.3 0.2";
    auto ivtime = !interval.empty() ? interval : "2.0t";
    return "sox --no-show-progress --type alsa default --rate 16k --channels "
           "1 " +
           file + " silence -l 1 1 2.0% 1 " + ivtime + " 1.0% pad 0.3 0.2";
}

static const std::filesystem::path keyFile = "../conf/key.json";
static const std::filesystem::path audioDirectory = "audio";
static const std::filesystem::path playbackName = "playback.mp3";
static const std::filesystem::path recordingName = "recording.wav";
static const auto audioFilePath = audioDirectory / recordingName;
static const std::string playAudioCmd =
    "play --no-show-progress " + (audioDirectory / playbackName).native() +
    " --type alsa";
static auto recordAudioCmd = getrecordingcmd(audioFilePath.native(), {});
// static constexpr const char* keyEnvVar = "GOOGLE_APPLICATION_CREDENTIALS";
// static const recognizer_t recognizerInfo = {"lukaszsttproject",
// "europe-west4", "stt-region", "chirp_2"};
static const recognizer_t recognizerInfo = {"lukaszsttproject", "eu",
                                            "stt-global", "short"};

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

// enum class language
// {
//     polish,
//     english,
//     german
// };

static const std::unordered_map<language, std::string> langMap = {
    {language::polish, "pl-PL"},
    {language::english, "en-US"},
    {language::german, "de-DE"}};

struct TextToVoice::Handler : public std::enable_shared_from_this<Handler>
{
  public:
    explicit Handler(const configmin_t& config) :
        logif{std::get<std::shared_ptr<logs::LogIf>>(config)},
        shell{shell::Factory::create<shell::lnx::bash::Shell>()},
        helpers{helpers::HelpersFactory::create()},
        filesystem{this, audioDirectory / playbackName},
        google{this, keyFile, std::get<voice_t>(config)}
    {}

    explicit Handler(const configall_t& config) :
        logif{std::get<std::shared_ptr<logs::LogIf>>(config)},
        shell{std::get<std::shared_ptr<shell::ShellIf>>(config)},
        helpers{std::get<std::shared_ptr<helpers::HelpersIf>>(config)},
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

    transcript_t listen()
    {
        while (true)
        {
            log(logs::level::debug, "Recording voice by: " + recordAudioCmd);
            shell->run(recordAudioCmd);
            google.uploadaudio(audioFilePath);
            if (auto transcript = google.gettranscript())
                return *transcript;
        }
        return {};
    }

    transcript_t listen(language lang)
    {
        while (true)
        {
            log(logs::level::debug, "Recording voice by: " + recordAudioCmd);
            shell->run(recordAudioCmd);
            google.uploadaudio(audioFilePath);
            if (auto transcript = google.gettranscript(lang))
                return *transcript;
        }
        return {};
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
                            }(keyfile))))},
            clientstt{speech_type::MakeSpeechConnection(
                std::get<1>(recognizerInfo),
                google::cloud::Options{}
                    .set<google::cloud::UnifiedCredentialsOption>(
                        google::cloud::MakeServiceAccountCredentials(
                            [](const std::filesystem::path& file) {
                                std::ifstream ifs(file);
                                if (!ifs.is_open())
                                    throw std::runtime_error(
                                        "Cannot open key file for STT");
                                return std::string(
                                    std::istreambuf_iterator<char>(ifs.rdbuf()),
                                    {});
                            }(keyfile))))}
        {
            setvoice(voice);
            audio.set_audio_encoding(texttospeech::LINEAR16);

            const auto& config = request.mutable_config();
            *config->mutable_auto_decoding_config() = {};
            config->set_model(std::get<3>(recognizerInfo));
            request.set_recognizer("projects/" + std::get<0>(recognizerInfo) +
                                   "/locations/" + std::get<1>(recognizerInfo) +
                                   "/recognizers/" +
                                   std::get<2>(recognizerInfo));
            setlang(language::polish);

            // handler->log(logs::level::info,
            //              "Created v2::gcloud stt [langcode/langid]: " +
            //                  getparams());

            handler->log(logs::level::info,
                         "Created gcloud speech [langcode/langname/gender]: " +
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

        void uploadaudio(const std::filesystem::path& filepath)
        {
            request.set_content([](const std::filesystem::path& path) {
                std::ifstream ifs(path, std::ios::in | std::ios::binary);
                if (!ifs.is_open())
                    throw std::runtime_error("Cannot open audio file for STT");
                return std::string(std::istreambuf_iterator<char>(ifs.rdbuf()),
                                   {});
            }(filepath));
            handler->log(logs::level::debug,
                         "Uploaded audio to stt engine: " + filepath.native());
        }

        std::optional<transcript_t> gettranscript()
        {
            if (auto response = clientstt.Recognize(request))
            {
                handler->log(logs::level::debug,
                             "Received results: " +
                                 str(response->results_size()));
                for (const auto& result : response->results())
                {
                    handler->log(logs::level::debug,
                                 "Received alternatives: " +
                                     str(result.alternatives_size()));
                    for (const auto& alternative : result.alternatives())
                    {
                        auto text = alternative.transcript();
                        if (text.empty())
                            continue;
                        auto confid = alternative.confidence();
                        auto quality = (uint32_t)std::lround(100 * confid);
                        handler->log(logs::level::debug,
                                     "Returning transcript [text/confid]: '" +
                                         text + "'/" + str(confid));
                        return std::make_optional<transcript_t>(std::move(text),
                                                                quality);
                    }
                }
            }
            handler->log(logs::level::debug, "Cannot recognize transcript");
            return std::nullopt;
        }

        std::optional<transcript_t> gettranscript(language tmplang)
        {
            const auto mainlang{lang};
            setlang(tmplang);
            auto transcript = gettranscript();
            handler->log(logs::level::debug,
                         "Speech detected for " + getparams());
            setlang(mainlang);
            return transcript;
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
        speech_type::SpeechClient clientstt;
        speech::RecognizeRequest request;
        texttospeech::SynthesisInput input;
        texttospeech::VoiceSelectionParams params;
        texttospeech::AudioConfig audio;
        voice_t voice;
        language lang;

        void setlang(language lang)
        {
            auto langId = [this](language newlang) {
                this->lang = newlang;
                static constexpr auto deflang{language::polish};
                return langMap.contains(newlang) ? langMap.at(newlang)
                                                 : langMap.at(deflang);
            }(lang);

            const auto& config = request.mutable_config();
            config->language_codes_size()
                ? config->set_language_codes(0, langId)
                : config->add_language_codes(langId);
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

transcript_t TextToVoice::listen()
{
    return handler->listen();
}

transcript_t TextToVoice::listen(language lang)
{
    return handler->listen(lang);
}

} // namespace tts::googlecloud
