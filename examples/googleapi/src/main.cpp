#include "logs/interfaces/console/logs.hpp"
#include "logs/interfaces/group/logs.hpp"
#include "logs/interfaces/storage/logs.hpp"
#include "speech/stt/interfaces/v2/googleapi.hpp"
#include "speech/tts/interfaces/googleapi.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2)
        {
            auto loglvl =
                (bool)atoi(argv[1]) ? logs::level::debug : logs::level::info;

            auto logconsole = logs::Factory::create<logs::console::Log,
                                                    logs::console::config_t>(
                {loglvl, logs::time::hide, logs::tags::hide});
            auto logstorage = logs::Factory::create<logs::storage::Log,
                                                    logs::storage::config_t>(
                {loglvl, logs::time::show, logs::tags::show, {}});
            auto logif =
                logs::Factory::create<logs::group::Log, logs::group::config_t>(
                    {logconsole, logstorage});

            using namespace tts::googleapi;
            auto tts =
                tts::TextToVoiceFactory::create<TextToVoice, configmin_t>(
                    {{tts::language::polish, tts::gender::female, 1}, logif});

            tts->speak("Jestem twoim zwykłym asystentem, co mam zrobić?");
            tts->speakasync("Jestem twoim asynk asystentem, co mam zrobić?");
            sleep(1);
            tts->speakasync("Jestem twoim asynk asystentem, co mam zrobić?");
            sleep(1);
            tts->speak("Jestem twoim zwykłym asystentem, co mam zrobić?");
            tts->waitspoken();
            tts->speak("Jestem twoim zwykłym asystentem, co mam zrobić?");

            tts->speak("Jestem twoim asystentem, co mam zrobić?",
                       {tts::language::polish, tts::gender::female, 1});
            tts->speak("Jestem twoim asystentem, co mam zrobić?",
                       {tts::language::polish, tts::gender::female, 2});
            tts->speak("Jestem twoim asystentem, co mam zrobić?",
                       {tts::language::polish, tts::gender::female, 3});
            tts->speak("Jestem twoim asystentem, co mam zrobić?",
                       {tts::language::polish, tts::gender::male, 1});
            tts->speak("Jestem twoim asystentem, co mam zrobić?",
                       {tts::language::polish, tts::gender::male, 2});
            tts::TextToVoiceFactory::create<TextToVoice, configmin_t>(
                {{tts::language::english, tts::gender::female, 1}, logif})
                ->speak("Hi, this is second speech!");
            tts->speak("Tschüss, wie gehts du?",
                       {tts::language::german, tts::gender::female, 1});
            tts->speak("To wszystko, dzięki :)");
        }
        if (argc > 2)
        {
            auto loglvl =
                (bool)atoi(argv[1]) ? logs::level::debug : logs::level::info;

            auto logconsole = logs::Factory::create<logs::console::Log,
                                                    logs::console::config_t>(
                {loglvl, logs::time::hide, logs::tags::hide});
            auto logstorage = logs::Factory::create<logs::storage::Log,
                                                    logs::storage::config_t>(
                {loglvl, logs::time::show, logs::tags::show, {}});
            auto logif =
                logs::Factory::create<logs::group::Log, logs::group::config_t>(
                    {logconsole, logstorage});

            auto tts =
                tts::TextToVoiceFactory::create<tts::googleapi::TextToVoice,
                                                tts::googleapi::configmin_t>(
                    {{tts::language::polish, tts::gender::female, 1}, logif});

            auto stt = stt::TextFromVoiceFactory::create<
                stt::v2::googleapi::TextFromVoice,
                stt::v2::googleapi::configmin_t>(
                {stt::language::polish, "1.0t", logif});

            tts->speak("Jestem twoim zwykłym asystentem, powiedz coś");
            auto spoken = stt->listen();
            tts->speak("Jestem pewna w " +
                       speech::helpers::str(std::get<1>(spoken)) +
                       "%, że powiedziałeś: " + std::get<0>(spoken));
        }
    }
    catch (std::exception& err)
    {
        std::cerr << "[ERROR] " << err.what() << '\n';
    }

    return 0;
}
