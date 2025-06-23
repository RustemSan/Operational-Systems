#ifndef __PROGTEST__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <functional>

/* Filesystem size: min 8MiB, max 1GiB
 * Filename length: min 1B, max 28B
 * Sector size: 512B
 * Max open files: 8 at a time
 * At most one filesystem mounted at a time.
 * Max file size: < 1GiB
 * Max files in the filesystem: 128
 */

constexpr int FILENAME_LEN_MAX = 28;
constexpr int DIR_ENTRIES_MAX = 128;
constexpr int OPEN_FILES_MAX = 8;
constexpr int SECTOR_SIZE = 512;
constexpr int DEVICE_SIZE_MAX = (1024 * 1024 * 1024);
constexpr int DEVICE_SIZE_MIN = (8 * 1024 * 1024);

struct TFile {
    char m_FileName[FILENAME_LEN_MAX + 1];
    size_t m_FileSize;
};

struct TBlkDev {
    size_t m_Sectors;
    std::function<size_t(size_t, void *, size_t)> m_Read;
    std::function<size_t(size_t, const void *, size_t)> m_Write;
};
#endif /* __PROGTEST__ */

struct FileSysHead {
    char sysVar[8];
    uint32_t filesOccupied;
    uint32_t fileSecOcc;
    uint32_t allignmentVar;
};

class StartProgramFile {
public:
    char name[FILENAME_LEN_MAX + 1]{};
    size_t size;
    size_t *sectorsArray;
    size_t sectorCount;
    size_t allocatedSectors;
    bool usedFlag;
    uint32_t rootIB;

    StartProgramFile()
            : size(0), sectorsArray(nullptr), sectorCount(0), allocatedSectors(16), usedFlag(false),
              rootIB(0xFFFFFFFF) {
        memset(name, 0, sizeof(name));
        sectorsArray = new size_t[allocatedSectors];
        memset(sectorsArray, 0, allocatedSectors * sizeof(size_t));
    }

    StartProgramFile(const StartProgramFile &other)
            : size(other.size), sectorCount(other.sectorCount), allocatedSectors(other.allocatedSectors),
              usedFlag(other.usedFlag), rootIB(other.rootIB) {
        memcpy(name, other.name, sizeof(name));
        sectorsArray = new size_t[allocatedSectors];
        memcpy(sectorsArray, other.sectorsArray, sectorCount * sizeof(size_t));
    }

    StartProgramFile &operator=(const StartProgramFile &other) {
        if (this != &other) {
            size = other.size;
            sectorCount = other.sectorCount;
            allocatedSectors = other.allocatedSectors;
            usedFlag = other.usedFlag;
            memcpy(name, other.name, sizeof(name));
            delete[] sectorsArray;
            sectorsArray = new size_t[allocatedSectors];
            memcpy(sectorsArray, other.sectorsArray, sectorCount * sizeof(size_t));
            rootIB = other.rootIB;
        }
        return *this;
    }

    ~StartProgramFile() {
        delete[] sectorsArray;
    }
};

struct ProgramOpenFile {
    StartProgramFile *fileStart;
    size_t tmpPos;
    bool writeFlag;
    bool openFLag;
};

struct InputFileDir {
    char name[FILENAME_LEN_MAX + 1];

    uint32_t size;
    uint32_t mainIndBlock;
    uint8_t usedFlag;
};

struct fileBlockID {
    uint32_t nextBlock;
    uint32_t tmpID[127];
};
// -----------------------------------------------------------



class CFileSystem {
public:
    static bool createFs(const TBlkDev &dev);

    static CFileSystem *mount(const TBlkDev &dev);

    bool umount();

    size_t fileSize(const char *fileName);

    int openFile(const char *fileName, bool writeMode);

    bool closeFile(int fd);

    size_t readFile(int fd, void *data, size_t len);

    size_t writeFile(int fd, const void *data, size_t len);

    bool deleteFile(const char *fileName);

    bool findFirst(TFile &file);

    bool findNext(TFile &file);

