#include "plugin.h"


#include "../project_files/Logs.h"
#include "../project_files/SAMP.h"

using namespace plugin;

uint32_t g_dwSAMP_Addr = NULL;

std::string gamePath = std::filesystem::current_path().string();
std::string sampSkinsPath = std::string(gamePath + "\\custom_models").c_str();

std::map<std::string, SAMPCustomPedData*> customSkins;
std::map<int, SAMPSkinPedData*> pedData;


class SAMPSkins {
public:
    int loaded = 0;
    int sampLoaded = 0;


    SAMPSkins() {
        createLog();

        writeLog("Starting plugin (build 1)");

        createDirectory(sampSkinsPath);

        getSAMP();

        Events::processScriptsEvent.AddAfter([this] {
            if (!sampLoaded) {
                stSAMP* samp = stGetSampInfo();

                if (samp == NULL) {
                    writeLog("There is no SAMP info to use.");

                    return;
                }
                
                sampLoaded = 1;

                addClientCommand((char*)"reloadmodels", cmd_reloadmodels);
                addClientCommand((char*)"modelid", cmd_modelid);
                addClientCommand((char*)"model", cmd_model);
                addClientCommand((char*)"setmodel", cmd_setmodel);
                addClientCommand((char*)"models", cmd_models);
            }
            });

        Events::pedCtorEvent.AddAfter([this](CPed*) {
            if (!sampLoaded) {
                return;
            }

            if (!loaded) {
                loaded = 1;
               
                updateModels();
            }
            });

        Events::processScriptsEvent.AddAfter([this]() {
            if (!loaded) {
                return;
            }

            stSAMP* samp = stGetSampInfo();

            if (samp == NULL) {
                return;
            }

            std::vector<SAMPPlayer*> players;

            if (samp->pPools->pPlayer->pLocalPlayer->pSAMP_Actor->pGTA_Ped != NULL) {
                SAMPPlayer* player = new SAMPPlayer();
                player->ped = samp->pPools->pPlayer->pLocalPlayer->pSAMP_Actor->pGTA_Ped;
                player->name = samp->pPools->pPlayer->strLocalPlayerName;

                players.push_back(player);
            }

            if (samp->pPools->pPlayer->iIsListed != NULL) {
                for (int playerid = 0; playerid < SAMP_MAX_PLAYERS; playerid++)
                {

                    if (samp->pPools->pPlayer->iIsListed[playerid] != 1)
                        continue;

                    stRemotePlayer* remotePlayer = samp->pPools->pPlayer->pRemotePlayer[playerid];

                    if (remotePlayer->pPlayerData == NULL)
                        continue;

                    if (remotePlayer->pPlayerData->pSAMP_Actor == NULL)
                        continue;

                    if (remotePlayer->pPlayerData->pSAMP_Actor->pGTA_Ped != NULL) {
                        SAMPPlayer* player = new SAMPPlayer();
                        player->ped = remotePlayer->pPlayerData->pSAMP_Actor->pGTA_Ped;
                        player->name = remotePlayer->strPlayerName;

                        players.push_back(player);
                    }
                }
            }

            for (int index = 0; index < players.size(); index++) {
                SAMPPlayer* player = players[index];


                if (player == NULL) {
                    continue;
                }
                
                if (player->ped == NULL) {
                    continue;
                }
                
                int handleid = CPools::GetPedRef(player->ped);

                if (!pedData.contains(handleid)) {
                    if (player != NULL) {
                        writeLog("Identified new ped, associated with player " + player->name);

                        SAMPSkinPedData* data = new SAMPSkinPedData();
                        data->originalModelId = player->ped->m_nModelIndex;
                        data->lastRegisteredModel = player->ped->m_nModelIndex;
                        data->lastRegisteredName = player->name;

                        pedData.insert({ handleid, data });

                        setCustomSkin(data, player->ped);
                    }
                }
                else {
                    bool shouldUpdate = false;

                    SAMPSkinPedData* data = pedData.at(handleid);

                    if (player->ped->m_nModelIndex != data->lastRegisteredModel && player->ped->m_nModelIndex != 63544) {
                        writeLog("Player " + player->name + " has a new skin (" + std::to_string(player->ped->m_nModelIndex) + ", previously " + std::to_string(data->lastRegisteredModel) + ")");

                        data->lastRegisteredModel = player->ped->m_nModelIndex;

                        shouldUpdate = true;
                    }

                    if (player->name != data->lastRegisteredName) {
                        writeLog("Player " + player->name + " has a new name (" + player->name + ", previously " + data->lastRegisteredName + ")");

                        data->lastRegisteredName = player->name;

                        shouldUpdate = true;
                    }

                    if (shouldUpdate) {
                        setCustomSkin(data, player->ped);
                    }
                }
            }
            });
	
        Events::pedDtorEvent.AddBefore([this](CPed* ped) {
            int handleid = CPools::GetPedRef(ped);

            if (pedData.contains(handleid)) {
                pedData.erase(handleid);
            }
            });
};

