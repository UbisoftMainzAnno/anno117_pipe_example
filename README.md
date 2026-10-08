This example code demonstrates how to read statistics screen data from the **Anno 117** pipe.
It establishes a connection with the game via a Windows pipe, reads statistics data and displays it in raw format using ImGUI.

![Example](example.png)

## Experimental Feature
The pipe interface is provided as an easter egg and is primarily intended for curiosity, experimentation, and community-made tools.
It should not be considered an official or supported API, and compatibility across game versions is not guaranteed.
Likewise, the code in this repository is provided for documentation purposes only and should not be regarded as an officially supported SDK or reference implementation.

## Data
Data available follows the format outlined in `pipe.h`:
```C++
struct ProductionEntryData
{
	std::int32_t ProductGuid;

	float ProductGeneration;
	float ProductConsumption;
	float ProductDelta;
	float PerfectProductGeneration;
	float PerfectProductConsumption;
	std::int32_t AmountOfBuildings;
	std::int32_t TotalMaintenance;
	float TotalIncome;
	std::int32_t TotalProfit;
	float SummedProductivity;
	float AverageProductivity;
	
	std::unordered_map<std::int32_t, std::int32_t> WorkforceGUIDtoAmount;
	std::unordered_map<std::int32_t, std::int32_t> BuildingGUIDtoAmount;
};
```
See `pipe.cpp` for the communication protocol.

Protocol version 3 adds `TradeRouteEntryData` and `ItemOfferEntryData`.

## Activation
The pipe can be activated by adding the launch argument `/pipe` to the game.
Statistics data is sent periodically to the pipe.

## Build
The script `build.ps1` can be used to build the sample in a Visual Studio command line.