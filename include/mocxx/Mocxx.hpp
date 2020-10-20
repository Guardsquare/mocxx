#pragma once

// frida
#include <frida/frida-gum.h>

// stl
#include <experimental/type_traits>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

namespace mocxx {
namespace details {

template<typename... Ts>
struct List
{};

using EmptyList = List<>;

template<typename T, typename... Ts>
constexpr auto
FirstImplMethod(List<T, Ts...>) -> T;

template<typename List>
using First = decltype(FirstImplMethod<>(List{}));

template<typename T, typename... Ts>
constexpr auto
TailImplMethod(List<T, Ts...>) -> List<Ts...>;

template<typename List>
using Tail = decltype(TailImplMethod<>(List{}));

// Convert a function type to a list of types
// \{
// Free Variadic Function
template<typename ResultType, typename... Args>
constexpr auto InvocableToTypeListImpl(ResultType(Args..., ...))
  -> List<ResultType, Args...>;

// Free Function or Member Static Funciton
template<typename ResultType, typename... Args>
constexpr auto InvocableToTypeListImpl(ResultType(Args...))
  -> List<ResultType, Args...>;

// Member Const Function
template<typename ClassType, typename ResultType, typename... Args>
constexpr auto
InvocableToTypeListImpl(ResultType (ClassType::*)(Args...) const)
  -> List<ResultType, const ClassType*, Args...>;

// Member Function
template<typename ClassType, typename ResultType, typename... Args>
constexpr auto InvocableToTypeListImpl(ResultType (ClassType::*)(Args...))
  -> List<ResultType, ClassType*, Args...>;

template<typename T>
using TypeCallOperator = decltype(&T::operator());

template<typename FunctionType>
using InvocableToTypeList = decltype(InvocableToTypeListImpl(
  std::conditional_t<
    /*   if */ std::is_member_function_pointer_v<FunctionType>,
    /* then */ FunctionType,
    /* else */
    std::experimental::detected_or_t<
      /*   default */ FunctionType,
      /*  operator */ TypeCallOperator,
      /* arguments */ FunctionType>>(nullptr)));
// \}

// Member Const Function
template<typename ClassType, typename ResultType, typename... Args>
constexpr auto
InvocableToTypeListImpl(ResultType (ClassType::*)(Args...) const)
  -> List<ResultType, const ClassType*, Args...>;

// Member Function
template<typename ClassType, typename ResultType, typename... Args>
constexpr auto InvocableToTypeListImpl(ResultType (ClassType::*)(Args...))
  -> List<ResultType, ClassType*, Args...>;

template<typename Lambda>
using LambdaResult = First<InvocableToTypeList<Lambda>>;

template<typename Lambda>
using LambdaParameters = Tail<Tail<InvocableToTypeList<Lambda>>>;

// Convert lambda callable type to its corresponding type member function
// counterpart. As there is no way of telling if the provided lambda can be
// used as a substitution for some type member function, this method should be
// considered speculative, where the first lambda argument type is assumed to
// be the target type.
// \{
//
// Immutable lambda
template<typename LambdaType,
         typename ClassType,
         typename ResultType,
         typename... Args>
constexpr auto
LambdaToMemberFunctionImpl(ResultType (LambdaType::*)(ClassType*, Args...)
                             const)
  -> std::conditional_t<
    /*   if */ std::is_const_v<std::remove_pointer_t<ClassType>>,
    /* then */ ResultType (std::remove_cv_t<ClassType>::*)(Args...) const,
    /* else */ ResultType (std::remove_cv_t<ClassType>::*)(Args...)>;

// Mutable lambda
template<typename LambdaType,
         typename ClassType,
         typename ResultType,
         typename... Args>
constexpr auto LambdaToMemberFunctionImpl(ResultType (LambdaType::*)(ClassType*,
                                                                     Args...))
  -> std::conditional_t<
    /*   if */ std::is_const_v<std::remove_pointer_t<ClassType>>,
    /* then */ ResultType (std::remove_cv_t<ClassType>::*)(Args...) const,
    /* else */ ResultType (std::remove_cv_t<ClassType>::*)(Args...)>;

template<typename Other>
constexpr auto LambdaToMemberFunctionImpl(Other) -> std::void_t<Other>;

template<typename Lambda>
using LambdaToMemberFunction =
  decltype(LambdaToMemberFunctionImpl(&Lambda::operator()));
// \}

// Convert lambda callable type to its corresponding free function type
// counterpart.
// \{
template<typename LambdaType, typename ResultType, typename... Args>
constexpr auto
LambdaToFreeFunctionImpl(ResultType (LambdaType::*)(Args...) const)
  -> ResultType (*)(Args...);

template<typename LambdaType, typename ResultType, typename... Args>
constexpr auto LambdaToFreeFunctionImpl(ResultType (LambdaType::*)(Args...))
  -> ResultType (*)(Args...);

template<typename LambdaType, typename ResultType, typename... Args>
constexpr auto
LambdaToFreeFunctionImpl(ResultType (LambdaType::*)(Args..., ...) const)
  -> ResultType (*)(Args..., ...);

template<typename Lambda>
using LambdaToFreeFunction =
  decltype(LambdaToFreeFunctionImpl(&Lambda::operator()));
// \}

// Convert a list of types to std::function
// /{
template<typename ResultType, typename... Args>
constexpr auto
ToStdFnImpl(List<ResultType, Args...>) -> std::function<ResultType(Args...)>;

template<typename List>
using ToStdFn = decltype(ToStdFnImpl(List{}));
// /}

// Convert a list of types to a corresponding free function type.
// \{
template<typename ResultType, typename... Args>
constexpr auto
ToFreeFnImpl(List<ResultType, Args...>) -> ResultType (*)(Args...);

template<typename List>
using ToFreeFn = decltype(ToFreeFnImpl(List{}));
// \}

/// Given a target this function tries to resolve it to a function symbol
/// within current binary.
///
/// \returns Pointer to a function symbol identified by \p target.
template<typename TargetType>
void*
TargetToVoidPtr(TargetType target)
{
  if constexpr (std::is_convertible_v<TargetType, std::string>) {
    return GSIZE_TO_POINTER(
      gum_module_find_export_by_name(nullptr, std::string(target).c_str()));

  } else {
    if constexpr (std::is_member_function_pointer_v<TargetType>) {
      // NOTE
      // Member function casts are technically unsafe, the structure of
      // member function pointer is unspecified and may not be an actual
      // address, for example virtual member function pointer is often
      // represented as and offset in vtable.  But since most compilers are
      // sane, non-virtual member function pointers are regular pointers.
      union
      {
        TargetType pf;
        void* p;
      };

      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
      pf = target;
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
      return p;

    } else {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      return reinterpret_cast<void*>(target);
    }
  }
}

// Required for type erasure
struct ReplacementProxyBase
{
  ReplacementProxyBase() = default;
  ReplacementProxyBase(const ReplacementProxyBase&) noexcept = default;
  ReplacementProxyBase(ReplacementProxyBase&&) noexcept = default;
  ReplacementProxyBase& operator=(const ReplacementProxyBase&) noexcept =
    default;
  ReplacementProxyBase& operator=(ReplacementProxyBase&&) noexcept = default;
  virtual ~ReplacementProxyBase() = default;
};

// Replacement proxy is parameterized by replacement signature. Its ::Invoke
// method is used to statically bind to frida interceptor. Since there can be
// many functions with the same signature this type resolves this collision via
// a static map from target to replacement; at interceptor invocation time
// ::Invoke queries frida for current invocation context that points to current
// function and the replacement can be resolved from there.
template<typename ResultType, typename... Args>
class ReplacementProxy : public ReplacementProxyBase
{
public:
  ReplacementProxy() = delete;
  ReplacementProxy(const ReplacementProxy&) = delete;
  ReplacementProxy(ReplacementProxy&&) noexcept = default;
  ReplacementProxy& operator=(const ReplacementProxy&) = delete;
  ReplacementProxy& operator=(ReplacementProxy&&) noexcept = default;