    void createDirectory(std::string path) {
        std::filesystem::path skinsPath(path);

        if (!std::filesystem::exists(skinsPath)) {
            if (std::filesystem::create_directories(skinsPath)) {
                writeLog("Created directory: " + skinsPath.string());
            }
            else {
                writeLog("Failed to create directory: " + skinsPath.string());
            }
        }
        else {
            writeLog("Directory already exists: " + skinsPath.string());
        }
    }

    ~SAMPSkins() {
        writeLog("Cleaning up plugin");
    }
} SAMPSkinsPlugin;

void cmd_reloadmodels(char*)
{
    updateModels();

    addChatMessage("{FFFF00}Models have been reloaded.");
}

void cmd_modelid(char*)
{
    CPed* ped = FindPlayerPed();
    
    if (ped == NULL) {
        return;
    }

    int handleid = CPools::GetPedRef(ped);

    int modelid = ped->m_nModelIndex;

    if (pedData.contains(handleid) && pedData[handleid]->originalModelId > 0 && pedData[handleid]->originalModelId < 19000) {
        modelid = pedData[handleid]->originalModelId;
    }

    stSAMP* samp = stGetSampInfo();

    if (samp != NULL && samp->pPools->pPlayer != NULL) {
        addChatMessage(std::string("{FFFF00}You are currently using model id " + std::to_string(modelid) + ", rename your model to " + samp->pPools->pPlayer->strLocalPlayerName + "_" + std::to_string(modelid) + " to assign a custom skin.").c_str());
    }
    else {
        addChatMessage(std::string("{FFFF00}You are currently using model id " + std::to_string(modelid) + ".").c_str());
    }
}

void cmd_model(char* input) {
    if (strlen(input) == 0) {
        addChatMessage("{FFFF00}Use /model [model name] or /setmodel [playerid] [model name] to set a model to yourself or another player.");
        addChatMessage("{FFFF00}You can use /models to see a list of your loaded custom models.");
        
        return;
    }

    CPed* ped = FindPlayerPed();

    if (ped == NULL) {
        return;
    }

    if (!strcmp(input, "0")) {
        int handleid = CPools::GetPedRef(ped);

        if (pedData.contains(handleid)) {
            if (pedData[handleid]->originalModelId == 0) {
                return;
            }

            ped->SetModelIndex(pedData[handleid]->originalModelId);
            pedData[handleid]->lastRegisteredModel = ped->m_nModelIndex;

            addChatMessage("{FFFF00}Your custom skin has been taken off.");
        }

        return;
    }

    if (!customSkins.contains(std::string(input))) {
        addChatMessage("{FFFF00}That model name is not loaded, if you have added it use /reloadmodels.");

        return;
    }

    SAMPCustomPedData* customPedData = customSkins.at(std::string(input));

    stSAMP* samp = stGetSampInfo();

    SAMPPlayer* player = new SAMPPlayer();
    player->ped = samp->pPools->pPlayer->pLocalPlayer->pSAMP_Actor->pGTA_Ped;
    player->name = samp->pPools->pPlayer->strLocalPlayerName;

    SAMPSkinPedData* data = new SAMPSkinPedData();
    data->lastRegisteredModel = player->ped->m_nModelIndex;
    data->lastRegisteredName = player->name;
    
    int handleid = CPools::GetPedRef(player->ped);

    pedData.insert({ handleid, data });

    setCustomSkin(data, player->ped, input);
}