    ~CFileSystem();

private:
    TBlkDev m_dev;
    bool *fSectorBit;
    size_t fSectorBitSize;

    StartProgramFile files[DIR_ENTRIES_MAX];
    ProgramOpenFile openedFiles[OPEN_FILES_MAX];
    size_t findfPosition;
    size_t m_nextFree;

    static constexpr uint32_t FILE_INPUT_SIZE = 172;
    static constexpr int REC_PER_SEC = SECTOR_SIZE / sizeof(InputFileDir);


    bool loadFile();

    bool saveFile();

    int findFileID(const char *fileName);

    int firstFreePosInput();

    int firstFreeOpenFile();

    void secArrSearchSpace(StartProgramFile &f, size_t extraCount) {
        if (f.sectorCount + extraCount <= f.allocatedSectors)
            return;

        size_t newCap = f.allocatedSectors;
        while (newCap < (f.sectorCount + extraCount))
            newCap *= 2;

        size_t *newArr = new size_t[newCap];
        memcpy(newArr, f.sectorsArray, f.sectorCount * sizeof(size_t));
        memset(newArr + f.sectorCount, 0, (newCap - f.sectorCount) * sizeof(size_t));

        delete[] f.sectorsArray;
        f.sectorsArray = newArr;
        f.allocatedSectors = newCap;
    }

    bool checkReadMethod(uint32_t sector, void *buffer) {
        if (m_dev.m_Read(sector, buffer, 1) == 1)
            return true;
        else
            return false;
    }

    bool checkWriteMethod(uint32_t sector, const void *buffer) {
        if (m_dev.m_Write(sector, buffer, 1) == 1)
            return true;
        else
            return false;
    }


    bool findAllocFreeSec(uint32_t &outSec);

    void freeIBChain(uint32_t ibSec);

    CFileSystem(const TBlkDev &dev);
};


bool CFileSystem::findAllocFreeSec(uint32_t &tmpSec) {
    size_t start = m_nextFree;

    for (size_t i = start; i < fSectorBitSize; ++i) {
        if (!fSectorBit[i]) {
            fSectorBit[i] = true;
            m_nextFree = i + 1;
            tmpSec = i;

            return true;
        }
    }


    for (size_t i = 0; i < start; ++i) {
        if (!fSectorBit[i]) {
            fSectorBit[i] = true;
            m_nextFree = i + 1;
            tmpSec = i;

            return true;
        }
    }
    return false;
}


void CFileSystem::freeIBChain(uint32_t ibSec) {
    while (ibSec != 0xFFFFFFFF) {
        if (ibSec < fSectorBitSize)
            fSectorBit[ibSec] = false;

        char buf[SECTOR_SIZE];
        if (m_dev.m_Read(ibSec, buf, 1) != 1) break;
        ibSec = reinterpret_cast<fileBlockID *>(buf)->nextBlock;
    }
}

CFileSystem::CFileSystem(const TBlkDev &dev)
        : m_dev(dev), fSectorBit(nullptr), fSectorBitSize(dev.m_Sectors), findfPosition(0), m_nextFree(0) {
    fSectorBit = new bool[fSectorBitSize];

    memset(fSectorBit, 0, fSectorBitSize * sizeof(bool));

    for (int i = 0; i < DIR_ENTRIES_MAX; i++)
        files[i] = StartProgramFile{};


    for (int i = 0; i < OPEN_FILES_MAX; i++)
        openedFiles[i] = ProgramOpenFile{nullptr, 0, false, false};
}

CFileSystem::~CFileSystem() {
    delete[] fSectorBit;
}



bool CFileSystem::createFs(const TBlkDev &dev) {
    const size_t minSec = DEVICE_SIZE_MIN / SECTOR_SIZE;
    const size_t maxSec = DEVICE_SIZE_MAX / SECTOR_SIZE;
    if (dev.m_Sectors < minSec || dev.m_Sectors > maxSec)
        return false;

    CFileSystem fs(dev);

    const uint32_t REC_PER_SEC = SECTOR_SIZE / sizeof ( InputFileDir );
    const uint32_t direcSec    = (DIR_ENTRIES_MAX + REC_PER_SEC - 1) / REC_PER_SEC;
    const uint32_t bmSec = (dev.m_Sectors + SECTOR_SIZE - 1) / SECTOR_SIZE;

    for (uint32_t i = 0; i < 1 + direcSec + bmSec; ++i)
        fs.fSectorBit[i] = true;

    return fs.saveFile();
}


