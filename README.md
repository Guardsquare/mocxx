<!-- TODO: logo -->

# Mocxx

[![license](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](https://choosealicense.com/licenses/gpl-3.0/)
![version](https://img.shields.io/badge/version-0.1.0-g)
[![tests-macos-clang](https://github.com/Guardsquare/mocxx/workflows/MacOS%2FClang/badge.svg)](https://github.com/Guardsquare/mocxx/actions?query=workflow%3AMacOS%2FClang)
[![tests-linux-clang](https://github.com/Guardsquare/mocxx/workflows/Linux%2FClang/badge.svg)](https://github.com/Guardsquare/mocxx/actions?query=workflow%3ALinux%2FClang)
[![tests-linux-gcc](https://github.com/Guardsquare/mocxx/workflows/Linux%2FGCC/badge.svg)](https://github.com/Guardsquare/mocxx/actions?query=workflow%3ALinux%2FGCC)

A versatile [C++](https://www.stroustrup.com/C++.html) function mocking
framework. It replaces a target function with the provided implementation, and
integrates well with existing testing and mocking frameworks.

[Article](https://tech.guardsquare.com/posts/mocxx-the-mocking-tool/) about how it works.

## Features

A few things that might catch your eye:

- No macros
- Virtually no source code modification
- State-of-the-art code injection (thanks to [Frida](https://github.com/frida))
- System functions mocking, for example **open**
- Type safety
- RAII friendly

## Examples

Mocking [std::filesystem](https://en.cppreference.com/w/cpp/filesystem):
```cpp
Mocxx mocxx;

// Lambda is typed and allows to resolve function overload set
mocxx.ReplaceOnce([&](const std::filesystem::path& p) { return true; },
                  std::filesystem::exists);

std::string file = "/this/file/now/exists";

// Returns true
std::filesystem::exists(file);

// Returns false, because of ReplaceOnce
std::filesystem::exists(file);
```

This example could be a good start for implementing an in-process fake file
system. We haven't tried yet. Dare to try?

Mocxx can also replace functions by name:

```cpp
mocxx.Replace([]() { return 0.0; }, "atof");

// Always true
atof("1.0") == 0.0;
```

Replacing member functions is also straightforward:

```cpp
struct Name
{
  using SizeType = std::string::SizeType;

  SizeType Size() const { return name.size(); }
  SizeType Size() { return name.size(); }

  std::string name;
};

// First argument to such lambdas is the type target (Name in this case) this
// member function belongs. Type constness decides which overload to use.
mocxx.ReplaceMember([](Name* foo) -> Name::SizeType { return 13; }, &Name::Size);
```

In many cases you might want to simply replace the result:

```cpp
mocxx.Result(0, getpid);
```

Mocxx of course works with more complex types than just integers:

```cpp
std::optional<std:vector<std::string>>
Repeat(const std::string& string, std::int32_t times)
{
  if (times == 0) {
    return std::nullopt;
  }

  std::vector<std::string> result;
  while (times --> 0) {
    result.push_back(string);
  }

  return result;
}

mocxx.Result(std::nullopt, Repeat);

mocxx.Result(std::vector<std::string>(), Repeat);
```

Keep in mind that if you pass a named value, a local variable, function
arguments etc. you are passing it as a reference. If Mocxx leaves the context
of this value you might end up with a runtime exception in the best case, and
in the worst you might just see garbage being returned by the replacement.

```cpp
Mocxx CreateFilesystemExistsMocks(bool result) {
  Mocxx mocxx;
  mocxx.Result(result, 
               (bool(*)(const std::filesystem::path&))std::filesystem::exists);
  mocxx.Result(result,
               (bool(*)(const std::filesystem::path&, std::error_code&))std::filesystem::exists);
  return mocxx;
}

auto mocxx = CreateFilesystemExistsMocks(true);

if (std::filesystem::exists("/dev/null")) {
  // Whether control enters this block is undefined
}
```

In case you don't need the parameters passed to the replaced function, but
still want to generate a new value every call, you can pass a generator, which
is a lambda with no arguments returning a result convertible to the result of
the replaced function:

```cpp
std::random_device rd;
std::uniform_int_distribution<int> dist(0, 9);

int notSoRandom = 0;

mocxx.ReplaceGenerator([&]() mutable { return (notSoRandom %= 9); },
                       &std::uniform_int_distribution::operator());
```

## Limitations

As all good things, this one does have some drawbacks. One major issue you
might encounter while using Mocxx is that your functions are not being
replaced, this is especially true for **system functions**. This section will
go over most common problems.

### Optimisations

C++ is a powerful language and it is usually packaged within a powerful
compiler that tries to optimise every bit of your code. The function you are
trying to replace might be **inlined**, or simply removed, because it is no
longer required for the program. This is especially evident in the very first
example this document provides. In order to successfully replace
**std::filesystem** API you want to wrap its header inclusion with the following
pragma (clang only).

```cpp
#pragma clang optimize off
#include <filesystem>
#pragma clang optimize on
```
### Overloading sets

The problem with overloading sets in C++ is that they are not first-class
citizens, you cannot bind an overloading set to a name, or pass it through a
function call. Overloading sets are always resolved at call site. At the moment
there is no straightforward solution to this problem, and you have to provide
target function type for lambdaless API.

### Templates

The major issue with template functions is the fact that they are not actual
functions. Mocxx is a runtime tool, it can only replace a function that has an
address. What this means is that you first need to instantiate the template
function, and then pass its address to the tool.

### Virtual methods

The nature of virtual methods (aka dynamic dispatch, or late method binding) in
C++ makes it impossible to home in on the target member function using language
means, which is what Mocxx requires. You can of course pass in the virtual
member function pointer, but it contains no information about the actual
function. The support for virtual member functions can be added to Mocxx, but
not in a portable way. A possible solution is explained in the [article](TODO).

### Forbidden Mocks

This tool makes use of some STL types, such as **std::variant**,
**std::string**, etc. To save your sanity, suppress the urge to mock commonly
used generic API.

### Different Compilers

Replacement by name works well for free functions, but not that well for member
functions, in a sense that you would have to provide properly mangled name for
your specific compiler. This can be improved of course.

## Future Work

After writing this tool and using it for some time, we have identified the
following, as the potential extensions and improvements:

- Ability to invoke the original function
- Sanity checks, for instance, recursive invocations
- Automatically mock the entire overload set
- Virtual functions replacement
- Template functions. All or some instantiations
- Full type/namespace API mocking 
- Concurrent mocks

You are welcome to contribute.

## Requirements

The base requirements to use Mocxx are quite humble:
- A C++17-standard-compliant compiler
- [Frida Gum](https://github.com/frida/frida-gum) (included for MacOS and Linux)
- Catch2 for testing (included)
- CMake for building

## Testing

```sh
git clone git@github.com:Guardsquare/mocxx.git; cd mocxx
mkdir build; cd build
cmake ../ -GNinja -DCMAKE_BUILD_TYPE=Debug
ninja
test/test_mocxx
```

## Installation

### Cmake

Copy `cmake/Mocxx.cmake` into your project `cmake` directory and add the
following in your test `CMakeLists.txt`:
```cmake
// Add ./cmake module folder if not already
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(Mocxx)

target_link_libraries(<test_target> PUBLIC Mocxx)
```

### Other

To install without cmake you need to add `Mocxx` includes and `Frida Gum`
includes and static library located in `vendor/frida/`. Make sure you are
compiling your tests with these flags:

```
-O0 -g -fno-lto -fno-inline-functions -fno-inline
```

And link with these:

```
-lresolv -lpthread -ldl
```

## Contributing
Pull requests are welcome. For major changes, please open an issue first to
discuss what you would like to change.

Please make sure to update tests as appropriate.
