#ifndef _RESULT_H
#define _RESULT_H

#include <cstdio>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

// Everything in this header lies in the result namespace.
namespace result
{

// TRY_RESULT allows you to do this:
// int value;
// TRY(maybe_error(a, b), value);
// If maybe_error(a, b) results in an error,
// the function will early-return with this error.
// Else, value will be set to the OK value of the result.
#define TRY_RESULT(expr, ok) \
    do { \
        auto&& result = (expr); \
        if (result.is_ok()) { \
            ok = result.unwrap(); \
        } else { \
            return err(result.unwrap_err()); \
        } \
    } while (0)

// Sadly, will have to copy-paste this.
#ifndef NO_TRY_ALIAS
#define TRY(expr, ok) \
    do { \
        auto&& result = (expr); \
        if (result.is_ok()) { \
            ok = result.unwrap(); \
        } else { \
            return err(result.unwrap_err()); \
        } \
    } while (0)
#endif

namespace detail
{

    // panic(...) is used whenever an error occurs when unwrapping the result.

#ifndef NO_EXCEPTIONS

    template <typename... Args>
    [[noreturn]] void panic(const char msg[], Args&&... args) noexcept
    {
        using namespace std;
        char test_buf[1];
        size_t required = std::snprintf(test_buf, 0, msg, static_cast<Args>(args)...);
        char* buf = new char[required + 1];
        std::sprintf(buf, msg, static_cast<Args>(args)...);
        std::runtime_error e (buf);
        delete[] buf;
        throw e;
    }

#elif
    
    template <typename... Args>
    [[noreturn]] void panic(const char msg[], Args&&... args) noexcept
    {
        using namespace std;
        fputs("\n", stderr);
        fprintf(stderr, msg, static_cast<Args>(args)...);
        fputs("\n", stderr);
        fflush(stderr);
        terminate();
    }
    
#endif

} // namespace detail

// Just a forward-declaration.
template <typename, typename>
class Result;

// The variant structs used for tagging a
// result without an Ok or Err value.
namespace variants
{

struct NullOk {};
struct NullErr {};

} // namespace variant


namespace detail
{

// These tags are used for disambiguating between
// Result<T, E>'s constructors.
struct OkTag {};
struct ErrTag {};

// The union type used
// for data storage.
template <typename T, typename E>
union OkOrErr
{
    struct {} uninit;
    T ok;
    E err;

    ~OkOrErr()
    {
    }

    OkOrErr()
        : uninit()
    {
    }

    OkOrErr(const T& ok, detail::OkTag)
        : ok(ok)
    {
    }
    
    OkOrErr(const E& err, detail::ErrTag)
        : err(err)
    {
    }

    OkOrErr(T&& ok, detail::OkTag)
        : ok(std::move(ok))
    {
    }
    
    OkOrErr(E&& err, detail::ErrTag)
        : err(std::move(err))
    {
    }

};

// This enumeration defines all 
// the possible states of a Result,
// including moved values.
enum class ResultState : char
{
    Ok = 0x1, 
    Err = 0x2, 
    Moved = 0x4,
    MovedOk = 0x5,
    MovedErr = 0x6,
};

} // namespace detail

// Constructs an Ok Result.
template <typename T>
Result<typename std::remove_reference<T>::type, variants::NullErr> ok(const T& value)
{
    return Result<typename std::remove_reference<T>::type, variants::NullErr>(value, detail::OkTag());
}

// Constructs an Err Result.
template <typename E>
Result<variants::NullOk, typename std::remove_reference<E>::type> err(const E& value)
{
    return Result<variants::NullOk, typename std::remove_reference<E>::type>(value, detail::ErrTag());
}

// Constructs an Ok Result.
template <typename T>
Result<typename std::remove_reference<T>::type, variants::NullErr> ok(T&& value)
{
    return Result<typename std::remove_reference<T>::type, variants::NullErr>(std::move(value), detail::OkTag());
}

// Constructs an Err Result.
template <typename E>
Result<variants::NullOk, typename std::remove_reference<E>::type> err(E&& value)
{
    return Result<variants::NullOk, typename std::remove_reference<E>::type>(std::move(value), detail::ErrTag());
}

template <typename T, typename E>
class Result
{
public:

    // T can never be NullErr.
    static_assert(!std::is_same<T, variants::NullErr>::value, "T cannot be of class result::variants::NullErr.");
    // Nor E can be NullOk
    static_assert(!std::is_same<E, variants::NullOk>::value, "E cannot be of class result::variants::NullOk.");