//---------------------------------------------------------------------------
CFileSystem *CFileSystem::mount(const TBlkDev &dev) {
    CFileSystem *fileSys = new CFileSystem(dev);

    if (!fileSys->loadFile()) {
        delete fileSys;
        return nullptr;
    }

    return fileSys;
}

//---------------------------------------------------------------------------
bool CFileSystem::umount() {
    for (int i = 0; i < OPEN_FILES_MAX; i++) {
        if (openedFiles[i].openFLag)
            closeFile(i);
    }
    bool tmpCheck = saveFile();
    return tmpCheck;
}

//---------------------------------------------------------------------------
size_t CFileSystem::fileSize(const char *fileName) {
    int idx = findFileID(fileName);
    if (idx < 0)
        return SIZE_MAX;
    return (files[idx].size == static_cast<size_t>( SIZE_MAX )) ? SIZE_MAX : files[idx].size;
}


//---------------------------------------------------------------------------
int CFileSystem::openFile(const char *fileName, bool writeMode) {
    int idx = findFileID(fileName);
    if (!writeMode && idx < 0)
        return -1;

    if (writeMode && idx < 0) {
        idx = firstFreePosInput();
        if (idx < 0) return -1;

        files[idx].usedFlag = true;
        files[idx].size = 0;
        files[idx].sectorCount = 0;
        files[idx].rootIB = 0xFFFFFFFF;
        memset(files[idx].name, 0, sizeof files[idx].name);
        strncpy(files[idx].name, fileName, FILENAME_LEN_MAX);
    }

    if (writeMode && idx >= 0) {
        for (size_t i = 0; i < files[idx].sectorCount; ++i)
            fSectorBit[files[idx].sectorsArray[i]] = false;

        freeIBChain(files[idx].rootIB);
        files[idx].rootIB = 0xFFFFFFFF;

        files[idx].size = 0;
        files[idx].sectorCount = 0;
        memset(files[idx].sectorsArray, 0,
               files[idx].allocatedSectors * sizeof(size_t));
    }

    int fd = firstFreeOpenFile();
    if (fd < 0)
        return -1;

    openedFiles[fd] = {&files[idx], 0, writeMode, true};
    return fd;
}


//---------------------------------------------------------------------------
bool CFileSystem::closeFile(int fd) {
    if (fd < 0 || fd >= OPEN_FILES_MAX || !openedFiles[fd].openFLag)
        return false;

    StartProgramFile &spf = *openedFiles[fd].fileStart;
    if (openedFiles[fd].tmpPos > spf.size)
        spf.size = openedFiles[fd].tmpPos;

    openedFiles[fd].openFLag = false;
    return true;
}

//---------------------------------------------------------------------------
size_t CFileSystem::readFile(int fd, void *data, size_t len) {
    if (fd < 0 || fd >= OPEN_FILES_MAX)
        return 0;
    auto &tmpOpenFile = openedFiles[fd];
    if (!tmpOpenFile.openFLag)
        return 0;
    if (tmpOpenFile.writeFlag) {
//        std::cout<< "debugprint" << std::endl;
    }
    StartProgramFile &tmpSPF = *tmpOpenFile.fileStart;
    if (tmpOpenFile.tmpPos >= tmpSPF.size)
        return 0;
    size_t needToRead = tmpSPF.size - tmpOpenFile.tmpPos;
    if (needToRead > len)
        needToRead = len;

    size_t tmpToTRead = 0;
    uint8_t *tmpDest = (uint8_t *) data;

    while (needToRead > 0) {
        uint32_t sectorIdx = (uint32_t) (tmpOpenFile.tmpPos / SECTOR_SIZE);
        if (sectorIdx >= tmpSPF.sectorCount)
            break;
        uint32_t offset = (uint32_t) (tmpOpenFile.tmpPos % SECTOR_SIZE);

        char sectorBuf[SECTOR_SIZE];
        if (!checkReadMethod(tmpSPF.sectorsArray[sectorIdx], sectorBuf))
            break;
        size_t tmpCouldRead = SECTOR_SIZE - offset;
        if (tmpCouldRead > needToRead) tmpCouldRead = needToRead;

        memcpy(tmpDest, sectorBuf + offset, tmpCouldRead);

        tmpDest = tmpDest + tmpCouldRead;
        tmpOpenFile.tmpPos = tmpOpenFile.tmpPos + tmpCouldRead;
        tmpToTRead = tmpToTRead + tmpCouldRead;
        needToRead = needToRead - tmpCouldRead;
    }

    return tmpToTRead;
}


