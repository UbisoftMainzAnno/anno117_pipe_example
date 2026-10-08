#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "imgui_impl.h"

#include "pipe.h"

void DrawImGui(std::mutex& productionDataMutex, std::map<std::string, std::map<std::int32_t, std::deque<anno_pipe::ProductionEntryData>>>& productionData, const std::string& headline, const std::map<std::string, std::vector<anno_pipe::TradeRouteEntryData>>& tradeData, const std::map<std::int32_t, anno_pipe::ItemOfferEntryData>& itemOffers)
{
	std::lock_guard lock{ productionDataMutex };

	ImGui::SetNextWindowSize({ 640.f, 800.f }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos({ 0.f, 0.f }, ImGuiCond_FirstUseEver);

	ImGui::Begin("Production");
	ImGui::Text(headline.c_str());
	for (auto& [name, products] : productionData)
	{
		if (ImGui::CollapsingHeader(name.c_str()))
		{
			ImGui::PushID(name.c_str());
			for (auto& [guid, entries] : products)
			{
				ImGui::PushID(guid);
				ImGui::PlotLines((std::to_string(guid) + std::string(" ProductGeneration")).c_str(), [](void* const data, const int index) -> float
					{
						const auto* const q = static_cast<const std::deque<anno_pipe::ProductionEntryData>*>(data);
						return q->at(index).ProductGeneration;
					}, static_cast<void*>(&entries), static_cast<int>(entries.size()), 0, std::to_string(entries.back().ProductGeneration).c_str(), 0.f, FLT_MAX, { 400.f, 30.f });
				ImGui::PopID();
			}
			ImGui::PopID();
		}
	}
	ImGui::End();

	ImGui::SetNextWindowSize({ 640.f, 600.f }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos({ 640.f, 0.f }, ImGuiCond_FirstUseEver);

	ImGui::Begin("Trade Routes");
	ImGui::PushID("Trade Routes");
	for (const auto& [folder, routes] : tradeData)
	{
		if (ImGui::TreeNode(folder.c_str()))
		{
			ImGui::PushID(folder.c_str());
			for (const auto& route : routes)
			{
				const auto label = std::format("{}{}{}", route.Name, (route.Errors ? " [!]" : ""), (route.Warnings ? " [?]" : ""));
				if (ImGui::TreeNode(label.c_str()))
				{
					ImGui::PushID(route.Name.c_str());
					ImGui::Text("%d active ships, %d lost ships", route.Ships, route.LostShips);
					if (!route.Stations.empty() && ImGui::BeginTabBar("stations"))
					{
						for (const auto& station : route.Stations)
						{
							const auto stationLabel = std::format("{}[{}]", station.ID, station.Owner);
							if (ImGui::BeginTabItem(stationLabel.c_str()))
							{
								for (const auto& good : station.Goods)
								{
									ImGui::Text("%s %dx [%d]", (good.Loading ? "->" : "<-"), good.Amount, good.GUID);
								}

								ImGui::EndTabItem();
							}
						}

						ImGui::EndTabBar();
					}
					ImGui::PopID();
					ImGui::TreePop();
				}
			}
			ImGui::PopID();
			ImGui::TreePop();
		}
	}
	ImGui::PopID();
	ImGui::End();

	ImGui::SetNextWindowSize({ 640.f, 200.f }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos({ 640.f, 600.f }, ImGuiCond_FirstUseEver);

	ImGui::Begin("Item Offers");
	ImGui::PushID("Item Offers");
	for (const auto& [trader, items] : itemOffers)
	{
		const auto label = std::format("Trader {}", trader);
		if (ImGui::CollapsingHeader(label.c_str()))
		{
			ImGui::PushID(label.c_str());
			constexpr int cols = 4;

			if (ImGui::BeginTable("items", cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32({0.3f, 0.3f, 0.3f, 1.f}));

				std::size_t i = 0;
				while (i < items.Items.size())
				{
					ImGui::TableNextRow();

					for (int j = 0; j < cols; ++j)
					{
						if(i + j < items.Items.size())
						{
							ImGui::TableSetColumnIndex(j);

							const auto itemLabel = std::format("[{}]", items.Items[i + j]);
							ImGui::Text(itemLabel.c_str());
						}
					}

					i += cols;
				}

				ImGui::EndTable();
			}

			ImGui::PopID();
		}
	}
	ImGui::PopID();
	ImGui::End();
}

int main()
{
	HWND hwnd;
	WNDCLASSEXW wc;
	if (SetupImgui(hwnd, wc) != 0)
	{
		return EXIT_FAILURE;
	}

	std::mutex dataMutex;
	std::map<std::string, std::map<std::int32_t, std::deque<anno_pipe::ProductionEntryData>>> productionData;
	std::map<std::string, std::vector<anno_pipe::TradeRouteEntryData>> tradeData;
	std::map<std::int32_t, anno_pipe::ItemOfferEntryData> itemOffersData;
	std::string headline;

	std::jthread pipe_thread{ anno_pipe::RunPipe, std::ref(headline), std::ref(productionData), std::ref(tradeData), std::ref(itemOffersData), std::ref(dataMutex) };

	bool done = false;
	while (!done)
	{
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				done = true;
			}
		}
		if (done)
		{
			break;
		}

		if ((g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) || ::IsIconic(hwnd))
		{
			::Sleep(10);
			continue;
		}
		g_SwapChainOccluded = false;

		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		DrawImGui(dataMutex, productionData, headline, tradeData, itemOffersData);

		// Rendering
		ImGui::Render();

		FrameContext* const frameCtx = WaitForNextFrameResources();
		const UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
		frameCtx->CommandAllocator->Reset();

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		g_pd3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
		g_pd3dCommandList->ResourceBarrier(1, &barrier);

		// Render Dear ImGui graphics
		constexpr ImVec4 clear_color(0.35f, 0.43f, 0.53f, 1.00f);
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, nullptr);
		g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);
		g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		g_pd3dCommandList->ResourceBarrier(1, &barrier);
		g_pd3dCommandList->Close();

		std::array<ID3D12CommandList*, 1> commands{ g_pd3dCommandList };
		g_pd3dCommandQueue->ExecuteCommandLists(static_cast<UINT>(commands.size()), commands.data());

		// Present
		const HRESULT hr = g_pSwapChain->Present(1, 0);
		g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

		const UINT64 fenceValue = g_fenceLastSignaledValue + 1;
		g_pd3dCommandQueue->Signal(g_fence, fenceValue);
		g_fenceLastSignaledValue = fenceValue;
		frameCtx->FenceValue = fenceValue;
	}

	pipe_thread.request_stop();

	CleanupImgui(hwnd, wc);

	if (pipe_thread.joinable())
	{
		std::this_thread::sleep_for(std::chrono::seconds{ 1 });
		std::exit(EXIT_SUCCESS);
	}
}