    // Currently can't store references.
    static_assert(!std::is_reference<T>::value, "T cannot be a reference type.");
    static_assert(!std::is_reference<E>::value, "E cannot be a reference type.");

    // Public typedefs
    typedef T OkType;
    typedef E ErrType;

    // These all use the private tagged constructor
    friend Result<T, variants::NullErr> ok<>(T&);
    friend Result<variants::NullOk, E> err<>(E&);
    friend Result<T, variants::NullErr> ok<>(const T&);
    friend Result<variants::NullOk, E> err<>(const E&);
    friend Result<T, variants::NullErr> ok<>(T&&);
    friend Result<variants::NullOk, E> err<>(E&&);

    template <typename U, typename F>
    friend class Result;

    ~Result()
    {
        if (State::Ok == state_ || State::MovedOk == state_)
        {
            // If the data is a valid or moved ok - destroy it.
            data_.ok.~T();
        }
        else
        {
            // If an error - destroy it too.
            data_.err.~E();
        }
    }

    // Copy-constructor
    Result(const Result<T, E>& result)
        : data_()
        , state_(result.state_)
    {
        if (State::Ok == state_)
        {
            new (&data_.ok) T(result.unwrap());
        }
        else
        {
            new (&data_.err) E(result.unwrap_err());
        }
    }

    // Move constructor.
    Result(Result<T, E>&& result)
        : data_()
        , state_(result.state_)
    {
        if (State::Ok == state_)
        {
            new (&data_.ok) T(std::move(result.unwrap()));
        }
        else 
        {
            new (&data_.err) E(std::move(result.unwrap_err()));
        }
    }

    template <typename U>
    Result(Result<U, variants::NullErr>&& result)
        : data_()
        , state_(result.state_)
    {
        new (&data_) OkOrErr(std::move(result.unwrap()), OkTag());
    }

    template <typename F>
    Result(Result<variants::NullOk, F>&& result)
        : data_()
        , state_(result.state_)
    {
        new (&data_) OkOrErr(std::move(result.unwrap_err()), ErrTag());
    }

    // Is the value Ok?
    bool is_ok() const
    {
        return State::Ok == state_;
    }
    
    // Is the value Err?
    bool is_err() const
    {
        return State::Err == state_;
    }

    operator bool() const 
    {
        return State::Ok == state_;
    }

    // Converts to a result referencing this,
    // without moving the values.
    Result<std::reference_wrapper<T>, std::reference_wrapper<E>> as_ref()
    {
        if (State::Ok == state_) 
        {
            return Result<std::reference_wrapper<T>, std::reference_wrapper<E>>(std::reference_wrapper<T>(data_.ok), OkTag());
        }
        else if (State::Err == state_)
        {
            return Result<std::reference_wrapper<T>, std::reference_wrapper<E>>(std::reference_wrapper<E>(data_.err), ErrTag());
        }
        else
        {
            detail::panic("Panic: Called as_ref() on a moved result!");
        }
    }

    // Converts to a result referencing this,
    // without moving the values.
    Result<std::reference_wrapper<const T>, std::reference_wrapper<const E>> as_ref() const
    {
        if (State::Ok == state_) 
        {
            return Result<std::reference_wrapper<const T>, std::reference_wrapper<const E>>(std::reference_wrapper<const T>(data_.ok), OkTag());
        }
        else if (State::Err == state_)
        {
            return Result<std::reference_wrapper<const T>, std::reference_wrapper<const E>>(std::reference_wrapper<const E>(data_.err), ErrTag());
        }
        else
        {
            detail::panic("Panic: Called as_ref() on a moved result!");
        }
    }

    // *Moves* the value and returns it, if it is Ok,
    // or throws / panics.
    T unwrap()
    {
        if (State::Ok == state_)
        {
            state_ = State::MovedOk;
            return std::move(data_.ok);
        }
        else
        {
            detail::panic("Panic: Called unwrap() on %s result!", 
                State::Err == state_ ? "an erroneous" : "a moved");
        }
    }

    // *Moves* the value and returns it, if it is Ok,
    // or returns optb.
    T unwrap_or(T&& optb)
    {
        if (State::Ok == state_)
        {
            state_ = State::MovedOk;
            return std::move(data_.ok);
        }
        else
        {
            return std::move(optb);
        }
    }

    // *Moves* the value and returns it, if it is Ok,
    // or returns the value of op().
    template <typename F>
    T unwrap_or_else(F&& op)
    {
        if (State::Ok == state_)
        {
            state_ = State::MovedOk;
            return std::move(data_.ok);
        }
        else
        {
            return op();
        }
    }

