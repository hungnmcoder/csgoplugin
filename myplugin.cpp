#include "myplugin.h"
#include "server_class.h"
#include "dt_send.h"

#include "sys/mman.h"

#include "playerdeathevent.h"
#include "playersayevent.h"
#include "playerconnectevent.h"
#include "playerdisconnectevent.h"
#include "roundstartevent.h"
#include "itempickupevent.h"
#include "playerspawnevent.h"
#include "playerspawnedevent.h"
#include "announcephaseendevent.h"

#include "hookmanager.h"
#include "hookfunctions.h"

class CBasePlayer;

IVEngineServer *vEngineServer = NULL;
IPlayerInfoManager *playerInfoManager = NULL;
IGameEventManager2 *gameEventManager2 = NULL;
IServerGameEnts *serverGameEnts = NULL;
CGlobalVars *globalVars = NULL;
IServerTools *serverTools = NULL;
IServerPluginHelpers *serverPluginHelpers = NULL;
IServerGameDLL *serverGameDLL = NULL;
AnnouncePhaseEndEvent *announphaseendevent = NULL;

// CBaseEntity
int m_iTeamNum_off;
int m_iPendingTeamNum_off;
int m_fEffects_off;
int m_nRenderMode_off;

// CBasePlayer
int m_iHealth_off;
int m_fFlags_off;

// CCSPlayerResource
int m_iCompetitiveRanking_off;

PlayerDeathEvent *playerDeathEvent = NULL;
PlayerSayEvent *playerSayEvent = NULL;
PlayerConnectEvent *playerConnectEvent = NULL;
PlayerDisconnectEvent *playerDisconnectEvent = NULL;
RoundStartEvent *roundStartEvent = NULL;
ItemPickupEvent *itemPickupEvent = NULL;
PlayerSpawnEvent *playerSpawnEvent = NULL;
PlayerSpawnedEvent *playerSpawnedEvent = NULL;

void MyPlugin::ClientActive(edict_t *pEntity)
{
	if (!pEntity || pEntity->IsFree())
		return;
	
	CBasePlayer *player = (CBasePlayer *)serverGameEnts->EdictToBaseEntity(pEntity);
	if (!player)
		return;
	
	HookFunctions *hk = new HookFunctions();
	hk->org_Weapon_CanUse = (fn_Weapon_CanUse)AddHook((void *)player, (void *)hk->Hook_Weapon_CanUse, 281);
}

bool MyPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	serverGameDLL = (IServerGameDLL *)gameServerFactory(INTERFACEVERSION_SERVERGAMEDLL, NULL);
	if (serverGameDLL)
	{
		ServerClass *svrclass = serverGameDLL->GetAllServerClasses();
		while (svrclass)
		{
			const char *classname = svrclass->GetName();
			Msg("[%s]\n", classname);
			if (strcmp(classname, "CBasePlayer") == 0)
			{
				SendTable *st = svrclass->m_pTable;
				for (int i = 0; i < st->m_nProps; i++)
				{
					SendProp *sp = st->GetProp(i);
					const char *propname = sp->GetName();
					Msg("Prop name: %s | Prop Offset: %d | Type: %d | IsSigned: %d\n", propname, sp->GetOffset(), sp->GetType(), sp->IsSigned());
					
					if (strcmp(propname, "m_fFlags") == 0)
					{
						m_fFlags_off = sp->GetOffset();
						continue;
					}
					
					if (strcmp(propname, "m_iHealth") == 0)
					{
						m_iHealth_off = sp->GetOffset();
						continue;
					}
				}
			}
			
			if (strcmp(classname, "CBaseEntity") == 0)
			{
				SendTable *st = svrclass->m_pTable;
				for (int i = 0; i < st->m_nProps; i++)
				{
					SendProp *sp = st->GetProp(i);
					const char *propname = sp->GetName();
					Msg("Prop name: %s | Prop Offset: %d | Type: %d | IsSigned: %d\n", propname, sp->GetOffset(), sp->GetType(), sp->IsSigned());
					
					if (strcmp(propname, "m_iTeamNum") == 0)
					{
						m_iTeamNum_off = sp->GetOffset();
						continue;
					}
					
					if (strcmp(propname, "m_iPendingTeamNum") == 0)
					{
						m_iPendingTeamNum_off = sp->GetOffset();
						continue;
					}
					
					if (strcmp(propname, "m_fEffects") == 0)
					{
						m_fEffects_off = sp->GetOffset();
						continue;
					}
					
					if (strcmp(propname, "m_nRenderMode") == 0)
					{
						m_nRenderMode_off = sp->GetOffset();
						continue;
					}
				}
			}
			
			svrclass = svrclass->m_pNext;
		}
	}
	else
	{
		Warning("Unable to load IServerGameDLL.\n");
		return false;
	}

	serverGameEnts = (IServerGameEnts *)gameServerFactory(INTERFACEVERSION_SERVERGAMEENTS, NULL);
	if (!serverGameEnts)
	{
		Warning("Unable to load IServerGameEnts.\n");
		return false;
	}

	playerInfoManager = (IPlayerInfoManager *)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER, NULL);
	if (playerInfoManager)
	{
		globalVars = playerInfoManager->GetGlobalVars();
	}
	else
	{
		Warning("Unable to load IPlayerInfoManager.\n");
		return false;
	}

	gameEventManager2 = (IGameEventManager2 *)interfaceFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2, NULL);
	if (!gameEventManager2)
	{
		Warning("Unable to load IGameEventManager2.\n");
		return false;
	}

	vEngineServer = (IVEngineServer *)interfaceFactory(INTERFACEVERSION_VENGINESERVER, NULL);
	if (!vEngineServer)
	{
		Warning("Unable to load IVEngineServer.\n");
		return false;
	}

	serverTools = (IServerTools *)gameServerFactory(VSERVERTOOLS_INTERFACE_VERSION, NULL);
	if (!serverTools)
	{
		Warning("Unable to load IServerTools.\n");
		return false;
	}

	serverPluginHelpers = (IServerPluginHelpers *)interfaceFactory(INTERFACEVERSION_ISERVERPLUGINHELPERS, NULL);
	if (!serverPluginHelpers)
	{
		Warning("Unable to load IServerPluginHelpers.\n");
		return false;
	}

	playerDeathEvent = new PlayerDeathEvent();
	playerSayEvent = new PlayerSayEvent();
	playerConnectEvent = new PlayerConnectEvent();
	playerDisconnectEvent = new PlayerDisconnectEvent();
	roundStartEvent = new RoundStartEvent();
	itemPickupEvent = new ItemPickupEvent();
	playerSpawnEvent = new PlayerSpawnEvent();
	playerSpawnedEvent = new PlayerSpawnedEvent();
	announphaseendevent = new AnnouncePhaseEndEvent();

	return true;
}

