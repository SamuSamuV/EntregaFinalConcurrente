//
//  LuaCoroutine.h
//  LuaState
//
//  See LICENSE and README.md files

#pragma once

namespace lua {

    class State;  // Full definition is available when included from LuaState.hpp

    //////////////////////////////////////////////////////////////////////////////////////////////
    /// Encapsulates a Lua coroutine thread (lua_newthread) and its complete lifecycle.
    /// The thread is anchored in the Lua registry to prevent the GC from destroying it,
    /// following the same pattern as lua::Ref.
    class Coroutine
    {
    public:

        /// State of the coroutine after the last resume() call.
        enum class Status { NotStarted, Suspended, Dead, Error };

    private:

        lua_State * _mainState;     ///< Main Lua state (not owner)
        lua_State * _thread;        ///< Coroutine thread (anchored in the registry)
        int         _refKey;        ///< Registry key that prevents the GC from destroying the thread
        Status      _status;        ///< Current status of the coroutine
        std::string _lastError;     ///< Last error message from lua_resume
        detail::DeallocQueue* _deallocQueue; ///< Deallocation queue from the owning lua::State

    public:

        /// Constructs the coroutine for the indicated global Lua function.
        ///
        /// @param state        The owning lua::State of the interpreter
        /// @param functionName Name of the global Lua function that will be executed as a coroutine
        Coroutine(lua::State& state, const char* functionName)
            : _mainState(state.getState())
            , _thread(nullptr)
            , _refKey(LUA_NOREF)
            , _status(Status::NotStarted)
            , _deallocQueue(state.getDeallocQueue())
        {
            // Validate functionName to avoid undefined behavior in lua_getglobal
            if (!functionName)
            {
                _mainState     = nullptr;
                _deallocQueue  = nullptr;
                _status        = Status::Error;
                _lastError     = "functionName is null";
                return;
            }

            // Creates the thread; lua_newthread pushes it to the stack of the main state
            _thread = lua_newthread(_mainState);

            // Anchors the thread in the registry to prevent the GC from collecting it (removes it from the stack)
            _refKey = luaL_ref(_mainState, LUA_REGISTRYINDEX);

            // Pushes the function to the coroutine thread stack
            int type = lua_getglobal(_mainState, functionName);
            if (type != LUA_TFUNCTION)
            {
                lua_pop(_mainState, 1);
                luaL_unref(_mainState, LUA_REGISTRYINDEX, _refKey);
                _thread    = nullptr;
                _refKey    = LUA_NOREF;
                _status    = Status::Error;
                _lastError = std::string("global '") + functionName + "' is not a function";
                return;
            }
            lua_xmove(_mainState, _thread, 1);
        }

        Coroutine(const Coroutine&)            = delete;
        Coroutine& operator=(const Coroutine&) = delete;

        Coroutine(Coroutine&& other) noexcept
            : _mainState    (other._mainState)
            , _thread       (other._thread)
            , _refKey       (other._refKey)
            , _status       (other._status)
            , _deallocQueue (other._deallocQueue)
        {
            other._mainState    = nullptr;
            other._thread       = nullptr;
            other._refKey       = LUA_NOREF;
            other._status       = Status::Dead;
            other._deallocQueue = nullptr;
        }

        Coroutine& operator=(Coroutine&& other) noexcept
        {
            if (this != &other)
            {
                if (_mainState && _refKey != LUA_NOREF)
                    luaL_unref(_mainState, LUA_REGISTRYINDEX, _refKey);

                _mainState    = other._mainState;
                _thread       = other._thread;
                _refKey       = other._refKey;
                _status       = other._status;
                _deallocQueue = other._deallocQueue;

                other._mainState    = nullptr;
                other._thread       = nullptr;
                other._refKey       = LUA_NOREF;
                other._status       = Status::Dead;
                other._deallocQueue = nullptr;
            }
            return *this;
        }

        ~Coroutine()
        {
            if (_mainState && _refKey != LUA_NOREF)
                luaL_unref(_mainState, LUA_REGISTRYINDEX, _refKey);
        }

        /// Starts or resumes the coroutine, passing zero or more arguments.
        /// The value(s) returned by yield() or return in Lua are moved to the
        /// main state and wrapped in a lua::Value.  Use getStatus() to query
        /// the coroutine state after each resume.
        ///
        /// @param  args  Values passed to the coroutine in this resume
        /// @return       A lua::Value holding the first yielded/returned value (nil if none)
        template<typename ... Args>
        lua::Value resume(Args&&... args)
        {
            if (_status == Status::Dead || _status == Status::Error)
                return lua::Value();

            stack::push(_thread, std::forward<Args>(args)...);

            int nresults = 0;
            int result   = lua_resume(_thread, _mainState, static_cast<int>(sizeof...(Args)), &nresults);

            switch (result)
            {
                case LUA_OK:    _status = Status::Dead;      break;
                case LUA_YIELD: _status = Status::Suspended; break;
                default:
                    _status = Status::Error;
                    nresults = lua_gettop(_thread);
                    if (nresults > 0 && lua_isstring(_thread, -1))
                        _lastError = lua_tostring(_thread, -1);
                    else
                        _lastError = "unknown coroutine error";
                    lua_pop(_thread, nresults);
                    return lua::Value();
            }

            if (nresults > 0)
            {
                int mainTop = stack::top(_mainState);
                lua_xmove(_thread, _mainState, nresults);
                return lua::Value(std::make_shared<detail::StackItem>(
                    _mainState, _deallocQueue, mainTop, nresults, nresults - 1));
            }

            return lua::Value();
        }

        /// Returns the current state of the coroutine.
        Status getStatus()    const { return _status; }

        bool   isNotStarted() const { return _status == Status::NotStarted; }
        bool   isSuspended()  const { return _status == Status::Suspended;  }
        bool   isDead()       const { return _status == Status::Dead;       }
        bool   isError()      const { return _status == Status::Error;      }

        /// Returns the last error message (empty if no error occurred).
        const std::string& getLastError() const { return _lastError; }
    };

}