void cmd_setmodel(char* input) {
    if (strlen(input) == 0) {
        addChatMessage("{FFFF00}Use /model [model name] or /setmodel [playerid] [model name] to set a model to yourself or another player.");
        addChatMessage("{FFFF00}You can use /models to see a list of your loaded custom models.");

        return;
    }

    int playerid = -1;  // Variable to hold the first word
    char* modelNameInput = nullptr;  // Variable to hold the second word

    // Split the string by space
    playerid = atoi(strtok(input, " "));  // Get the first token
    modelNameInput = strtok(nullptr, " ");  // Get the second token

    // Check if tokens were found
    if (playerid == -1 || modelNameInput == nullptr) {
        addChatMessage("{FFFF00}Use /model [model name] or /setmodel [playerid] [model name] to set a model to yourself or another player.");
        addChatMessage("{FFFF00}You can use /models to see a list of your loaded custom models.");

        return;
    }

    stSAMP* samp = stGetSampInfo();

    if (samp->pPools->pPlayer->iIsListed[playerid] != 1) {
        addChatMessage("{FFFF00}That player is unavailable right now.");

        return;
    }

    stRemotePlayer* remotePlayer = samp->pPools->pPlayer->pRemotePlayer[playerid];

    if (remotePlayer->pPlayerData == NULL) {
        addChatMessage("{FFFF00}That player is unavailable right now.");

        return;
    }

    if (remotePlayer->pPlayerData->pSAMP_Actor == NULL) {
        addChatMessage("{FFFF00}That player is unavailable right now.");

        return;
    }

    if (remotePlayer->pPlayerData->pSAMP_Actor->pGTA_Ped == NULL) {
        addChatMessage("{FFFF00}That player is unavailable right now.");

        return;
    }

    SAMPPlayer* player = new SAMPPlayer();
    player->ped = remotePlayer->pPlayerData->pSAMP_Actor->pGTA_Ped;
    player->name = remotePlayer->strPlayerName;

    if (!strcmp(modelNameInput, "0")) {
        int handleid = CPools::GetPedRef(remotePlayer->pPlayerData->pSAMP_Actor->pGTA_Ped);

        if (pedData.contains(handleid)) {
            if (pedData[handleid]->originalModelId == 0) {
                return;
            }

            player->ped->SetModelIndex(pedData[handleid]->originalModelId);
            pedData[handleid]->lastRegisteredModel = player->ped->m_nModelIndex;

            addChatMessage("{FFFF00}Their custom skin has been taken off.");
        }

        return;
    }

    if (!customSkins.contains(std::string(modelNameInput))) {
        addChatMessage("{FFFF00}That model name is not loaded, if you have added it use /reloadmodels.");

        return;
    }


    SAMPCustomPedData* customPedData = customSkins.at(std::string(modelNameInput));


    SAMPSkinPedData* data = new SAMPSkinPedData();
    data->lastRegisteredModel = player->ped->m_nModelIndex;
    data->lastRegisteredName = player->name;

    int handleid = CPools::GetPedRef(player->ped);

    pedData.insert({ handleid, data });

    setCustomSkin(data, player->ped, modelNameInput);
}

