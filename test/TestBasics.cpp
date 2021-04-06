#include "catch.hpp"

#include <mocxx/Mocxx.hpp>

// std
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif

// stl
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if defined(__clang__)
#pragma clang optimize off
#include <filesystem>
#pragma clang optimize on
#elif defined(__GNUC__)
#pragma GCC push_options
#pragma GCC optimize ("O0")
#include <experimental/filesystem>
#pragma GCC pop_options
#elif defined(_WIN32)
#include <filesystem>
#else
#error "Unknown compiler used"
#endif

using namespace mocxx;

std::vector<int>
OverloadSet(std::vector<int> vector)
{
  return vector;
}

void
OverloadSet()
{}

std::unique_ptr<int>
UniqueInt()
{
  return nullptr;
}

int
TrivialPlus(int x, int y)
{
  return x + y;
}

int
TrivialMinus(int x, int y)
{
  return x - y;
}

const int&
ConstRefPlus(const int& a, const int& b)
{
  static const int c = a + b;
  return c;
}

struct Name
{
  using size_type = std::string::size_type;

  Name(std::string value = "") : name(value) {}

  static size_type static_size() { return 1337; }

  const Name& self() const { return *this; }
  Name& self() { return *this; }

  Name&& take() { return std::move(*this); }

  std::optional<std::vector<Name>> replicate(std::int32_t times) const
  {
    if (times == 0) {
      return std::nullopt;
    }

    std::vector<Name> result;
    while (times-- > 0) {
      result.push_back(*this);
    }
    return result;
  }

  size_type size() const { return name.size(); }
  size_type size() { return name.size(); }

  std::string name;
};

using ConstName = const Name;

TEST_CASE("Mocxx follows RAII", "[Mocxx]")
{
  REQUIRE(OverloadSet(std::vector<int>{ 3, 2, 1 }) ==
          std::vector<int>{ 3, 2, 1 });

  {
    Mocxx mocxx;

    REQUIRE(mocxx.Replace(
      [](std::vector<int> a) {
        std::sort(a.begin(), a.end());
        return a;
      },
      OverloadSet));

    // Without the replacement target it is hard to reason about overloaded
    // functions, here we provide the exact type
    REQUIRE(
      mocxx.IsReplaced((std::vector<int>(*)(std::vector<int>))OverloadSet));

    REQUIRE(OverloadSet(std::vector<int>{ 3, 2, 1 }) ==
            std::vector<int>{ 1, 2, 3 });
  }

  REQUIRE(OverloadSet(std::vector<int>{ 3, 2, 1 }) ==
          std::vector<int>{ 3, 2, 1 });
}

TEST_CASE("Mocxx::Restore() in the replacement", "[Mocxx]")
{
  Mocxx mocxx;

  REQUIRE(mocxx.Replace(
    [&mocxx](int x, int y) {
      mocxx.Restore(TrivialPlus);
      return x * y + TrivialPlus(x, y);
    },
    &TrivialPlus));

  REQUIRE(mocxx.IsReplaced(TrivialPlus));
  REQUIRE(TrivialPlus(3, 3) == 15);
  REQUIRE_FALSE(mocxx.IsReplaced(TrivialPlus));
}

TEST_CASE("Mocxx::Replace() with identical signatures", "[Mocxx]")
{
  REQUIRE(TrivialPlus(3, 2) == 5);
  REQUIRE(TrivialMinus(2, 1) == 1);

  Mocxx mocxx;

  REQUIRE(mocxx.Replace([](int x, int y) { return x * y; }, TrivialPlus));
  REQUIRE(TrivialPlus(3, 2) == 6);

  REQUIRE(mocxx.Replace([](int x, int y) { return x + y; }, TrivialMinus));
  REQUIRE(TrivialMinus(2, 1) == 3);
  REQUIRE(TrivialPlus(3, 2) == 6);
}

#ifndef _WIN32
TEST_CASE("Mocxx::Replace() by name", "[Mocxx]")
{
  REQUIRE(atof("1.0") == 1.0);

  Mocxx mocxx;
  REQUIRE(mocxx.Replace([]() { return 0.0; }, "atof"));

  REQUIRE(atof("1.0") == 0.0);

  REQUIRE(mocxx.Restore("atof"));
  REQUIRE(atof("1.0") == 1.0);
}
#endif

