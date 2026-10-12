#include <iostream>
#include <algorithm> 
#include <ranges>
#include "mes.hpp"

namespace mes
{
	const script_info script_info::infos[]
	{  /*  name         offtype  version   uint8x2         uint8str        string          encstr          uint16x4      enckey  output strings... */
		{ "dc3dd"     , offset2, 0xA5A8, { 0x00, 0x38 }, { 0x39, 0x43 }, { 0x44, 0x62 }, { 0x63, 0x67 }, { 0x68, 0xFF },  0x20, { 0x58 }/*---------*/}, // D.C.Ⅲ DreamDays～ダ・カーポⅢ～ドリームデイズ
	};

	[[maybe_unused]] static struct __script_infos_auto_sort__
	{
		__script_infos_auto_sort__(void) noexcept
		{
			const auto begin   { const_cast<script_info*>(script_info::infos)     };
			auto constexpr size{ sizeof(script_info::infos) / sizeof(script_info) };
			std::ranges::sort(begin, begin + size, std::greater{}, &script_info::version);
		}
	} __unused__;

	auto script_info::section::its(const uint8_t key) const noexcept -> bool
	{
		return !(beg == end && beg == 0xFF) && (key >= beg && key <= end);
	}

	auto script_info::operator=(const mes::script_info& other) -> script_info&
	{
		if (this != reinterpret_cast<const script_info*>(&other))
		{
			std::memcpy(this, &other, sizeof(script_info));
		}
		return *this;
	}

	auto script_info::query(const std::span<uint8_t> data) -> const script_info* const
	{
		if (!data.data() || data.empty())
		{
			return nullptr;
		}

		const size_t   size{ data.size() };
		const int32_t* head{ reinterpret_cast<int32_t*>(data.data()) };

		uint16_t version1{}, version2{};
		if (size > 4)
		{
			const int32_t offset1{ head[0] * 0x04 + 0x04 };
			const int32_t offset2{ head[0] * 0x06 + 0x04 };

			if (size > offset1 + 0x02)
			{
				version1 = *reinterpret_cast<uint16_t*>(data.data() + offset1);
			}
			if (size > offset2 + 0x02)
			{
				version2 = *reinterpret_cast<uint16_t*>(data.data() + offset2);
			}
		}

		if (version1 == 0x00 && version2 == 0x00)
		{
			return nullptr;
		}

		for (const auto& info : script_info::infos)
		{
			switch (info.offset)
			{
			case script_info::offset1:
			{
				if (version1 == 0x00)
				{
					continue;
				}
				if ((info.version & 0xFF00) == 0x00)
				{
					if ((version1 & 0xFF) == info.version)
					{
						return &info;
					}
				}
				else if (info.version == version1)
				{
					return &info;
				}
				continue;
			}
			case script_info::offset2:
			{
				if (version2 == 0x00)
				{
					continue;
				}
				if (head[1] == 0x03 && info.version == version2)
				{
					return &info;
				}
				continue;
			}
			};
		}

		return nullptr;
	}

	auto script_info::query(const std::string_view name) -> const script_info* const
	{
		if (!name.empty())
		{
			for (const auto& info : script_info::infos)
			{
				if (name == info.name) return &info;
			}
		}
		return nullptr;
	}

	auto script_info::query(const uint16_t version) -> const script_info* const
	{
		for (const auto& info : script_info::infos)
		{
			if (version == info.version) return &info;
		}
		return nullptr;
	}
}
