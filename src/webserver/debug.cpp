// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "webserver.h"
#include "debugger/debugger_api.h"
#include "private/cpu.h"
#include "private/debug.h"

#include <string>

#include "json/json.h"

using httplib::Request, httplib::Response;
using json = nlohmann::json;

#if C_DEBUGGER

namespace Webserver {

static void send_error(Response& res,
                       const int status,
                       const std::string& error)
{
	res.status = status;
	send_json(res, json{{"error", error}});
}

template <typename T>
struct ParseResult {
	bool ok          = false;
	T value          = {};
	std::string error = {};
};

static DebugResult parse_json_body(const Request& req, json& request)
{
	if (req.body.empty()) {
		request = json::object();
		return {};
	}

	request = json::parse(req.body, nullptr, false);
	if (request.is_discarded() || !request.is_object()) {
		return {false, "request body must be a JSON object"};
	}
	return {};
}

static ParseResult<std::string> parse_string_field(const json& request,
                                                   const char* field_name)
{
	if (!request.contains(field_name)) {
		return {false, {}, std::string(field_name) + " is required"};
	}
	if (!request.at(field_name).is_string()) {
		return {false, {}, std::string(field_name) + " must be a string"};
	}
	return {true, request.at(field_name).get<std::string>(), {}};
}

static std::string debug_pause_mode_to_string(const DebugPauseMode pause_mode)
{
	switch (pause_mode) {
	case DebugPauseMode::Interactive: return "interactive";
	case DebugPauseMode::Headless: return "headless";
	}
	return "interactive";
}

static std::string debug_stop_reason_to_string(const DebugStopReason reason)
{
	switch (reason) {
	case DebugStopReason::None: return "none";
	case DebugStopReason::StepComplete: return "stepComplete";
	case DebugStopReason::PauseRequested: return "pauseRequested";
	}
	return "none";
}

static std::string debug_step_mode_to_string(const DebugStepMode step_mode)
{
	switch (step_mode) {
	case DebugStepMode::Into: return "into";
	}
	return "into";
}

static ParseResult<DebugPauseMode> parse_pause_mode(const json& request)
{
	if (!request.contains("mode")) {
		return {true, DebugPauseMode::Headless, {}};
	}

	const auto mode = parse_string_field(request, "mode");
	if (!mode.ok) {
		return {false, {}, mode.error};
	}

	if (mode.value == "interactive") {
		return {true, DebugPauseMode::Interactive, {}};
	}
	if (mode.value == "headless") {
		return {true, DebugPauseMode::Headless, {}};
	}
	return {false, {}, "invalid pause mode: " + mode.value};
}

static ParseResult<DebugStepMode> parse_step_mode(const json& request)
{
	if (!request.contains("mode")) {
		return {true, DebugStepMode::Into, {}};
	}

	const auto mode = parse_string_field(request, "mode");
	if (!mode.ok) {
		return {false, {}, mode.error};
	}

	if (mode.value == "into") {
		return {true, DebugStepMode::Into, {}};
	}
	return {false, {}, "invalid step mode: " + mode.value};
}

static json stop_state_to_json(const DebugStopState& stop_state)
{
	return {
	        {"running", stop_state.running},
	        {"paused", stop_state.paused},
	        {"pauseMode", debug_pause_mode_to_string(stop_state.pause_mode)},
	        {"stopSequence", stop_state.stop_sequence},
	        {"reason", debug_stop_reason_to_string(stop_state.reason)},
	        {"description", stop_state.description},
	        {"registers", cpu_registers_to_json(stop_state.registers)},
	};
}

void DebugStatusCommand::Execute()
{
	stop_state = DEBUG_GetStopState();
}

void DebugStatusCommand::Get(const Request&, Response& res)
{
	DebugStatusCommand command = {};
	command.WaitForCompletion();
	send_json(res, stop_state_to_json(command.stop_state));
}

void DebugPauseCommand::Execute()
{
	result = DEBUG_RequestPause(pause_mode);
}

void DebugPauseCommand::Post(const Request& req, Response& res)
{
	json request = {};
	auto result  = parse_json_body(req, request);
	if (!result.ok) {
		send_error(res, httplib::StatusCode::BadRequest_400, result.error);
		return;
	}

	const auto pause_mode = parse_pause_mode(request);
	if (!pause_mode.ok) {
		send_error(res,
		           httplib::StatusCode::BadRequest_400,
		           pause_mode.error);
		return;
	}

	DebugPauseCommand command(pause_mode.value);
	command.WaitForCompletion();

	if (!command.result.ok) {
		send_error(res,
		           httplib::StatusCode::Conflict_409,
		           command.result.error);
		return;
	}
	DebugStatusCommand status_command = {};
	status_command.WaitForCompletion();
	send_json(res, stop_state_to_json(status_command.GetStopState()));
}

void DebugContinueCommand::Execute()
{
	result = DEBUG_RequestContinue();
}

void DebugContinueCommand::Post(const Request&, Response& res)
{
	DebugContinueCommand command = {};
	command.WaitForCompletion();

	if (!command.result.ok) {
		send_error(res,
		           httplib::StatusCode::Conflict_409,
		           command.result.error);
		return;
	}
	send_json(res, json{{"accepted", true}});
}

void DebugStepCommand::Execute()
{
	result = DEBUG_RequestStep(step_mode);
}

void DebugStepCommand::Post(const Request& req, Response& res)
{
	json request = {};
	auto result  = parse_json_body(req, request);
	if (!result.ok) {
		send_error(res, httplib::StatusCode::BadRequest_400, result.error);
		return;
	}

	const auto step_mode = parse_step_mode(request);
	if (!step_mode.ok) {
		send_error(res, httplib::StatusCode::BadRequest_400, step_mode.error);
		return;
	}

	DebugStepCommand command(step_mode.value);
	command.WaitForCompletion();

	if (!command.result.ok) {
		send_error(res,
		           httplib::StatusCode::Conflict_409,
		           command.result.error);
		return;
	}
	send_json(res,
	          json{{"accepted", true},
	               {"mode", debug_step_mode_to_string(command.step_mode)}});
}

} // namespace Webserver

#endif
