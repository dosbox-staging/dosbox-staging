/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"

#include <utility>

#include "debug_lua.h"

#include "lua.hpp"

namespace {

using LuaStatePtr = std::unique_ptr<lua_State, decltype(&lua_close)>;

// Creates a fresh LuaStatePtr, or aborts if it fails.
LuaStatePtr CreateRawLuaState()
{
	auto lua_state = LuaStatePtr(luaL_newstate(), lua_close);
	if (!lua_state) {
		ABORT_F("Could not allocate memory for lua state");
	}
	return lua_state;
}

const char* LuaStringViewReader(lua_State* lua_state, void* data, size_t* size)
{
	(void)lua_state; // unused
	auto* view = static_cast<std::string_view*>(data);
	if (view->empty()) {
		*size = 0;
		return nullptr;
	} else {
		*size               = view->size();
		auto* returned_text = view->data();
		// Set view to the empty stringview so that we correctly
		// terminate the reader.
		*view = std::string_view();
		return returned_text;
	}
}

std::string_view Lua_PushStringView(lua_State* lua_state, std::string_view view)
{
	auto* new_data = lua_pushlstring(lua_state, view.data(), view.size());
	return std::string_view(new_data, view.size());
}

std::string_view Lua_ToStringView(lua_State* lua_state, int index)
{
    size_t size;
    auto* data = lua_tolstring(lua_state, index, &size);
    return std::string_view(data, size);
}

// Creates a new userdata on the lua stack, and returns a pointer to the newly
// constructed object. Takes arguments similar to std::make_unique<T>, as an
// addition to the lua state and the nuvalue argument.
//
// Returns a pointer to the newly constructed object. This object is owned by
// the Lua interpreter. It will be destroyed when the variable is garbage
// collected, or the state is closed.
template <class T, class... Args>
T* Lua_NewUserData(lua_State* lua_state, int nuvalue, Args&&... args)
{
	// Allocate space for storage of the object
	void* storage = lua_newuserdatauv(lua_state, sizeof(T), nuvalue);
	T* ptr        = new (storage) T(std::forward<Args>(args)...);

	if constexpr (!std::is_trivially_destructible_v<T>) {
		// We need to set the value's metatable so that it can be
		// deallocated.
		lua_createtable(lua_state, 0, 1);
		lua_pushcfunction(lua_state, [](lua_State* lua_state) {
			// Get the pointer to the object from the userdata
			auto* storage = static_cast<T*>(lua_touserdata(lua_state, 1));
			// Call the destructor
			storage->~T();
			return 0;
		});
		lua_setfield(lua_state, -2, "__gc");
		lua_setmetatable(lua_state, -2);
	}

	return ptr;
}

template <class T>
T* Lua_GetUserData(lua_State* lua_state, int index)
{
	return static_cast<T*>(lua_touserdata(lua_state, index));
}

class LuaInterpreterImpl : public LuaInterpreter {
public:
	static constexpr std::string_view kThisRegistryKey = "_dosbox_interpreter";

	explicit LuaInterpreterImpl(LuaDebugInterface* debug_interface,
	                            LuaStatePtr lua_state)
	        : debug_interface_(debug_interface),
	          lua_state_(std::move(lua_state))
	{}

	LuaInterpreterImpl(const LuaInterpreterImpl&)            = delete;
	LuaInterpreterImpl& operator=(const LuaInterpreterImpl&) = delete;

	// Do initial setup of the lua state. This will be called once after
	// construction. Returns false if the lua state could not be initialized.
	bool InitLuaState()
	{
		// Add this interpreter object to the registry, so it can be
		// accessed by the lua state.
		Lua_PushStringView(lua_state_.get(), kThisRegistryKey);
		Lua_NewUserData<LuaInterpreterImpl*>(lua_state_.get(), 0, this);
		lua_settable(lua_state_.get(), LUA_REGISTRYINDEX);

        // Register the interpreter functions
        RegisterInterpreterCall("print", &LuaInterpreterImpl::LM_PrintString);

		return true;
	}

	bool RunCommand(std::string_view command) override
	{
		auto result = lua_load(lua_state_.get(),
		                       LuaStringViewReader,
		                       &command,
		                       "<debugger command>",
		                       "t");
		switch (result) {
		case LUA_OK: break;
		case LUA_ERRSYNTAX:
			debug_interface_->WriteToConsole("Syntax error\n");
			return false;
		case LUA_ERRMEM:
			debug_interface_->WriteToConsole("Memory allocation error\n");
			return false;
		case LUA_ERRRUN:
			debug_interface_->WriteToConsole(
			        "Error during garbage collection\n");
			return false;
		case LUA_ERRERR:
			debug_interface_->WriteToConsole("Recursive error\n");
			return false;
		}

		// The command was loaded, and should be a function on top of
		// the lua stack. Call it.
		lua_pcall(lua_state_.get(), 0, 0, 0);
		return false;
	}

private:
    // A pointer-to-member-function that can be registered and called from Lua.
	using LuaMethod = int (LuaInterpreterImpl::*)(lua_State* lua_state);

	static int InterpreterCall(lua_State* lua_state)
	{
		auto* interpreter = *Lua_GetUserData<LuaInterpreterImpl*>(
		        lua_state, lua_upvalueindex(1));
		auto method = *Lua_GetUserData<LuaMethod>(lua_state,
		                                          lua_upvalueindex(2));
		return (interpreter->*method)(lua_state);
	}

	static void PushInterpreter(lua_State* lua_state)
	{
		Lua_PushStringView(lua_state, kThisRegistryKey);
		lua_gettable(lua_state, LUA_REGISTRYINDEX);
	}

    // Pushes a function onto the lua stack that will call the given method. That method must
    // be of the
	void PushInterpreterCall(LuaMethod lua_method)
	{
		// Upvalues:
		// 1 - The interpreter object
		// 2 - The method to call
		PushInterpreter(lua_state_.get());
		Lua_NewUserData<LuaMethod>(lua_state_.get(), 0, lua_method);
		lua_pushcclosure(lua_state_.get(), &InterpreterCall, 2);
	}

	void RegisterInterpreterCall(const char* global_name, LuaMethod lua_method)
	{
		PushInterpreterCall(lua_method);
		lua_setglobal(lua_state_.get(), global_name);
	}

    // Lua methods
    int LM_PrintString(lua_State* lua_state)
    {
        if (lua_gettop(lua_state) != 1) {
            debug_interface_->WriteToConsole("print() takes exactly one argument\n");
            return 0;
        }

        auto text = Lua_ToStringView(lua_state, 1);
        if (!text.data()) {
            debug_interface_->WriteToConsole("print() argument is not a string\n");
            return 0;
        }

        debug_interface_->WriteToConsole(text);
        return 0;
    }

	LuaDebugInterface* debug_interface_;
	LuaStatePtr lua_state_;
};

} // namespace

std::unique_ptr<LuaInterpreter> LuaInterpreter::Create(LuaDebugInterface* debug_interface)
{
    auto interpreter = std::make_unique<LuaInterpreterImpl>(debug_interface,
                                                            CreateRawLuaState());
    if (!interpreter->InitLuaState()) {
        ABORT_F("Unable to initialize Lua interpreter");
    }

	return interpreter;
}
