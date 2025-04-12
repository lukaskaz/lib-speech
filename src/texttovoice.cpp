#include "tts/interfaces/texttovoice.hpp"

#include "shell/interfaces/linux/bash/shell.hpp"

namespace tts
{

void tts::TextToVoiceIf::kill()
{
    shell::Factory::create<shell::lnx::bash::Shell>()->run(
        "killall -s KILL play");
}

} // namespace tts
