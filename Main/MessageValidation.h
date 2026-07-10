#pragma once

#include <boost/json.hpp>

#include <expected>
#include <string>

class MessageValidation
{
public:
	static auto validate_request(const boost::json::object& message) -> std::expected<void, std::string>;
};