void cmd_models(char*) {
    if (customSkins.size() > 0) {
        addChatMessage(std::string("{FFFF00}You have " + std::to_string(customSkins.size()) + " custom models loaded:").c_str());

        for (std::map<std::string, SAMPCustomPedData*>::iterator it = customSkins.begin(); it != customSkins.end(); ++it) {
            addChatMessage(std::string(it->first + " (found at " + it->second->folderPath + ")").c_str());
        }

        addChatMessage("{FFFF00}Use /model [model name] or /setmodel [playerid] [model name] to set a model to yourself or another player.");
        addChatMessage("{FFFF00}If you're missing any models, try /reloadmodels.");
    }
    else {
        addChatMessage("{FFFF00}You have no custom models loaded, try /reloadmodels if you have added some models.");
    }
}

void setCustomSkin(SAMPSkinPedData* data, CPed* ped, char* modelName) {
    std::string name = (modelName == NULL) ? (data->lastRegisteredName + "_" + std::to_string(ped->m_nModelIndex)) : (std::string(modelName));

    writeLog("Looking for custom skins to set for " + data->lastRegisteredName + " with skin " + name);

    if (ped->m_nModelIndex < 19000) {
        data->originalModelId = ped->m_nModelIndex;
    }

    if (customSkins.contains(name)) {
        SAMPCustomPedData* customPedData = customSkins.at(name);

        writeLog("Found custom skin: " + std::to_string(customPedData->modelId) + "; original id is " + std::to_string(data->originalModelId));

        if (data->originalModelId > 0) {
            setPedBaseModel(customPedData->pedModelInfo, data->originalModelId);
        }
        else {
            setPedBaseModel(customPedData->pedModelInfo, 1);
        }

        data->lastRegisteredModel = customPedData->modelId;
        ped->SetModelIndex(customPedData->modelId);
    }
    else {
        writeLog("Found nothing.");
    }
}

CPedModelInfo* createPedModel(int modelId, std::string path, char* modelName, int baseModelId) {
    writeLog("Loading " + std::string(modelName) + ".dff");

    std::string modelNameStr = std::string(modelName);

    int underscoreCount = std::count(modelNameStr.begin(), modelNameStr.end(), '_');

    if (underscoreCount == 2) {
        size_t lastUnderscorePos = modelNameStr.find_last_of('_');
        std::string numberStr = modelNameStr.substr(lastUnderscorePos + 1);
        int modelNameId = std::stoi(numberStr);

        CBaseModelInfo* baseModelInfo = CModelInfo::GetModelInfo(modelNameId);

        if (baseModelInfo != NULL) {
            if (baseModelInfo->GetModelType() != MODEL_INFO_PED) {
                writeLog("Unsure how to handle model type, skipping...");

                return NULL;
            }
        }
    }

    int txdSlot = CTxdStore::AddTxdSlot(modelName);
    CTxdStore::LoadTxd(txdSlot, std::string(std::string(path) + ".txd").c_str());
    CTxdStore::AddRef(txdSlot);
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(txdSlot);

    RwStream* dffStream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, std::string(std::string(path) + ".dff").c_str());

    if (!dffStream) {
        writeLog("Stream could not be opened!");

        return NULL;
    }

    if (!RwStreamFindChunk(dffStream, rwID_CLUMP, (RwUInt32*)NULL, (RwUInt32*)NULL)) {
        writeLog("Could not find clump chunk in stream!");
        
        return NULL;
    }

    RpClump* clump = RpClumpStreamRead(dffStream);

    if (clump == NULL) {
        writeLog("Clump was null!");
        
        return NULL;
    }

    RwStreamClose(dffStream, NULL);

    CPedModelInfo* modelInfo = CModelInfo::AddPedModel(modelId);

    modelInfo->ClearTexDictionary();
    modelInfo->SetClump(clump);

    modelInfo->SetTexDictionary(modelName);

    modelInfo->m_nTxdIndex = txdSlot;

    CTxdStore::PopCurrentTxd();

    return modelInfo;
}

