#include <std_include.hpp>

namespace Steam
{
	HMODULE Overlay = 0;

	uint64_t Callbacks::CallID = 0;
	std::map<uint64_t, bool> Callbacks::Calls;
	std::map<uint64_t, Callbacks::Base*> Callbacks::ResultHandlers;
	std::vector<Callbacks::Result> Callbacks::Results;
	std::vector<Callbacks::Base*> Callbacks::CallbackList;

	uint64_t Callbacks::RegisterCall()
	{
		Callbacks::Calls[++Callbacks::CallID] = false;
		return Callbacks::CallID;
	}

	void Callbacks::RegisterCallback(Callbacks::Base* handler, int callback)
	{
		handler->SetICallback(callback);
		Callbacks::CallbackList.push_back(handler);
	}

	void Callbacks::RegisterCallResult(uint64_t call, Callbacks::Base* result)
	{
		Callbacks::ResultHandlers[call] = result;
	}

	void Callbacks::ReturnCall(void* data, int size, int type, uint64_t call)
	{
		Callbacks::Result result;

		Callbacks::Calls[call] = true;

		result.call = call;
		result.data = data;
		result.size = size;
		result.type = type;

		Callbacks::Results.push_back(result);
	}

	void Callbacks::RunCallbacks()
	{
		for (auto result : Callbacks::Results)
		{
			if (Callbacks::ResultHandlers.find(result.call) != Callbacks::ResultHandlers.end())
			{
				Callbacks::ResultHandlers[result.call]->Run(result.data, false, result.call);
			}

			for (auto callback : Callbacks::CallbackList)
			{
				if (callback && callback->GetICallback() == result.type)
				{
					callback->Run(result.data, false, 0);
				}
			}

			if (result.data)
			{
				free(result.data);
			}
		}

		Callbacks::Results.clear();
	}

	extern "C"
	{
		bool SteamAPI_RestartAppIfNecessary()
		{
			return false;
		}

		bool SteamAPI_Init()
		{
			Overlay = GetModuleHandleA("gameoverlayrenderer.dll");

			if (!Overlay)
			{
				HKEY hRegKey;
				if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &hRegKey) == ERROR_SUCCESS)
				{
					char steamPath[MAX_PATH] = { 0 };
					DWORD dwLength = sizeof(steamPath);
					RegQueryValueExA(hRegKey, "InstallPath", NULL, NULL, reinterpret_cast<BYTE*>(steamPath), &dwLength);
					RegCloseKey(hRegKey);

					SetDllDirectoryA(steamPath);

					strcat_s(steamPath, "gameoverlayrenderer.dll");

					Overlay = LoadLibraryA(steamPath);
				}
			}

			return true;
		}

		void SteamAPI_RegisterCallResult(Callbacks::Base* result, uint64_t call)
		{
			Callbacks::RegisterCallResult(call, result);
		}

		void SteamAPI_RegisterCallback(Callbacks::Base* handler, int callback)
		{
			Callbacks::RegisterCallback(handler, callback);
		}

		void SteamAPI_RunCallbacks()
		{
			Callbacks::RunCallbacks();
		}

		void SteamAPI_Shutdown()
		{
		}

		void SteamAPI_UnregisterCallResult()
		{
		}

		void SteamAPI_UnregisterCallback()
		{
		}


		bool SteamGameServer_Init()
		{
			return true;
		}

		void SteamGameServer_RunCallbacks()
		{
		}

		void SteamGameServer_Shutdown()
		{
		}


		Steam::Friends* SteamFriends()
		{
			static Steam::Friends iFriends;
			return &iFriends;
		}

		Steam::Matchmaking* SteamMatchmaking()
		{
			static Steam::Matchmaking iMatchmaking;
			return &iMatchmaking;
		}

		Steam::MatchmakingServers* SteamMatchmakingServers()
		{
			static Steam::MatchmakingServers iMatchmakingServers;
			return &iMatchmakingServers;
		}

		Steam::GameServer* SteamGameServer()
		{
			static Steam::GameServer iGameServer;
			return &iGameServer;
		}

		Steam::MasterServerUpdater* SteamMasterServerUpdater()
		{
			static Steam::MasterServerUpdater iMasterServerUpdater;
			return &iMasterServerUpdater;
		}

		Steam::Networking* SteamNetworking()
		{
			static Steam::Networking iNetworking;
			return &iNetworking;
		}

		Steam::RemoteStorage* SteamRemoteStorage()
		{
			static Steam::RemoteStorage iRemoteStorage;
			return &iRemoteStorage;
		}

		Steam::User* SteamUser()
		{
			static Steam::User iUser;
			return &iUser;
		}

		Steam::Utils* SteamUtils()
		{
			static Steam::Utils iUtils;
			return &iUtils;
		}

		Steam::Apps* SteamApps()
		{
			static Steam::Apps iApps;
			return &iApps;
		}

		Steam::UserStats* SteamUserStats()
		{
			static Steam::UserStats iUserStats;
			return &iUserStats;
		}
	}
}
