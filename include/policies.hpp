#pragma once

namespace LBind
{
	struct null_policy_t
	{};

	struct returns_self_t
	{};

	extern null_policy_t null_policy;
	extern returns_self_t returns_self;
}