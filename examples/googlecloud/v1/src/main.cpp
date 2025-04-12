#include "logs/interfaces/console/logs.hpp"
#include "logs/interfaces/group/logs.hpp"
#include "logs/interfaces/storage/logs.hpp"
#include "speech/stt/interfaces/v1/googlecloud.hpp"
#include "speech/tts/interfaces/googlecloud.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    try
    {
        if (argc > 1)
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
                tts::TextToVoiceFactory::create<tts::googlecloud::TextToVoice,
                                                tts::googlecloud::configmin_t>(
                    {{tts::language::polish, tts::gender::female, 1}, logif});

            auto stt = stt::TextFromVoiceFactory::create<
                stt::v1::googlecloud::TextFromVoice,
                stt::v1::googlecloud::configmin_t>(
                {stt::language::polish, "1.0t", logif});

            tts->speak("Jestem twoim zwykłym asystentem, powiedz coś");
            auto spoken = stt->listen();
            tts->speak("Jestem pewna w " + helpers::str(std::get<1>(spoken)) +
                       "%, że powiedziałeś: '" + std::get<0>(spoken) + "'");
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
            tts::TextToVoiceFactory::create<tts::googlecloud::TextToVoice,
                                            tts::googlecloud::configmin_t>(
                {{tts::language::english, tts::gender::female, 1}, logif})
                ->speak("Hi, this is second speech!");
            tts->speak("Tschüss, wie gehts du?",
                       {tts::language::german, tts::gender::female, 1});
            tts->speak("To wszystko, dzięki :)");
        }
    }
    catch (std::exception& err)
    {
        std::cerr << "[ERROR] " << err.what() << '\n';
    }

    return 0;
}