TEST_CASE("Mocxx::Replace() system functions", "[Mocxx]")
{
#ifndef _WIN32
  SECTION("replacing open")
  {
    Mocxx mocxx;

    std::string outFile;
    int outMode;
    REQUIRE(mocxx.Replace(
      [&outFile, &outMode](const char* file, int mode, ...) -> int {
        outFile = file, outMode = mode;
        return 1337;
      },
      open));

    auto file = open("/etc/hosts", O_RDONLY);
    REQUIRE(file == 1337);
    REQUIRE(outFile == "/etc/hosts");
    REQUIRE(outMode == O_RDONLY);

    file = open("/etc/hosts", O_RDONLY);
    REQUIRE(file == 1337);
    REQUIRE(outFile == "/etc/hosts");
    REQUIRE(outMode == O_RDONLY);

    REQUIRE(mocxx.Restore(open));

    REQUIRE(mocxx.Replace(
      [](const char* file, int mode, ...) -> int { return 0; }, open));
    file = open("/etc/hosts", O_RDONLY);
    REQUIRE(file == 0);
  }
#endif

  SECTION("replacing std::filesystem")
  {
    REQUIRE_FALSE(std::filesystem::exists("doesn't exist"));

    Mocxx mocxx;
    REQUIRE(mocxx.Replace([](const std::filesystem::path& p) { return true; },
                          std::filesystem::exists));

    REQUIRE(std::filesystem::exists("how about now?"));

    std::error_code error;
    REQUIRE_FALSE(
      std::filesystem::exists("another overload still active", error));
  }
}

TEST_CASE("Mocxx::ReplaceMember()", "[Mocxx]")
{
#if !defined(_WIN32) || defined(_WIN64)
  SECTION("generating result")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.ReplaceMember(
      [](Name* foo) -> Name::size_type { return foo->name.size() + 1; },
      &Name::size));

    REQUIRE(mocxx.ReplaceMember(
      [](const Name* foo) -> Name::size_type { return foo->name.size() + 2; },
      &Name::size));

    REQUIRE(Name{}.size() == 1);
    REQUIRE(ConstName{}.size() == 2);
  }
  #endif

  SECTION("replacing result with lambda")
  {
    Mocxx mocxx;

    Name result("Eastre");
    REQUIRE(mocxx.ReplaceMember(
      [&result](Name* self) -> Name& { return result; }, &Name::self));

    ConstName cresult("Uller");
    REQUIRE(mocxx.ReplaceMember(
      [&cresult](const Name* self) -> const Name& { return cresult; },
      &Name::self));

    REQUIRE(&Name{}.self() == &result);
    REQUIRE(&ConstName{}.self() == &cresult);
  }

  SECTION("replacing result with mutable lambdas")
  {
    Mocxx mocxx;

    Name result("Skadi");
    REQUIRE(mocxx.ReplaceMember(
      [&result](Name* self) mutable -> Name& { return result; }, &Name::self));

    ConstName cresult("Elli");
    REQUIRE(mocxx.ReplaceMember(
      [&cresult](const Name* self) mutable -> const Name& { return cresult; },
      &Name::self));

    REQUIRE(&Name{}.self() == &result);
    REQUIRE(&ConstName{}.self() == &cresult);
  }

  SECTION("modifying arguments")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.ReplaceMember(
      [&mocxx](const Name* self, std::int32_t times) {
        mocxx.Restore(&Name::replicate);
        return self->replicate(0);
      },
      &Name::replicate));
  }
}

TEST_CASE("Mocxx::Result()", "[Mocxx]")
{
  SECTION("value and target result types are trivially")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.Result(13, &TrivialPlus));
    REQUIRE(mocxx.IsReplaced(TrivialPlus));

    REQUIRE(TrivialPlus(3, 3) == 13);
  }

  SECTION("value and target result types are const& in free function")
  {
    Mocxx mocxx;

    const int& result = 13;
    REQUIRE(mocxx.Result(result, &ConstRefPlus));
    REQUIRE(mocxx.IsReplaced(ConstRefPlus));

    REQUIRE(ConstRefPlus(3, 3) == result);
  }

  SECTION("value is trivial, result type is const& in free function")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.Result(13, &ConstRefPlus));
    REQUIRE(mocxx.IsReplaced(ConstRefPlus));

    REQUIRE(ConstRefPlus(3, 3) == 13);
  }

  SECTION("value is const&, result type is trivial in free function")
  {
    Mocxx mocxx;

    const int& result = 13;
    REQUIRE(mocxx.Result(result, &TrivialPlus));
    REQUIRE(mocxx.IsReplaced(TrivialPlus));

    REQUIRE(TrivialPlus(3, 3) == result);
  }
}

