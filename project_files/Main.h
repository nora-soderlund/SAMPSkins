#pragma once

#include "CPools.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <set>
#include <map>
#include <cstdint>

#include "CPed.h"
#include "CTxdStore.h"
#include "CStreaming.h"
#include "CModelInfo.h"
#include "CFileLoader.h"
#include "CPedModelInfo.h"
#include "NodeName.h"

extern uint32_t	g_dwSAMP_Addr;

struct SAMPSkinPedData {
    int lastRegisteredModel;
    std::string lastRegisteredName;
    int originalModelId;
};

struct SAMPCustomPedData {
    CPedModelInfo* pedModelInfo;
    int modelId;
    std::string folderPath;
};

struct SAMPPlayer {
    CPed* ped;
    std::string name;
};

void cmd_reloadmodels(char*);
void cmd_modelid(char*);
void cmd_model(char* input);
void cmd_setmodel(char* input);
void cmd_models(char*);

void setPedBaseModel(CPedModelInfo* modelInfo, int baseModelId);
void updateModels();
void setCustomSkin(SAMPSkinPedData* data, CPed* ped, char* modelName = NULL);