  ReplacementProxy(const void* target,
                   std::function<ResultType(Args...)> replacement)
    : mTarget(target)
  {
    ReplacementProxy<ResultType, Args...>::Replacements.emplace(
      target, std::move(replacement));
  }

  ~ReplacementProxy() override
  {
    ReplacementProxy<ResultType, Args...>::Replacements.erase(mTarget);
  }

  /// The actual function that substitutes a target. It dispatches correct
  /// handler based of currently invoked function.
  static ResultType Invoke(Args... args)
  {
    auto* context = gum_interceptor_get_current_invocation();
    const auto* target =
      gum_invocation_context_get_replacement_function_data(context);
    return Replacements.at(target)(std::forward<Args>(args)...);
  }

private:
  inline static std::unordered_map<const void*,
                                   std::function<ResultType(Args...)>>
    Replacements;

  const void* mTarget;
};

// Since this lambda survives beyond many API invocations here, we need to
// properly forward the result value using std::tuple. This is necessary
// because we don't know upfront what storage the value would require (perfect
// forwarding), and lambda declaration semantics requires explicit statement
// about it, either store by copy or by reference. The std::shared_pointer is
// used to create storage for potential moved-in values, this is necessary
// because lambdas are wrapped in std::function in ReplacementProxy, and
// std::function copies functions in, including lambdas.
template<typename Value>
auto
Capture(Value&& value)
{
  return std::make_shared<std::tuple<Value>>(
    std::tuple<Value>(std::forward<Value>(value)));
}

} // namespace details

/// Frida-based mocking framework, allows to replace function implementation
/// with a custom one at runtime, e.g:
///
/// \code{.cpp}
///  Mocxx mocxx;
///
///  mocxx.ReplaceOnce([&](const std::filesystem::path& p) { return true; },
///                    std::filesystem::exists);
///
///  std::string file = "/this/file/now/exists";
///
///  // Returns true
///  std::filesystem::exists(file);
///
///  // Returns false
///  std::filesystem::exists(file);
///
/// \endcode
class Mocxx
{
public:
  Mocxx()
  {
    // Gum must be initialized once per application because invoking
    // \a gum_deinit_embedded fails for some reason.
    if (!GumInitialised) {
      gum_init_embedded();
    }

    mInterceptor = gum_interceptor_obtain();
  }

