#pragma once

#include <functional>
#include <memory>
#include <string>

namespace speech::helpers
{

class HelpersIf
{
  public:
    virtual ~HelpersIf()
    {}
    virtual bool uploadData(const std::string&, const std::string&,
                            std::string&) = 0;
    virtual bool downloadFile(const std::string&, const std::string&,
                              const std::string&) = 0;
    virtual bool uploadFile(const std::string&, const std::string&,
                            std::string&) = 0;
    virtual bool createasync(std::function<void()>&&) = 0;
    virtual bool waitasync() = 0;
    virtual bool killasync() = 0;
};

class Helpers : public HelpersIf
{
  public:
    bool uploadData(const std::string&, const std::string&,
                    std::string&) override;
    bool uploadFile(const std::string&, const std::string&,
                    std::string&) override;
    bool downloadFile(const std::string&, const std::string&,
                      const std::string&) override;
    bool createasync(std::function<void()>&&) override;
    bool waitasync() override;
    bool killasync() override;

  private:
    friend class HelpersFactory;
    Helpers() = default;

    bool isasyncrunning() const;
};

class HelpersFactory
{
  public:
    HelpersFactory() = delete;
    HelpersFactory(const HelpersFactory&) = delete;
    HelpersFactory(HelpersFactory&&) = delete;
    HelpersFactory& operator=(const HelpersFactory&) = delete;
    HelpersFactory& operator=(HelpersFactory&&) = delete;

    static std::shared_ptr<HelpersIf> create();
};

std::string str(const auto& value)
{
    if constexpr (std::is_same<const std::string&, decltype(value)>())
        return value;
    else
        return std::to_string(value);
}

std::string getrecordingcmd(const std::string&, const std::string&);

} // namespace speech::helpers