//---------------------------------------------------------------------------
size_t CFileSystem::writeFile(int fd, const void *data, size_t len) {
    if (fd < 0 || fd >= OPEN_FILES_MAX)
        return 0;
    auto &tmpOpenFile = openedFiles[fd];
    if (!tmpOpenFile.openFLag || !tmpOpenFile.writeFlag)
        return 0;

    StartProgramFile &tmpSPF = *tmpOpenFile.fileStart;
    size_t totalBitWritten = 0;
    const uint8_t *src = static_cast<const uint8_t *>(data);

    while (len > 0) {
        uint32_t sectorIndex = static_cast<uint32_t>(tmpOpenFile.tmpPos / SECTOR_SIZE);
        uint32_t offset = static_cast<uint32_t>(tmpOpenFile.tmpPos % SECTOR_SIZE);

        if (sectorIndex >= tmpSPF.sectorCount) {
            uint32_t newSec;
            if (!findAllocFreeSec(newSec))
                break;

            secArrSearchSpace(tmpSPF, 1);
            tmpSPF.sectorsArray[tmpSPF.sectorCount++] = newSec;
            continue;
        }
        uint32_t physSec = static_cast<uint32_t>(tmpSPF.sectorsArray[sectorIndex]);

        bool splitSpaceFlag = (offset != 0 || (offset + len < SECTOR_SIZE));

        char sectorBuf[SECTOR_SIZE];
        if (splitSpaceFlag) {
            if (!checkReadMethod(physSec, sectorBuf))
                break;
        } else {
            memset(sectorBuf, 0, SECTOR_SIZE);
        }

        size_t canWrite = SECTOR_SIZE - offset;
        if (canWrite > len)
            canWrite = len;

        memcpy(sectorBuf + offset, src, canWrite);

        if (!checkWriteMethod(physSec, sectorBuf))
            break;

        src = src + canWrite;
        len = len - canWrite;
        totalBitWritten = totalBitWritten + canWrite;
        tmpOpenFile.tmpPos = tmpOpenFile.tmpPos + canWrite;
    }

    if (tmpOpenFile.tmpPos > tmpSPF.size)
        tmpSPF.size = tmpOpenFile.tmpPos;

    return totalBitWritten;
}

//---------------------------------------------------------------------------
bool CFileSystem::deleteFile(const char *fileName) {
    int idx = findFileID(fileName);
    if (idx < 0)
        return false;

    for (size_t i = 0; i < files[idx].sectorCount; ++i)
        fSectorBit[files[idx].sectorsArray[i]] = false;

    freeIBChain(files[idx].rootIB);

    files[idx] = StartProgramFile{};
    return true;
}

//---------------------------------------------------------------------------
bool CFileSystem::findFirst(TFile &file) {
    findfPosition = 0;
    return findNext(file);
}

bool CFileSystem::findNext(TFile &file) {
    while (findfPosition < DIR_ENTRIES_MAX && !files[findfPosition].usedFlag)
        findfPosition++;
    if (findfPosition >= DIR_ENTRIES_MAX)
        return false;
    strncpy(file.m_FileName, files[findfPosition].name, FILENAME_LEN_MAX);
    file.m_FileName[FILENAME_LEN_MAX] = '\0';

    file.m_FileSize = files[findfPosition].size;
    findfPosition++;


    return true;
}

