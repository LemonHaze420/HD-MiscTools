#pragma once

#include <stdint.h>

struct vec3
{
	float x, y, z;
};

struct vec4
{
	float x, y, z, w;
};

struct cameraSwitch {
	uint32_t f0;
	int32_t f4;
};

struct taskToken {
	char one;
	char two;
	char three;
	char four;
};

struct Task {
	uint64_t* callbackFuncPtr;
	char taskName[4];
	uint8_t unk1;
	uint8_t unk2;
	uint16_t pad0;
	uint64_t* unk3;
	uint64_t* nextTask;
	uint64_t* unk5;
	uint64_t* unk6;
	uint64_t* unk7;
	uint64_t* unk8;
	uint64_t* unk9;
	uint64_t* unk10;
	uint64_t* unk11;
	uint64_t* unk12;
	uint64_t* unk13;
	uint64_t* callbackParamPtr;
};

struct TaskQueue {
	Task Tasks[300];
};

struct StorageEntry {
	char typeName[4];
	uint32_t pad0;
	uint64_t contentSize;
	uint64_t* previousEntry;
	uint64_t* nextEntry;
	char usageToken[4];
	uint32_t pad1;
	uint64_t pad2;
	uint64_t pad3;
	uint64_t pad4;
	void* contentData;
};