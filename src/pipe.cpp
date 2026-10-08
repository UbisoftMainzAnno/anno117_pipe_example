#include "pipe.h"

#include <Windows.h>

#include <bit>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <system_error>

namespace anno_pipe
{

static_assert(std::endian::native == std::endian::little);

std::int32_t ReadInt32(std::size_t& pos, const std::span<const unsigned char> data)
{
	if (pos + 4 > data.size())
	{
		return 0;
	}

	std::uint32_t value = data[pos++];
	value |= (static_cast<std::uint32_t>(data[pos++]) << CHAR_BIT);
	value |= (static_cast<std::uint32_t>(data[pos++]) << (2 * CHAR_BIT));
	value |= (static_cast<std::uint32_t>(data[pos++]) << (3 * CHAR_BIT));
	return std::bit_cast<std::int32_t>(value);
}

std::int64_t ReadInt64(std::size_t& pos, const std::span<const unsigned char> data)
{
	if (pos + 8 > data.size())
	{
		return 0;
	}

	std::uint64_t value = data[pos++];
	value |= (static_cast<std::uint64_t>(data[pos++]) << CHAR_BIT);
	value |= (static_cast<std::uint64_t>(data[pos++]) << (2 * CHAR_BIT));
	value |= (static_cast<std::uint64_t>(data[pos++]) << (3 * CHAR_BIT));
	value |= (static_cast<std::uint64_t>(data[pos++]) << (4 * CHAR_BIT));
	value |= (static_cast<std::uint64_t>(data[pos++]) << (5 * CHAR_BIT));
	value |= (static_cast<std::uint64_t>(data[pos++]) << (6 * CHAR_BIT));
	value |= (static_cast<std::uint64_t>(data[pos++]) << (7 * CHAR_BIT));
	return std::bit_cast<std::int64_t>(value);
}

float ReadFloat(std::size_t& pos, const std::span<const unsigned char> data)
{
	if (pos + 4 > data.size())
	{
		return 0.f;
	}

	std::uint32_t value = data[pos++];
	value |= (static_cast<std::uint32_t>(data[pos++]) << CHAR_BIT);
	value |= (static_cast<std::uint32_t>(data[pos++]) << (2 * CHAR_BIT));
	value |= (static_cast<std::uint32_t>(data[pos++]) << (3 * CHAR_BIT));
	return std::bit_cast<float>(value);
}

std::string_view ReadString(std::size_t& pos, const std::span<const unsigned char> data)
{
	if (pos + 1 > data.size())
	{
		return {};
	}

	const std::size_t length = data[pos++];
	if (pos + length > data.size())
	{
		return {};
	}

	std::string_view s(reinterpret_cast<const char*>(data.data()) + pos, length);
	pos += length;
	return s;
}

//////////////////////////////////////////////////////////////////////////

void ProductionEntryData::read(std::size_t& pos, const std::span<const unsigned char> data)
{
	ProductGuid = ReadInt32(pos, data);

	ProductGeneration = ReadFloat(pos, data);
	ProductConsumption = ReadFloat(pos, data);
	ProductDelta = ReadFloat(pos, data);
	PerfectProductGeneration = ReadFloat(pos, data);
	PerfectProductConsumption = ReadFloat(pos, data);
	AmountOfBuildings = ReadInt32(pos, data);
	TotalMaintenance = ReadInt32(pos, data);
	TotalIncome = ReadFloat(pos, data);
	TotalProfit = ReadInt32(pos, data);
	SummedProductivity = ReadFloat(pos, data);
	AverageProductivity = ReadFloat(pos, data);

	{
		WorkforceGUIDtoAmount.clear();
		auto numWorkforces = ReadInt32(pos, data);
		while (numWorkforces --> 0)
		{
			const auto workforceGUID = ReadInt32(pos, data);
			const auto amount = ReadInt32(pos, data);
			WorkforceGUIDtoAmount[workforceGUID] += amount;
		}
	}
	{
		BuildingGUIDtoAmount.clear();
		auto numBuildings = ReadInt32(pos, data);
		while (numBuildings --> 0)
		{
			const auto buildingGUID = ReadInt32(pos, data);
			const auto amount = ReadInt32(pos, data);
			BuildingGUIDtoAmount[buildingGUID] += amount;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void TradeRouteEntryData::read(std::size_t& pos, const std::span<const unsigned char> data)
{
	Name = ReadString(pos, data);
	Folder = ReadString(pos, data);

	Errors = ReadInt32(pos, data);
	Warnings = ReadInt32(pos, data);

	Ships = ReadInt32(pos, data);
	LostShips = ReadInt32(pos, data);

	const auto numStations = ReadInt32(pos, data);
	Stations.resize(numStations);
	for (auto& station : Stations)
	{
		station.read(pos, data);
	}
}

//////////////////////////////////////////////////////////////////////////

void TradeRouteEntryData::StationData::read(std::size_t& pos, const std::span<const unsigned char> data)
{
	ID = ReadInt32(pos, data);

	if (pos + 3 > data.size())
	{
		return;
	}
	SessionID = data[pos++];
	IslandID = data[pos++];
	AreaIndex = data[pos++];
	SessionGUID = ReadInt32(pos, data);
	Owner = ReadInt32(pos, data);

	if (pos + 1 > data.size())
	{
		return;
	}
	const int flags = data[pos++];
	ThrowOverboard = (flags & (1 << 0)) != 0;
	WaitUntilFull  = (flags & (1 << 1)) != 0;
	WaitUntilEmpty = (flags & (1 << 2)) != 0;

	const auto numGoods = ReadInt32(pos, data);
	Goods.resize(numGoods);
	for (auto& good : Goods)
	{
		good.read(pos, data);
	}
}

//////////////////////////////////////////////////////////////////////////

void TradeRouteEntryData::StationData::GoodsData::read(std::size_t& pos, const std::span<const unsigned char> data)
{
	GUID = ReadInt32(pos, data);
	Amount = ReadInt32(pos, data);

	if (pos + 1 > data.size())
	{
		return;
	}
	const int loadFlags = data[pos++];
	Loading = (loadFlags & (1 << 0)) != 0;
}

//////////////////////////////////////////////////////////////////////////

void ItemOfferEntryData::read(std::size_t& pos, const std::span<const unsigned char> data)
{
	const auto numItems = ReadInt32(pos, data);
	Items.resize(numItems);
	for (auto& item : Items)
	{
		item = ReadInt32(pos, data);
	}
}

//////////////////////////////////////////////////////////////////////////

[[noreturn]] void ErrorExit()
{
	const auto error = ::GetLastError();
	std::cout << "Error: " << error << std::endl;
	throw std::system_error{static_cast<int>(error), std::system_category()};
}

//////////////////////////////////////////////////////////////////////////

void RunPipe(std::stop_token stop, std::string& headline, std::map<std::string, std::map<std::int32_t, std::deque<anno_pipe::ProductionEntryData>>>& productionData,
	std::map<std::string, std::vector<anno_pipe::TradeRouteEntryData>>& tradeData, std::map<std::int32_t, ItemOfferEntryData>& itemOffers, std::mutex& dataMutex)
{
	auto ReadFromPipe = [&headline, &productionData, &tradeData, &itemOffers, &dataMutex](HANDLE pipe, unsigned char* const data, const std::size_t numBytes, DWORD& readBytes)
	{
		if (ReadFile(pipe, data, static_cast<DWORD>(numBytes), &readBytes, nullptr) == 0)
		{
			const auto error = ::GetLastError();
			if (error == ERROR_BROKEN_PIPE)
			{
				std::cout << "Pipe was closed.\n";

				std::lock_guard lock{ dataMutex };
				headline = "Session End";
				productionData.clear();
				tradeData.clear();
				itemOffers.clear();
				return false;
			}
			ErrorExit();
		}
		return true;
	};

	try{

	constexpr std::size_t maxMessageSize{ 1024 * 1024 };
	auto buffer = std::make_unique<std::array<unsigned char, maxMessageSize>>();

	while (!stop.stop_requested())
	{
		std::cout << "Waiting for pipe server.\n";
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			
			if (stop.stop_requested())
			{
				return;
			}

			if (WaitNamedPipe(pipeName, NMPWAIT_WAIT_FOREVER) == 0)
			{
				if (::GetLastError() != ERROR_FILE_NOT_FOUND)
				{
					ErrorExit();
				}
			}
			else
			{
				break;
			}
		}

		std::cout << "Connecting to pipe.\n";
		
		auto pipe = CreateFile(pipeName, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (pipe == INVALID_HANDLE_VALUE)
		{
			ErrorExit();
		}

		// Read preamble with version info.
		{
			DWORD readBytes{ 0 };
			if(!ReadFromPipe(pipe, buffer->data(), sizeof(std::int32_t), readBytes)) break;
			if (readBytes < 4)
			{
				std::cout << "Message too short.\n";
				continue;
			}

			std::size_t pos{ 0 };
			const auto size = static_cast<std::size_t>(ReadInt32(pos, {buffer->data(), 4}));
			if (size > buffer->size())
			{
				std::cout << "Message too long.\n";
				continue;
			}

			if(!ReadFromPipe(pipe, buffer->data(), size, readBytes)) break;
			pos = 0;
			if (readBytes != size)
			{
				std::cout << "Message size mismatched: " << readBytes << " != " << size << "\n";
				continue;
			}

			const auto type = static_cast<Message>((*buffer)[pos++]);
			if (type == Message::Version)
			{
				const auto version = ReadInt32(pos, {buffer->data(), size});
				std::cout << "Version " << version << "\n";
				if (version != PROTOCOL_VERSION)
				{
					std::cout << "... but expected " << PROTOCOL_VERSION << "\n";
				}
			}
			else
			{
				std::cout << "Expected version message but got " << static_cast<int>(type) << " instead.\n";
			}
		}

		// Read messages.
		while (!stop.stop_requested())
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));

			DWORD readBytes{ 0 };
			if(!ReadFromPipe(pipe, buffer->data(), 4u, readBytes)) break;
			if (readBytes < 4)
			{
				std::cout << "Message too short.\n";
				continue;
			}

			std::size_t pos{ 0 };
			const auto size = static_cast<std::size_t>(ReadInt32(pos, {buffer->data(), 4}));
			if (size > buffer->size())
			{
				std::cout << "Message too long.\n";
				continue;
			}

			if(!ReadFromPipe(pipe, buffer->data(), size, readBytes)) break;
			pos = 0;
			if (readBytes != size)
			{
				std::cout << "Message size mismatched: " << readBytes << " != " << size << "\n";
				continue;
			}

			std::lock_guard lock{ dataMutex };

			const auto type = static_cast<Message>((*buffer)[pos++]);

			if (type == Message::SessionEnd)
			{
				std::cout << "SessionEnd [" << readBytes << " bytes]\n";
				headline = "Session End";
				productionData.clear();
				tradeData.clear();
				itemOffers.clear();
			}
			else if (type == Message::SessionStart)
			{
				std::cout << "SessionStart[" << readBytes << " bytes]\n";
				headline = ReadString(pos, {buffer->data(), size});
				productionData.clear();
				tradeData.clear();
				itemOffers.clear();
			}
			else if (type == Message::AreaProductionStatistics)
			{
				std::cout << "AreaProductionStatistics[" << readBytes << " bytes]\n";
				if (pos + 3 > size)
				{
					continue;
				}
				const int sessionID = (*buffer)[pos++];
				const int islandID = (*buffer)[pos++];
				const int areaIndex = (*buffer)[pos++];

				const auto sessionGUID = ReadInt32(pos, {buffer->data(), size});

				const std::string areaName(ReadString(pos, {buffer->data(), size}));

				const auto timeStamp = ReadInt64(pos, {buffer->data(), size});

				const auto numEntries = ReadInt32(pos, {buffer->data(), size});

				std::cout << "area [" << sessionID << "|" << islandID << "|" << areaIndex << "] session [" << sessionGUID << "] " << areaName << ": " << timeStamp << " with " << numEntries << " entries\n";

				for (std::int32_t i = 0; i < numEntries; ++i)
				{
					ProductionEntryData entry;
					entry.read(pos, {buffer->data(), size});
					auto& q = productionData[areaName][entry.ProductGuid];
					q.push_back(entry);

					constexpr std::size_t historyLength{ 20 };
					if (q.size() > historyLength) 
					{
						q.pop_front();
					}
				}
			}
			else if (type == Message::TradeRoutes)
			{
				tradeData.clear();

				std::cout << "TradeRoutes[" << readBytes << " bytes]\n";

				const auto numEntries = ReadInt32(pos, {buffer->data(), size});
				for (std::int32_t i = 0; i < numEntries; ++i)
				{
					const auto tradeRouteID = ReadInt32(pos, {buffer->data(), size});

					TradeRouteEntryData entry;
					entry.read(pos, {buffer->data(), size});

					std::cout << "route [" << tradeRouteID << "|" << entry.Name << "|" << entry.Folder << "]" << ((entry.Errors > 0) ? " major problems" : "")
						<< ((entry.Warnings > 0) ? " minor problems" : "") << " #ships: " << entry.Ships << " #lost: " << entry.LostShips << " with: " << entry.Stations.size() << " stations\n";

					tradeData[entry.Folder].push_back(std::move(entry));
				}
			}
			else if (type == Message::ItemOffers)
			{
				itemOffers.clear();

				std::cout << "ItemOffers[" << readBytes << " bytes]\n";

				const auto numEntries = ReadInt32(pos, {buffer->data(), size});
				for (std::int32_t i = 0; i < numEntries; ++i)
				{
					const auto trader = ReadInt32(pos, {buffer->data(), size});

					ItemOfferEntryData entry;
					entry.read(pos, {buffer->data(), size});

					std::cout << "trader [" << trader << "]";
					for(const auto item : entry.Items)
					{
						std::cout << " " << item;
					}
					std::cout << "\n";

					itemOffers[trader] = std::move(entry);
				}
			}
		}

		::CloseHandle(pipe);
	}

	} catch (...)
	{
		std::cerr << "Aborting" << std::endl;
		std::exit(EXIT_FAILURE);
	}
}

}