//---------------------------------------------------------------------------
int CFileSystem::findFileID(const char *fileName) {
    for (int i = 0; i < DIR_ENTRIES_MAX; i++) {
        if (files[i].usedFlag && strcmp(files[i].name, fileName) == 0)
            return i;
    }
    return -1;
}

int CFileSystem::firstFreePosInput() {
    for (int i = 0; i < DIR_ENTRIES_MAX; i++) {
        if (!files[i].usedFlag)
            return i;
    }
    return -1;
}

int CFileSystem::firstFreeOpenFile() {
    for (int i = 0; i < OPEN_FILES_MAX; i++) {
        if (!openedFiles[i].openFLag)
            return i;
    }
    return -1;
}


bool CFileSystem::saveFile() {
    const uint32_t REC_PER_SEC = SECTOR_SIZE / sizeof ( InputFileDir );
    const uint32_t direcSec    = (DIR_ENTRIES_MAX + REC_PER_SEC - 1) / REC_PER_SEC;
    const uint32_t bmSec = (fSectorBitSize + SECTOR_SIZE - 1) / SECTOR_SIZE;
    const uint32_t bitMapStart = 1 + direcSec;

    char headSec[SECTOR_SIZE]{};
    auto &hdr = *reinterpret_cast<FileSysHead *>( headSec );
    strncpy(hdr.sysVar, "MYFS000", 7);
    hdr.filesOccupied = direcSec;
    hdr.fileSecOcc = bmSec;
    if (!checkWriteMethod(0, headSec))
        return false;

    char dirSec[SECTOR_SIZE]{};
    InputFileDir *d = reinterpret_cast<InputFileDir *>( dirSec );

    for (int fi = 0; fi < DIR_ENTRIES_MAX; ++fi) {
        int secOfs = fi / REC_PER_SEC;
        int recOfs = fi % REC_PER_SEC;

        if (recOfs == 0)
            memset(dirSec, 0, SECTOR_SIZE);

        InputFileDir &de = d[recOfs];
        de.mainIndBlock = 0xFFFFFFFF;

        if (files[fi].usedFlag) {
            strncpy(de.name, files[fi].name, FILENAME_LEN_MAX);
            de.name[FILENAME_LEN_MAX] = '\0';
            de.size = files[fi].size > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<uint32_t>( files[fi].size );
            de.usedFlag = 1;

            size_t pos = 0;
            uint32_t prev = 0xFFFFFFFF;

            while (pos < files[fi].sectorCount) {
                uint32_t ibSec;
                if (!findAllocFreeSec(ibSec))
                    return false;

                if (de.mainIndBlock == 0xFFFFFFFF)
                    de.mainIndBlock = ibSec;

                char ibBuf[SECTOR_SIZE]{};
                auto &ib = *reinterpret_cast<fileBlockID *>(ibBuf);

                size_t fill = 0;
                while (fill < 127 && pos < files[fi].sectorCount)
                    ib.tmpID[fill++] = static_cast<uint32_t>( files[fi].sectorsArray[pos++] );

                ib.nextBlock = 0xFFFFFFFF;

                if (prev != 0xFFFFFFFF) {
                    char prevBuf[SECTOR_SIZE];
                    if (!checkReadMethod(prev, prevBuf))
                        return false;
                    reinterpret_cast<fileBlockID *>( prevBuf )->nextBlock = ibSec;
                    if (!checkWriteMethod(prev, prevBuf))
                        return false;
                }
                prev = ibSec;

                if (!checkWriteMethod(ibSec, ibBuf))
                    return false;
            }
        }

        if (recOfs == REC_PER_SEC - 1)
            if (!checkWriteMethod(1 + secOfs, dirSec))
                return false;
    }


    for (uint32_t i = 0; i < bitMapStart + bmSec && i < fSectorBitSize; ++i)
        fSectorBit[i] = true;

    char bmpSec[SECTOR_SIZE]{};
    size_t cur = 0;
    for (uint32_t s = 0; s < bmSec; ++s) {
        for (size_t i = 0; i < SECTOR_SIZE && cur < fSectorBitSize; ++i, ++cur)
            bmpSec[i] = fSectorBit[cur] ? 1 : 0;
        if (!checkWriteMethod(bitMapStart + s, bmpSec))
            return false;
        memset(bmpSec, 0, SECTOR_SIZE);
    }

    int lastSec = direcSec - 1;
    int lastRec = DIR_ENTRIES_MAX % REC_PER_SEC;

    if (lastRec) {
        memset(dirSec + lastRec * sizeof(InputFileDir), 0, SECTOR_SIZE - lastRec * sizeof(InputFileDir));

        if (!checkWriteMethod(1 + lastSec, dirSec))
            return false;
    }


    return true;
}