void setPedBaseModel(CPedModelInfo* modelInfo, int baseModelId) {
    CPedModelInfo* baseModelInfo = (CPedModelInfo*)CModelInfo::GetModelInfo(baseModelId);

    modelInfo->SetColModel(baseModelInfo->m_pColModel, 0);
    modelInfo->m_nPedType = baseModelInfo->m_nPedType;
    modelInfo->m_animFileName = baseModelInfo->m_animFileName;
    modelInfo->m_dwAnimFileIndex = baseModelInfo->m_dwAnimFileIndex;
    modelInfo->m_nAnimType = baseModelInfo->m_nAnimType;
    modelInfo->m_nCarsCanDriveMask = baseModelInfo->m_nCarsCanDriveMask;
}

void updateModels() {
    // clean up players
    if (pedData.size() > 0) {
        for (std::map<int, SAMPSkinPedData*>::iterator it = pedData.begin(); it != pedData.end(); ++it) {
            CPed* ped = CPools::GetPed(it->first);

            if (ped != NULL) {
                if (ped->m_nModelIndex != it->second->originalModelId && it->second->originalModelId > 0) {
                    ped->SetModelIndex(it->second->originalModelId);
                }
            }
        }

        pedData.clear();
    }

    // clean up resources
    if (customSkins.size() > 0) {
        for (std::map<std::string, SAMPCustomPedData*>::iterator it = customSkins.begin(); it != customSkins.end(); ++it) {
            it->second->pedModelInfo->RemoveRef();

            CTxdStore::RemoveTxd(CTxdStore::FindTxdSlot(it->first.c_str()));
        }

        customSkins.clear();
    }

    writeLog("\nUpdating models");

    struct FileGroup {
        std::string folderPath;
        std::set<std::string> extensions;
    };

    std::map<std::string, FileGroup*> fileGroups;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(sampSkinsPath)) {
        if (entry.is_regular_file()) {
            std::filesystem::path filePath = entry.path();
            std::string folderPath = filePath.parent_path().string();
            std::string stem = filePath.stem().string();
            std::string extension = filePath.extension().string();

            if (!fileGroups.contains(stem)) {
                fileGroups[stem] = new FileGroup();
                fileGroups[stem]->folderPath = folderPath;
            }

            if (fileGroups[stem]->folderPath != folderPath) {
                writeLog("Warning! You have conflicting files spread across different folders!");
                writeLog("File " + stem + extension + " exists in " + folderPath + " but also " + fileGroups[stem]->folderPath);

                continue;
            }

            fileGroups[stem]->extensions.insert(extension);
        }
    }

    for (const auto& [fileName, fileGroup] : fileGroups) {
        if (fileGroup->extensions.count(".dff") && fileGroup->extensions.count(".txd")) {
            writeLog("Found matching files for: " + fileName + " (.dff and .txd)");

            size_t pos = fileGroup->folderPath.find(gamePath + "\\");
            if (pos != std::string::npos) {
                fileGroup->folderPath.erase(pos, (gamePath + "\\").length());
            }

            std::string path = fileGroup->folderPath + "\\" + fileName;
            std::string txdPath = path + ".txd";
            std::string dffPath = path + ".dff";

            writeLog("DFF: " + dffPath);
            writeLog("TXD: " + txdPath);

            //RpClump* clump = CFileLoader::LoadAtomicFile2Return(dffPath.c_str());


            int modelId = 19000 + customSkins.size();

            SAMPCustomPedData* customPedData = new SAMPCustomPedData();
            customPedData->modelId = modelId;
            customPedData->folderPath = fileGroup->folderPath;

            CPedModelInfo* pedModelInfo = createPedModel(modelId, path, (char*)fileName.c_str(), 1);

            if (pedModelInfo == NULL) {
                continue;
            }

            customPedData->pedModelInfo = pedModelInfo;

            customSkins.insert({ fileName, customPedData });
        }
    }

    writeLog("");
};
