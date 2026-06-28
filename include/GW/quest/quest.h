#pragma once

#include "base/error_handling.h"

#include "GW/common/constants/constants.h"
#include "GW/context/quest.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace GW::quest {

bool Initialize();
void Shutdown();

using RequestQuestInfoFn = void(__cdecl*)(uint32_t identifier);
using RequestQuestDataFn = void(__cdecl*)(uint32_t identifier, bool update_markers);

GW::Constants::QuestID GetActiveQuestId();

// bool SetActiveQuestId(GW::Constants::QuestID quest_id);
// bool SetActiveQuest(Context::Quest* quest);
// bool AbandonQuest(Context::Quest* quest);
// bool AbandonQuestId(GW::Constants::QuestID quest_id);
// Deferred until UIMgr exists. Legacy behavior routes through UI message plumbing.

Context::Quest* GetActiveQuest();
Context::QuestLog* GetQuestLog();
Context::Quest* GetQuest(GW::Constants::QuestID quest_id);

bool GetQuestEntryGroupName(GW::Constants::QuestID quest_id, wchar_t* out, size_t out_len);

bool RequestQuestInfo(const Context::Quest* quest, bool update_markers = false);
bool RequestQuestInfoId(GW::Constants::QuestID quest_id, bool update_markers = false);

void AsyncGetQuestName(const Context::Quest* quest, std::wstring& res);
void AsyncGetQuestDescription(const Context::Quest* quest, std::wstring& res);
void AsyncGetQuestObjectives(const Context::Quest* quest, std::wstring& res);
void AsyncGetQuestLocation(const Context::Quest* quest, std::wstring& res);
void AsyncGetQuestNPC(const Context::Quest* quest, std::wstring& res);
void AsyncDecodeAnyEncStr(const wchar_t* str, std::wstring& res);

extern RequestQuestInfoFn g_request_quest_info_func;
extern RequestQuestDataFn g_request_quest_data_func;
extern std::atomic<bool> g_initialized;

}  // namespace GW::quest
