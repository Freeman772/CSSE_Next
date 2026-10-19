#include "CSGameFile.h"

namespace se::cs {
	bool GameFile::getIsMasterFile() const {
		const auto GameFile_getIsMasterFile = reinterpret_cast<bool(__thiscall*)(const GameFile*)>(0x402B1C);
		return GameFile_getIsMasterFile(this);
	}

	bool GameFile::getToLoadFlag() const {
		const auto GameFile_getToLoadFlag = reinterpret_cast<bool(__thiscall*)(const GameFile*)>(0x40148D);
		return GameFile_getToLoadFlag(this);
	}

	void GameFile::setToLoadFlag(bool state) {
		const auto GameFile_setToLoadFlag = reinterpret_cast<void(__thiscall*)(GameFile*, bool)>(0x401D84);
		GameFile_setToLoadFlag(this, state);
	}

	int GameFile::sortAgainst(const GameFile* other) const {
		const auto isMaster = getIsMasterFile();
		const auto otherIsMaster = other->getIsMasterFile();
		if (isMaster != otherIsMaster) {
			return otherIsMaster ? 1 : -1;
		}

		return CompareFileTime(&findFileData.ftLastWriteTime, &other->findFileData.ftLastWriteTime);;
	}
}