TEST_CASE("Mocxx::ResultOnce() makes replacement execute only once", "[Mocxx]")
{
  SECTION("works on trivials")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.ResultOnce(13, TrivialPlus));

    REQUIRE(mocxx.IsReplaced(TrivialPlus));
    REQUIRE(TrivialPlus(3, 3) == 13);
    REQUIRE(TrivialPlus(3, 3) == 6);
    REQUIRE_FALSE(mocxx.IsReplaced(TrivialPlus));
  }

  SECTION("works with references to trivial types")
  {
    Mocxx mocxx;

    int value = 13;
    REQUIRE(mocxx.ResultOnce(value, ConstRefPlus));

    REQUIRE(mocxx.IsReplaced(ConstRefPlus));
    auto& returned = ConstRefPlus(3, 3);
    REQUIRE(&returned == &value);
    REQUIRE(ConstRefPlus(3, 3) == 6);
    REQUIRE_FALSE(mocxx.IsReplaced(ConstRefPlus));
  }

  SECTION("works with move only values")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.ResultOnce(std::make_unique<int>(13), UniqueInt));

    REQUIRE(mocxx.IsReplaced(UniqueInt));
    REQUIRE(*UniqueInt() == 13);
    REQUIRE(UniqueInt().get() == nullptr);
    REQUIRE_FALSE(mocxx.IsReplaced(UniqueInt));
  }
}

TEST_CASE("Mocxx::ResultMember()", "[Mocxx]")
{
  SECTION("works on & and const& types")
  {
    Mocxx mocxx;

    Name cresult("Vidar");
    REQUIRE(mocxx.ResultMember(cresult,
                               (const Name& (Name::*)() const) & Name::self));

    Name result("Bragi");
    REQUIRE(mocxx.ResultMember(result, (Name & (Name::*)()) & Name::self));

    REQUIRE(ConstName{ "Alaisiagae" }.self().name == "Vidar");
    cresult = Name("Heimdall");
    REQUIRE(ConstName{ "Alaisiagae" }.self().name == "Heimdall");

    REQUIRE(Name{ "Alaisiagae" }.self().name == "Bragi");
  }
}

TEST_CASE("Mocxx::ResultGenerator()", "[Mocxx]")
{
  SECTION("value and target result types are trivially")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.ResultGenerator([] { return 13; }, &TrivialPlus));
    REQUIRE(mocxx.IsReplaced(TrivialPlus));

    REQUIRE(TrivialPlus(3, 3) == 13);
  }

  SECTION("value and target result types are const& in free function")
  {
    Mocxx mocxx;

    const int& result = 13;
    REQUIRE(mocxx.ResultGenerator([&]() -> const int& { return result; },
                                  &ConstRefPlus));
    REQUIRE(mocxx.IsReplaced(ConstRefPlus));

    REQUIRE(ConstRefPlus(3, 3) == result);
  }

  SECTION("value and target result types are & in free function")
  {
    Mocxx mocxx;

    int result = 13;
    REQUIRE(
      mocxx.ResultGenerator([&]() -> int& { return result; }, &ConstRefPlus));
    REQUIRE(mocxx.IsReplaced(ConstRefPlus));

    REQUIRE(ConstRefPlus(3, 3) == result);
  }
}

TEST_CASE("Mocxx::ResultGeneratorMember()", "[Mocxx]")
{
  SECTION("generator can return trivial result")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.Replace([]() mutable -> Name::size_type { return 13; },
                          &Name::static_size));
    REQUIRE(mocxx.ResultGeneratorMember(
      [] { return 15; }, (Name::size_type(Name::*)() const) & Name::size));
    REQUIRE(mocxx.ResultGeneratorMember(
      [] { return 16; }, (Name::size_type(Name::*)()) & Name::size));

    REQUIRE(Name::static_size() == 13);

    REQUIRE(ConstName{ "Njord" }.size() == 15);
    REQUIRE(ConstName{ "Syn" }.size() == 15);
    REQUIRE(ConstName{ "Aegir" }.size() == 15);

    REQUIRE(Name{ "Freya" }.size() == 16);
    REQUIRE(Name{ "Vali" }.size() == 16);
    REQUIRE(Name{ "Vidar" }.size() == 16);
  }

  SECTION("generator can return trivial result")
  {
    Mocxx mocxx;

    REQUIRE(mocxx.Replace([]() mutable -> Name::size_type { return 13; },
                          &Name::static_size));
    REQUIRE(mocxx.ResultGeneratorMember(
      [] { return 15; }, (Name::size_type(Name::*)() const) & Name::size));
    REQUIRE(mocxx.ResultGeneratorMember(
      [] { return 16; }, (Name::size_type(Name::*)()) & Name::size));

    REQUIRE(Name::static_size() == 13);

    REQUIRE(ConstName{ "Njord" }.size() == 15);
    REQUIRE(ConstName{ "Syn" }.size() == 15);
    REQUIRE(ConstName{ "Aegir" }.size() == 15);

    REQUIRE(Name{ "Freya" }.size() == 16);
    REQUIRE(Name{ "Vali" }.size() == 16);
    REQUIRE(Name{ "Vidar" }.size() == 16);
  }
}
