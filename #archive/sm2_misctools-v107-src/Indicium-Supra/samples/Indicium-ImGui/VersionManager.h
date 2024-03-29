#pragma once
// Credits to Raymonf and davi for initial version of VersionManager
// Source: https://github.com/Raymonf/Forklift/blob/master/Forklift/VersionManager.h

enum class Version {
	// Shenmue 1
	Coconut100,
	Coconut101,
	Coconut102,
	Coconut103,
	Coconut104,
	Coconut105,
	Coconut106,
	Coconut107,

	// Shenmue 2
	Mango100,
	Mango101,
	Mango102,
	Mango103,
	Mango104,
	Mango105,
	Mango106,
	Mango107,

	Unknown
};

class VersionManager
{
public:
	static VersionManager* singleton();

	__int64 getHandleCreationAddress();
	__int64 getFileSizeAddress();
	__int64 getHasherSmallAddress();

	const char *getGameId();

	Version getVersion();

	static VersionManager* sInstance;
	VersionManager();

	Version version;
	__int64 handleCreationAddress;
	__int64 fileSizeAddress;
	__int64 hasherSmallAddress;
};