void MyPlugin::Unload()
{
	gameEventManager2->RemoveListener(playerDeathEvent);
	delete playerDeathEvent;
	gameEventManager2->RemoveListener(playerSayEvent);
	delete playerSayEvent;
	gameEventManager2->RemoveListener(playerConnectEvent);
	delete playerConnectEvent;
	gameEventManager2->RemoveListener(playerDisconnectEvent);
	delete playerDisconnectEvent;
	gameEventManager2->RemoveListener(roundStartEvent);
	delete roundStartEvent;
	gameEventManager2->RemoveListener(itemPickupEvent);
	delete itemPickupEvent;
	gameEventManager2->RemoveListener(playerSpawnEvent);
	delete playerSpawnEvent;
	gameEventManager2->RemoveListener(playerSpawnedEvent);
	delete playerSpawnedEvent;
	gameEventManager2->RemoveListener(announphaseendevent);
	delete announphaseendevent;
}

void MyPlugin::Pause()
{
	
}

void MyPlugin::UnPause()
{
	
}

const char *MyPlugin::GetPluginDescription()
{
	return "MyPlugin version 1.0 by hungnmcoder";
}

void MyPlugin::LevelInit(char const *pMapName)
{
	gameEventManager2->AddListener(playerDeathEvent, "player_death", true);
	gameEventManager2->AddListener(playerSayEvent, "player_say", true);
	gameEventManager2->AddListener(playerConnectEvent, "player_connect", true);
	gameEventManager2->AddListener(playerDisconnectEvent, "player_disconnect", true);
	gameEventManager2->AddListener(roundStartEvent, "round_start", true);
	gameEventManager2->AddListener(itemPickupEvent, "item_pickup", true);
	gameEventManager2->AddListener(playerSpawnEvent, "player_spawn", true);
	gameEventManager2->AddListener(playerSpawnedEvent, "player_spawned", true);
	gameEventManager2->AddListener(announphaseendevent, "announce_phase_end", true);
}

void MyPlugin::ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
	
}

void MyPlugin::GameFrame(bool simulating)
{
	
}

void MyPlugin::LevelShutdown()
{
	gameEventManager2->RemoveListener(playerDeathEvent);
	gameEventManager2->RemoveListener(playerSayEvent);
	gameEventManager2->RemoveListener(playerConnectEvent);
	gameEventManager2->RemoveListener(playerDisconnectEvent);
	gameEventManager2->RemoveListener(roundStartEvent);
	gameEventManager2->RemoveListener(itemPickupEvent);
	gameEventManager2->RemoveListener(playerSpawnEvent);
	gameEventManager2->RemoveListener(playerSpawnedEvent);
	gameEventManager2->RemoveListener(announphaseendevent);
}

void MyPlugin::OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue)
{
	
}

void MyPlugin::OnEdictAllocated(edict_t *edict)
{
	
}

void MyPlugin::OnEdictFreed(const edict_t *edict)
{
	
}


PLUGIN_RESULT MyPlugin::ClientConnect(bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen)
{
	return PLUGIN_CONTINUE;
}

void MyPlugin::ClientFullyConnect(edict_t *pEntity)
{

}

void MyPlugin::ClientPutInServer(edict_t *entity, const char *playername)
{

}

void MyPlugin::ClientDisconnect(edict_t *pEntity)
{

}

void MyPlugin::SetCommandClient(int index)
{
	
}

void MyPlugin::ClientSettingsChanged(edict_t *pEdict)
{
	
}

PLUGIN_RESULT MyPlugin::ClientCommand(edict_t *pEntity, const CCommand &args)
{
	return PLUGIN_CONTINUE;
}

PLUGIN_RESULT MyPlugin::NetworkIDValidated(const char *pszUserName, const char *pszNetworkID)
{
	return PLUGIN_CONTINUE;
}
