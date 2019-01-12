#pragma once

#include <stdint.h>
#include "structs.h"

// Function Definitions
typedef void(__stdcall *Hooked_DbgPrint_t) (char*, ...);
typedef signed __int64(__stdcall *charDispCheck_t)();
typedef __int64(__fastcall *npc_Callback_Enqueue_t)(uint8_t a1, int a2, int a3);
typedef signed __int64(__fastcall *NPC_Cleanup_Func_1_t)(__int64 a1);
typedef void(__fastcall *SwitchCameraMode_t)(struct cameraSwitch* cs);
typedef void(__fastcall *hooked_sub_1404B62C0_t)(__int64 a1, unsigned int a2, unsigned int a3, int a4);

// Task system function definitions
typedef __int64(__stdcall *MainLoop_t)();
typedef __int64(__fastcall *EnqueueTaskWithoutParam_t)(__int64 callbackFunction, uint8_t nextFunctionIndex, uint8_t r8b, unsigned int taskToken);
typedef __int64(__fastcall *EnqueueTaskWithParam_t)(__int64 callbackFunction, uint8_t nextFunctionIndex, __int64 a3, __int64 param, unsigned int taskToken, char* taskName);
typedef __int64(__fastcall *CleanupTaskFlags_t)(__int64 taskPointer);
typedef __int64(__fastcall *CleanupTask_t)(__int64 taskPointer);
typedef signed __int64(__fastcall *GetTaskParameterPointer_t)(__int64 a1);