  Mocxx(const Mocxx&) = delete;

  Mocxx& operator=(const Mocxx&) = delete;

  Mocxx(Mocxx&& other) noexcept = default;

  Mocxx& operator=(Mocxx&& other) noexcept
  {
    this->mReplacements = std::move(other.mReplacements);
    this->mInterceptor = other.mInterceptor;
    other.mInterceptor = nullptr; // Important for destruction
    return *this;
  }

  ~Mocxx()
  {
    if (mInterceptor == nullptr) {
      return;
    }

    g_object_unref(mInterceptor);

    while (!mReplacements.empty()) {
      Restore(mReplacements.begin()->first);
    }
  }

  /// \returns \a true if \p target is replaced, \a false otherwise.
  template<typename TargetType>
  bool IsReplaced(TargetType target)
  {
    return mReplacements.find(details::TargetToVoidPtr(target)) !=
           mReplacements.end();
  }

  /// Restore a previously replaced \p target.
  ///
  /// \param target A function or name of a function to restore.
  ///
  /// \returns \a true if \p target was replaced and its replacement was
  /// removed, \a false otherwise.
  template<typename TargetType>
  bool Restore(TargetType target)
  {
    if constexpr (std::is_convertible_v<TargetType, std::string>) {
      void* symbol = details::TargetToVoidPtr(target);
      if (symbol == nullptr) {
        return false;
      }

      return Restore(symbol);

    } else {
      void* targetPtr = details::TargetToVoidPtr(target);

      const auto repl = mReplacements.find(targetPtr);
      if (repl == mReplacements.end()) {
        return true;
      }

      mReplacements.erase(repl);

      gum_interceptor_begin_transaction(mInterceptor);
      gum_interceptor_revert_function(
        /*   self */ mInterceptor,
        /* target */ targetPtr);
      gum_interceptor_end_transaction(mInterceptor);

      return true;
    }
  }

  /// Replace a \p target definition with \p replacement definition. Every
  /// successive invocation will substitute the previous replacement. The type
  /// of replacement must match the target type with except of c variadic
  /// arguments which are omitted.
  ///
  /// \param replacement A function like object to replace \p target.
  /// \param target Function to replace.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  template<bool isMember = false, typename Replacement>
  bool Replace(
    Replacement&& replacement,
    std::conditional_t<isMember,
                       details::LambdaToMemberFunction<Replacement>,
                       details::LambdaToFreeFunction<Replacement>> target)
  {
    static_assert(!std::is_same_v<decltype(target), void>,
                  "Could not resolve the target type");

    if (IsReplaced(target)) {
      Restore(target);
    }

    // Split target type and pointer
    using TargetFunctionType =
      details::ToStdFn<details::InvocableToTypeList<decltype(target)>>;
    void* targetPtr = details::TargetToVoidPtr(target);

    using ProxyType = decltype(details::ReplacementProxy(
      targetPtr, TargetFunctionType(std::forward<Replacement>(replacement))));

    mReplacements.insert_or_assign(
      targetPtr,
      std::make_unique<ProxyType>(targetPtr,
                                  std::forward<Replacement>(replacement)));

    gum_interceptor_begin_transaction(mInterceptor);
    gum_interceptor_replace_function(
      /*        self */ mInterceptor,
      /*      target */ targetPtr,
      /* replacement */
      details::TargetToVoidPtr(&ProxyType::Invoke),
      /*        data */ targetPtr);
    gum_interceptor_end_transaction(mInterceptor);

    return true;
  }

