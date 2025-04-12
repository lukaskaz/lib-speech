#pragma once

#include <functional>
#include <memory>
#include <string>

namespace helpers
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
    virtual bool createasync(std::function<void()>&&) = 0;
};

class Helpers : public HelpersIf
{
  public:
    bool uploadData(const std::string&, const std::string&,
                    std::string&) override;
    bool downloadFile(const std::string&, const std::string&,
                      const std::string&) override;

    bool createasync(std::function<void()>&&);

  private:
    friend class HelpersFactory;
    Helpers() = default;
};

class HelpersFactory
{
  public:
    static std::shared_ptr<HelpersIf> create()
    {
        return std::shared_ptr<Helpers>(new Helpers());
    }
};

std::string str(const auto& value)
{
    if constexpr (std::is_same<const std::string&, decltype(value)>())
        return value;
    else
        return std::to_string(value);
}

} // namespace helpers
