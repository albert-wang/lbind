#pragma once

namespace LBind
{
	struct null_policy_t
	{};

	struct returns_self_t
	{};

	struct ignore_return_t
	{};

	extern null_policy_t null_policy;
	extern returns_self_t returns_self;
	extern ignore_return_t ignore_return;
}