  /// Replace a target named as \p name with \p replacement. Every successive
  /// invocation will substitute the previous replacement. The type
  /// of replacement must match the target type with except of c variadic
  /// arguments which are omitted. Since there is no way to determined the type
  /// from function name the type is assumed to be the one from replacement.
  ///
  /// \param replacement A function like object to replace \p target. Its type
  /// determines the type of \p target
  /// \param target Name of a function to replace.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  template<bool isMember = false, typename Replacement>
  bool Replace(Replacement&& replacement, const std::string& target)
  {
    void* symbol = details::TargetToVoidPtr(target);
    if (symbol == nullptr) {
      return false;
    }

    return Replace<isMember>(
      std::forward<Replacement>(replacement),
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      reinterpret_cast<details::LambdaToFreeFunction<Replacement>>(symbol)

    );
  }

  /// Replace a \p target member definition with \p replacement definition.
  /// Every successive invocation will substitute the previous replacement. The
  /// type of replacement must match the target type with except of c variadic
  /// arguments which are omitted. The first replacement parameter must be of
  /// pointer type to the class of the definition to replace. Const/non-const
  /// overload is controlled via constness of this first parameter.
  ///
  /// \param target Function to replace.
  /// \param replacement A function like object to replace \p target.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  template<typename Replacement>
  bool ReplaceMember(Replacement&& replacement,
                     details::LambdaToMemberFunction<Replacement> target)
  {
    return Replace<true>(std::forward<Replacement>(replacement), target);
  }

  /// Replace a \p target free function result with \p value.
  ///
  /// \param value A value to be returned on every \p target invocation.
  /// \param target A target to replace.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  template<typename ResultValue, typename TargetResult, typename... TargetArgs>
  bool Result(ResultValue&& value, TargetResult (*target)(TargetArgs...))
  {
    static_assert(
      std::is_reference_v<ResultValue> ||
        std::is_copy_constructible_v<std::remove_cv<ResultValue>>,
      "Result type must be a reference type or have a copy constructor.");

    static_assert(
      std::is_convertible_v<ResultValue, TargetResult>,
      "Target result value must be convertible to the target's result type.");

    return Replace(
      [capture = details::Capture<ResultValue>(std::forward<ResultValue>(
         value))](TargetArgs... /* unused */) -> TargetResult {
        return std::forward<ResultValue>(
          std::get<ResultValue>(std::move(*capture)));
      },
      target);
  }

  /// Replace a \p target member function result with \p value just once.
  /// Second and successive invocation will return non-mocked result.
  ///
  /// \param value A value to be returned on every \p target invocation.
  /// \param target A target to replace.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  template<typename ResultValue, typename TargetResult, typename... TargetArgs>
  bool ResultOnce(ResultValue&& value, TargetResult (*target)(TargetArgs...))
  {
    static_assert(
      std::is_convertible_v<ResultValue, TargetResult>,
      "Target result value must be convertible to the target's result type.");

    return Replace(
      [this,
       target,
       capture = details::Capture(std::forward<ResultValue>(value))](
        TargetArgs... /* unused */) -> TargetResult {
        // The following order of moving, restoring and returning is necessary
        // to preserve the returned value until after this lambda object is
        // destroyed by the restore.
        auto tuple = std::move(*capture);
        Restore(target);
        return std::forward<ResultValue>(std::get<ResultValue>(tuple));
      },
      target);
  }

  /// Replace a \p target member function result with \p value.
  ///
  /// \param value A value to be returned on every \p target invocation.
  /// \param target A target to replace.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  /// \{
  template<typename ResultValue,
           typename TargetResult,
           typename TargetClass,
           typename... TargetArgs>
  bool ResultMember(ResultValue&& value,
                    TargetResult (TargetClass::*target)(TargetArgs...))
  {
    static_assert(
      std::is_reference_v<ResultValue> ||
        std::is_copy_constructible_v<std::remove_cv<ResultValue>>,
      "Result type must be a reference type or have a copy constructor.");

    static_assert(
      std::is_convertible_v<ResultValue, TargetResult>,
      "Target result value must be convertible to the target's result type.");

    // Since this lambda survives beyond Result invocation we need to properly
    // forward the result value using std::tuple. Secondly, the lambda does not
    // care about the target arguments, so we leave those anonymous as well.
    return ReplaceMember(
      [capture = details::Capture(std::forward<ResultValue>(value))](
        TargetClass* /* unused */, TargetArgs... /* unused */) -> TargetResult {
        return std::get<ResultValue>(*capture);
      },
      target);
  }