bool CFileSystem::loadFile() {
    char headSec[SECTOR_SIZE];
    if (!checkReadMethod(0, headSec))
        return false;
    const auto &hdr = *reinterpret_cast<const FileSysHead *>( headSec );
    if (strncmp(hdr.sysVar, "MYFS000", 7))
        return false;

    const uint32_t REC_PER_SEC = SECTOR_SIZE / sizeof ( InputFileDir );
    const uint32_t direcSec    = (DIR_ENTRIES_MAX + REC_PER_SEC - 1) / REC_PER_SEC;
    const uint32_t bmSec = hdr.fileSecOcc;
    const uint32_t bitMapStart = 1 + direcSec;

    char dirSec[SECTOR_SIZE];
    for (int sec = 0, fi = 0; sec < static_cast<int>( direcSec ); ++sec) {
        if (!checkReadMethod(1 + sec, dirSec))
            return false;
        const InputFileDir *d = reinterpret_cast<const InputFileDir *>( dirSec );

        for (int rec = 0; rec < (int)REC_PER_SEC && fi < DIR_ENTRIES_MAX; ++rec, ++fi) {
            files[fi] = StartProgramFile{};
            if (!d[rec].usedFlag)
                continue;

            strncpy(files[fi].name, d[rec].name, FILENAME_LEN_MAX);
            files[fi].name[FILENAME_LEN_MAX] = '\0';
            files[fi].size = (d[rec].size == 0xFFFFFFFFu) ? static_cast<size_t>( SIZE_MAX )
                                                          : static_cast<size_t>(d[rec].size);

            files[fi].usedFlag = true;
            files[fi].rootIB = d[rec].mainIndBlock;

            uint32_t ibSec = d[rec].mainIndBlock;
            while (ibSec != 0xFFFFFFFF) {
                if (ibSec < fSectorBitSize)
                    fSectorBit[ibSec] = true;

                char ibBuf[SECTOR_SIZE];
                if (!checkReadMethod(ibSec, ibBuf))
                    return false;
                const auto &ib = *reinterpret_cast<const fileBlockID *>( ibBuf );

                for (int k = 0; k < 127 && ib.tmpID[k]; ++k) {
                    uint32_t ds = ib.tmpID[k];
                    if (ds < fSectorBitSize)
                        fSectorBit[ds] = true;

                    secArrSearchSpace(files[fi], 1);
                    files[fi].sectorsArray[files[fi].sectorCount++] = ds;
                }
                ibSec = ib.nextBlock;
            }
        }
    }

    char bmpSec[SECTOR_SIZE];
    size_t cur = 0;
    for (uint32_t s = 0; s < bmSec; ++s) {
        if (!checkReadMethod(bitMapStart + s, bmpSec))
            return false;
        for (size_t i = 0; i < SECTOR_SIZE && cur < fSectorBitSize; ++i, ++cur)
            fSectorBit[cur] = fSectorBit[cur] || bmpSec[i];
    }
    for (size_t i = 0; i < bitMapStart + bmSec && i < fSectorBitSize; ++i)
        fSectorBit[i] = true;

    m_nextFree = bitMapStart + bmSec;
    return true;
}


#ifndef __PROGTEST__

#include "simple_test.inc"

#endif