    // *Moves* the value and returns it, if it is Ok,
    // or returns the default value of T.
    T unwrap_or_default()
    {
        if (State::Ok == state_)
        {
            state_ = State::MovedOk;
            return std::move(data_.ok);
        }
        else
        {
            return T();
        }
    }

    // Unwraps the result if Ok, or panics / throws an
    // error with the contents of msg.
    T expect(const char* const msg)
    {
        if (State::Ok == state_)
        {
            state_ = State::MovedOk;
            return std::move(data_.ok);
        }
        else
        {
            detail::panic(msg);
        }
    }

    // *Moves* the value and returns it, if it is Err,
    // or throws / panics.
    E unwrap_err()
    {
        if (State::Err == state_)
        {
            state_ = State::MovedErr;
            return std::move(data_.err);
        }
        else
        {
            detail::panic("Panic: Called unwrap_err() on %s result!", 
                State::Ok == state_ ? "an OK" : "a moved");
        }
    }

    // If the result is Ok,
    // returns res.
    template <typename U>
    Result<U, E> all(Result<U, E>&& res)
    {
        if (State::Ok == state_)
        {
            return std::move(res);
        }
        else 
        {
            return Result<U, E>(unwrap_err(), ErrTag());
        }
    }

    // If the result is Ok,
    // returns the value of op().
    template <typename F>
    Result<typename std::result_of<F()>::type, E> all_then(F&& op)
    {
        if (State::Ok == state_)
        {
            return op();
        }
        else 
        {
            return Result<typename std::result_of<F()>::type, E>(unwrap_err(), ErrTag());
        }
    }

    template <typename F>
    Result<T, F> any(Result<T, F>&& result)
    {
        if (State::Err == state_)
        {
            return std::move(result);
        }
        else
        {
            return Result<T, F>(unwrap(), OkTag());
        }
    }

    template <typename F>
    Result<T, typename std::result_of<F()>::type> any_else(F&& op)
    {
        if (State::Err == state_)
        {
            return op();
        }
        else
        {
            return Result<T, typename std::result_of<F()>::type>(unwrap(), OkTag());
        }
    }

    // Calls either if_ok or if_err, with the unwrapped value,
    // depending on the state of the result. 
    template <typename IfOk, typename IfErr,
        typename = typename std::is_same<
            typename std::result_of<IfOk(T)>::type,
            typename std::result_of<IfErr(E)>::type
        >::type>
    typename std::result_of<IfOk(T)>::type match(IfOk&& if_ok, IfErr&& if_err)
    {
        if (State::Ok == state_) 
        {
            return if_ok(unwrap());
        }
        else
        {
            return if_err(unwrap_err());
        }
    }

    // maps
    
    template <class F>
    Result<typename std::result_of<F(T)>::type, E> map(const F& project)
    {
        if (State::Ok == state_)
        {
            return Result<typename std::result_of<F(T)>::type, E>(project(unwrap()), OkTag());
        } 
        else 
        {
            return Result<typename std::result_of<F(T)>::type, E>(unwrap_err(), ErrTag());
        }
    }

    template <class F>
    Result<T, typename std::result_of<F(E)>::type> map_err(const F& project)
    {
        if (State::Err == state_)
        {
            return Result<T, typename std::result_of<F(E)>::type>(project(unwrap_err()), ErrTag());
        }
        else
        {
            return Result<T, typename std::result_of<F(E)>::type>(unwrap(), OkTag());
        }
    }

private:

    // Private typedefs
    typedef detail::OkOrErr<T, E> OkOrErr;
    typedef detail::ResultState State;
    typedef detail::OkTag OkTag;
    typedef detail::ErrTag ErrTag;

    // The data.
    OkOrErr data_;
    // And the state of the data.
    State state_;

    // Generally called only by ok(x).
    Result(const T& value, OkTag)
        : data_(value, OkTag())
        , state_(State::Ok)
    {
    }

    // Generally called only by err(x).
    Result(const E& value, ErrTag)
        : data_(value, ErrTag())
        , state_(State::Err)
    {
    }

    Result(T&& value, OkTag)
        : data_(std::move(value), OkTag())
        , state_(State::Ok)
    {
    }

    Result(E&& value, ErrTag)
        : data_(std::move(value), ErrTag())
        , state_(State::Err)
    {
    }

};

} // namespace result

#endif // _RESULT_H