  template<typename ResultValue,
           typename TargetResult,
           typename TargetClass,
           typename... TargetArgs>
  bool ResultMember(ResultValue&& value,
                    TargetResult (TargetClass::*target)(TargetArgs...) const)
  {
    static_assert(
      std::is_reference_v<ResultValue> ||
        std::is_copy_constructible_v<std::remove_cv<ResultValue>>,
      "Result type must be a reference type or have a copy constructor.");

    static_assert(
      std::is_convertible_v<ResultValue, TargetResult>,
      "Target result value must be convertible to the target's result type.");

    return ReplaceMember(
      [capture = details::Capture(std::forward<ResultValue>(value))](
        const TargetClass* /* unused */, TargetArgs... /* unused */)
        -> TargetResult { return std::get<ResultValue>(*capture); },
      target);
  }
  // \}

  /// Replace a \p target free function result with a value generated by \p
  /// generator.
  ///
  /// \param generator A lambda providing the value to return. Must not have
  /// any arguments on its own.
  /// \param target A target to replace.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  template<typename ResultGenerator,
           typename TargetResult,
           typename... TargetArgs>
  bool ResultGenerator(ResultGenerator&& generator,
                       TargetResult (*target)(TargetArgs...))
  {
    static_assert(std::is_same_v<details::LambdaParameters<ResultGenerator>,
                                 details::EmptyList>,
                  "Result generator lambda must not accept any arguments");

    using ResultValue = details::LambdaResult<ResultGenerator>;

    static_assert(
      std::is_convertible_v<ResultValue, TargetResult>,
      "Target result value must be convertible to the target's result type.");

    return Replace(
      [capture = details::Capture(std::forward<ResultGenerator>(generator))](
        TargetArgs... /* unused */) -> TargetResult {
        return std::get<ResultGenerator>(*capture)();
      },
      target);
  }

  /// Replace a \p target free function result with \p value of type
  /// ResultConstructor.
  ///
  /// \tparam ResultConstructor A constructor accepting to arguments. The type
  /// must be convertible to \p TargetResult.
  ///
  /// \param target A target to replace.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  template<typename ResultConstructor,
           typename TargetResult,
           typename... TargetArgs>
  bool ResultConstructor(TargetResult (*target)(TargetArgs...))
  {
    static_assert(
      std::is_convertible_v<ResultConstructor, TargetResult>,
      "Target result value must be convertible to the target's result type.");

    return Replace(
      [](TargetArgs... /* unused */) -> TargetResult {
        return ResultConstructor();
      },
      target);
  }

  /// Replace a \p target member function result with a value generated by \p
  /// generator.
  ///
  /// \param generator A lambda providing the value to return. Must not have
  /// any arguments on its own.
  /// \param target A target to replace.
  ///
  /// \returns \a true if replacement was successful, \a false otherwise.
  /// \{
  template<typename ResultGenerator,
           typename TargetClass,
           typename TargetResult,
           typename... TargetArgs>
  bool ResultGeneratorMember(ResultGenerator&& generator,
                             TargetResult (TargetClass::*target)(TargetArgs...))
  {
    using ResultValue = details::LambdaResult<ResultGenerator>;

    static_assert(
      std::is_convertible_v<ResultValue, TargetResult>,
      "Target result value must be convertible to the target's result type.");

    return ReplaceMember(
      [capture = details::Capture(std::forward<ResultGenerator>(generator))](
        TargetClass* /* unused */, TargetArgs... /* unused */) -> TargetResult {
        return std::get<ResultGenerator>(*capture)();
      },
      target);
  }

  template<typename ResultGenerator,
           typename TargetClass,
           typename TargetResult,
           typename... TargetArgs>
  bool ResultGeneratorMember(ResultGenerator&& generator,
                             TargetResult (TargetClass::*target)(TargetArgs...)
                               const)
  {
    using ResultValue = details::LambdaResult<ResultGenerator>;

    static_assert(
      std::is_convertible_v<ResultValue, TargetResult>,
      "Target result value must be convertible to the target's result type.");

    return ReplaceMember(
      [capture = details::Capture(std::forward<ResultGenerator>(generator))](
        const TargetClass* /* unused */, TargetArgs... /* unused */)
        -> TargetResult { return std::get<ResultGenerator>(*capture)(); },
      target);
  }
  /// \}

public:
  inline static bool GumInitialised = false;

  GumInterceptor* mInterceptor = nullptr;
  std::unordered_map<void*, std::unique_ptr<details::ReplacementProxyBase>>
    mReplacements;
};

} // namespace mocxx
