#include "tts/helpers.hpp"

#include "tts/interfaces/texttovoice.hpp"

#include <curl/curl.h>
#include <curl/easy.h>

#include <fstream>
#include <future>
#include <vector>

namespace helpers
{

std::future<void> async;

static size_t DownloadWriteFunction(char* data, size_t size, size_t nmemb,
                                    std::ofstream* ofs)
{
    size_t datasize{size * nmemb};
    ofs->write(data, datasize);
    return datasize;
}

bool Helpers::downloadFile(const std::string& url, const std::string& text,
                           const std::string& filepath)
{
    CURLcode res{CURLE_FAILED_INIT};
    if (auto curl = curl_easy_init(); curl != nullptr)
    {
        std::ofstream ofs(filepath, std::ios::out | std::ofstream::binary);
        auto escapedtext =
            curl_easy_escape(curl, text.c_str(), (int)text.length());
        curl_easy_setopt(curl, CURLOPT_URL, (url + escapedtext).c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DownloadWriteFunction);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
        res = curl_easy_perform(curl); // synchronous file download
        curl_free(escapedtext);
        curl_easy_cleanup(curl);
    }
    return res == CURLE_OK;
}

static size_t UploadWriteFunction(char* data, size_t size, size_t nmemb,
                                  std::string* output)
{
    size_t datasize{size * nmemb};
    output->append(data, datasize);
    return datasize;
}

bool Helpers::uploadData(const std::string& url, const std::string& datastr,
                         std::string& output)
{
    CURLcode res{CURLE_FAILED_INIT};
    if (auto curl = curl_easy_init(); curl != nullptr)
    {
        static constexpr auto header{"Content-Type: application/json"};
        if (curl_slist * hlist{}; (hlist = curl_slist_append(hlist, header)))
        {
            std::vector<char> data(datastr.begin(), datastr.end());
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, &data[0]);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, UploadWriteFunction);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
            res = curl_easy_perform(curl); // synchronous file upload
            curl_slist_free_all(hlist);
        }
        curl_easy_cleanup(curl);
    }
    return res == CURLE_OK;
}

bool Helpers::createasync(std::function<void()>&& func)
{
    if (async.valid())
    {
        tts::TextToVoiceIf::kill();
        async.wait();
    }
    async = std::async(std::launch::async, std::move(func));
    return true;
}

} // namespace helpers
