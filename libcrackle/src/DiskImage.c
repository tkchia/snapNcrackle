/*  Copyright (C) 2012  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#include <assert.h>
#include <string.h>
#include "DiskImagePriv.h"
#include "DiskImageTest.h"
#include "BinaryBuffer.h"
#include "util.h"


static void DiskImageScriptEngine_Init(DiskImageScriptEngine* pThis);
__throws void DiskImage_Init(DiskImage* pThis, DiskImageVTable* pVTable, unsigned int imageSize)
{
    __try
    {
        memset(pThis, 0, sizeof(*pThis));
        pThis->pVTable = pVTable;
        ByteBuffer_Allocate(&pThis->image, imageSize);
        DiskImageScriptEngine_Init(&pThis->script);
    }
    __catch
    {
        __rethrow;
    }
}

static void DiskImageScriptEngine_Init(DiskImageScriptEngine* pThis)
{
    __try
        pThis->pParser = ParseCSV_Create();
    __catch
        __rethrow;
}


static void DiskImageScriptEngine_Free(DiskImageScriptEngine* pThis);
void DiskImage_Free(DiskImage* pThis)
{
    if (!pThis)
        return;
    
    if (pThis->pVTable)
        pThis->pVTable->freeObject(pThis);
    ByteBuffer_Free(&pThis->object);
    ByteBuffer_Free(&pThis->image);
    DiskImageScriptEngine_Free(&pThis->script);
    free(pThis);
}

static void DiskImageScriptEngine_Free(DiskImageScriptEngine* pThis)
{
    ParseCSV_Free(pThis->pParser);
}


#define LOG_ERROR(pTHIS, FORMAT, ...) fprintf(stderr, \
                                       "%s:%d: error: " FORMAT LINE_ENDING, \
                                       pTHIS->pScriptFilename, \
                                       pTHIS->lineNumber, \
                                       __VA_ARGS__)

static void DiskImageScriptEngine_ProcessScriptFile(DiskImageScriptEngine* pThis, 
                                                    DiskImage*              pDiskImage, 
                                                    const char*             pScriptFilename);
static void processScriptFromTextFile(DiskImageScriptEngine* pThis);
static int isLineAComment(const char* pLineText);
static void processNextScriptLine(DiskImageScriptEngine* pThis, char* pScriptLine);
static void processBlockScriptLine(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields);
static unsigned int parseLengthField(DiskImageScriptEngine* pThis, const char* pLengthField);
static unsigned int parseFieldWhichSupportsAsteriskForDefaulValue(DiskImageScriptEngine* pThis, 
                                                                  const char*            pField, 
                                                                  unsigned int           defaultValue);
static void parseBlockRelatedFieldsAndSetInsertFields(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields);
static int isAsterisk(const char* pString);
static void setBlockInsertFieldsBasedOnLastInsertion(DiskImageScriptEngine* pThis);
static void setBlockInsertFieldsBaseOnScriptFields(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields);
static void rememberLastInsertionInformation(DiskImageScriptEngine* pThis);
static void processRWTS16ScriptLine(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields);
static void processRWTS18ScriptLine(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields);
static void reportScriptLineException(DiskImageScriptEngine* pThis);
static void closeTextFile(DiskImageScriptEngine* pThis);
__throws void DiskImage_ProcessScriptFile(DiskImage* pThis, const char* pScriptFilename)
{
    DiskImageScriptEngine_ProcessScriptFile(&pThis->script, pThis, pScriptFilename);
}

static void DiskImageScriptEngine_ProcessScriptFile(DiskImageScriptEngine* pThis, 
                                                    DiskImage*             pDiskImage, 
                                                    const char*            pScriptFilename)
{
    __try
    {
        SizedString scriptFilename = SizedString_InitFromString(pScriptFilename);
        pThis->pScriptFilename = pScriptFilename;
        pThis->pDiskImage = pDiskImage;
        pThis->pTextFile = TextFile_CreateFromFile(NULL, &scriptFilename, NULL);
    }
    __catch
    {
        LOG_ERROR(pThis, "Failed to open %s for parsing.", pScriptFilename);
        __rethrow;
    }
    
    processScriptFromTextFile(pThis);
}

static void processScriptFromTextFile(DiskImageScriptEngine* pThis)
{
    char* pNextLine;

    pThis->lineNumber = 1;
    while ((pNextLine = TextFile_GetNextLine(pThis->pTextFile)) != NULL)
    {
        if (!isLineAComment(pNextLine))
            processNextScriptLine(pThis, pNextLine);
        pThis->lineNumber++;
    }
    closeTextFile(pThis);
}

static int isLineAComment(const char* pLineText)
{
    return pLineText[0] == '#';
}

static void processNextScriptLine(DiskImageScriptEngine* pThis, char* pScriptLine)
{
    size_t       fieldCount;
    const char** ppFields;
    
    ParseCSV_Parse(pThis->pParser, pScriptLine);
    fieldCount = ParseCSV_FieldCount(pThis->pParser);
    ppFields = ParseCSV_FieldPointers(pThis->pParser);
    if (fieldCount < 1 || ppFields[0][0] == '\0')
    {
        LOG_ERROR(pThis, "%s cannot be blank.", "Script line");
        return;
    }
    
    __try
    {
        if (0 == strcasecmp(ppFields[0], "block"))
            processBlockScriptLine(pThis, fieldCount, ppFields);
        else if (0 == strcasecmp(ppFields[0], "rwts16"))
            processRWTS16ScriptLine(pThis, fieldCount, ppFields);
        else if (0 == strcasecmp(ppFields[0], "rwts18"))
            processRWTS18ScriptLine(pThis, fieldCount, ppFields);
        else
            LOG_ERROR(pThis, "%s isn't a recognized image insertion type of BLOCK or RWTS16.", ppFields[0]);
    }
    __catch
    {
        reportScriptLineException(pThis);
        __nothrow;
    }
}

static void processBlockScriptLine(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields)
{
    if (fieldCount < 5 || fieldCount > 6)
    {
        LOG_ERROR(pThis, 
                  "%s doesn't contain correct fields: BLOCK,objectFilename,objectStartOffset,insertionLength,block[,intraBlockOffset]",
                  "Line");
        __throw(invalidArgumentException);
    }
    
    __try
    {
        DiskImage_ReadObjectFile(pThis->pDiskImage, ppFields[1]);
        pThis->insert.sourceOffset = strtoul(ppFields[2], NULL, 0);
        pThis->insert.length = parseLengthField(pThis, ppFields[3]);
        pThis->insert.type = DISK_IMAGE_INSERTION_BLOCK;
        parseBlockRelatedFieldsAndSetInsertFields(pThis, fieldCount, ppFields);
        rememberLastInsertionInformation(pThis);
        DiskImage_InsertObjectFile(pThis->pDiskImage, &pThis->insert);
    }
    __catch
    {
        __rethrow;
    }
}

static unsigned int parseLengthField(DiskImageScriptEngine* pThis, const char* pLengthField)
{
    return parseFieldWhichSupportsAsteriskForDefaulValue(pThis, pLengthField, pThis->pDiskImage->objectFileLength);
}

static unsigned int parseFieldWhichSupportsAsteriskForDefaulValue(DiskImageScriptEngine* pThis, 
                                                                  const char*            pField, 
                                                                  unsigned int           defaultValue)
{
    if (isAsterisk(pField))
        return defaultValue;
    else
        return strtoul(pField, NULL, 0);
}

static void parseBlockRelatedFieldsAndSetInsertFields(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields)
{
    const char* pBlockField = ppFields[4];

    if (isAsterisk(pBlockField))
        setBlockInsertFieldsBasedOnLastInsertion(pThis);
    else
        setBlockInsertFieldsBaseOnScriptFields(pThis, fieldCount, ppFields);
}

static int isAsterisk(const char* pString)
{
    return 0 == strcmp(pString, "*");
}

static void setBlockInsertFieldsBasedOnLastInsertion(DiskImageScriptEngine* pThis)
{
    unsigned int lastOffset = pThis->lastBlock * DISK_IMAGE_BLOCK_SIZE + pThis->lastLength;
    pThis->insert.block = lastOffset / DISK_IMAGE_BLOCK_SIZE;
    pThis->insert.intraBlockOffset = lastOffset % DISK_IMAGE_BLOCK_SIZE;
}

static void setBlockInsertFieldsBaseOnScriptFields(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields)
{
    pThis->insert.block = strtoul(ppFields[4], NULL, 0);
    if (fieldCount > 5)
        pThis->insert.intraBlockOffset = strtoul(ppFields[5], NULL, 0);
    else
        pThis->insert.intraBlockOffset = 0;
}

static void rememberLastInsertionInformation(DiskImageScriptEngine* pThis)
{
    pThis->lastBlock = pThis->insert.block;
    pThis->lastLength = pThis->insert.length;
}

static void processRWTS16ScriptLine(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields)
{
    if (fieldCount != 6)
    {
        LOG_ERROR(pThis, 
                  "%s doesn't contain correct fields: RWTS16,objectFilename,objectStartOffset,insertionLength,track,sector",
                  "Line");
        __throw(invalidArgumentException);
    }
    
    __try
    {
        DiskImage_ReadObjectFile(pThis->pDiskImage, ppFields[1]);
        pThis->insert.sourceOffset = strtoul(ppFields[2], NULL, 0);
        pThis->insert.length = parseLengthField(pThis, ppFields[3]);
        pThis->insert.type = DISK_IMAGE_INSERTION_RWTS16;
        pThis->insert.track = strtoul(ppFields[4], NULL, 0);
        pThis->insert.sector = strtoul(ppFields[5], NULL, 0);
        DiskImage_InsertObjectFile(pThis->pDiskImage, &pThis->insert);
    }
    __catch
    {
        __rethrow;
    }
}

static void processRWTS18ScriptLine(DiskImageScriptEngine* pThis, size_t fieldCount, const char** ppFields)
{
    if (fieldCount != 8)
    {
        LOG_ERROR(pThis, 
                  "%s doesn't contain correct fields: "
                    "RWTS18,objectFilename,objectStartOffset,insertionLength,side,track,sector,offset",
                  "Line");
        __throw(invalidArgumentException);
    }
    
    __try
    {
        DiskImage_ReadObjectFile(pThis->pDiskImage, ppFields[1]);
        pThis->insert.type = DISK_IMAGE_INSERTION_RWTS18;
        pThis->insert.sourceOffset = strtoul(ppFields[2], NULL, 0);
        pThis->insert.length = parseLengthField(pThis, ppFields[3]);
        pThis->insert.side = parseFieldWhichSupportsAsteriskForDefaulValue(pThis, ppFields[4], 
                                                                           pThis->pDiskImage->insert.side);
        pThis->insert.track = parseFieldWhichSupportsAsteriskForDefaulValue(pThis, ppFields[5], 
                                                                            pThis->pDiskImage->insert.track);
        pThis->insert.sector = parseFieldWhichSupportsAsteriskForDefaulValue(pThis, ppFields[6], 
                                                                             pThis->pDiskImage->insert.sector);
        pThis->insert.intraSectorOffset = parseFieldWhichSupportsAsteriskForDefaulValue(pThis, ppFields[7], 
                                                                           pThis->pDiskImage->insert.intraSectorOffset);
        DiskImage_InsertObjectFile(pThis->pDiskImage, &pThis->insert);
    }
    __catch
    {
        __rethrow;
    }
}

static void reportScriptLineException(DiskImageScriptEngine* pThis)
{
    const char** ppFields = ParseCSV_FieldPointers(pThis->pParser);
    int          exceptionCode = getExceptionCode();
    
    assert ( exceptionCode == fileException || 
             exceptionCode == invalidArgumentException ||
             exceptionCode == blockExceedsImageBoundsException ||
             exceptionCode == invalidInsertionTypeException ||
             exceptionCode == invalidSectorException ||
             exceptionCode == invalidTrackException ||
             exceptionCode == invalidIntraBlockOffsetException ||
             exceptionCode == invalidIntraSectorOffsetException);

    if (exceptionCode == fileException)
        LOG_ERROR(pThis, "Failed to read '%s' object file.", ppFields[1]);
    else if (exceptionCode == blockExceedsImageBoundsException)
        LOG_ERROR(pThis, "Write starting at block %u offset %u won't fit in output image file.", 
                  pThis->insert.block, pThis->insert.intraBlockOffset);
    else if (exceptionCode == invalidInsertionTypeException)
        LOG_ERROR(pThis, "%s insertion type isn't supported for this output image type.", ppFields[0]);
    else if (exceptionCode == invalidSectorException)
        LOG_ERROR(pThis, "%u specifies an invalid sector.  Must be 0 - 15.", pThis->insert.sector);
    else if (exceptionCode == invalidTrackException)
        LOG_ERROR(pThis, "Write starting at track/sector %u/%u won't fit in output image file.", 
                  pThis->insert.track, pThis->insert.sector);
    else if (exceptionCode == invalidIntraBlockOffsetException)
        LOG_ERROR(pThis, "%u specifies an invalid intra block offset.  Must be 0 - 511.", 
                  pThis->insert.intraBlockOffset);
    else if (exceptionCode == invalidIntraSectorOffsetException)
        LOG_ERROR(pThis, "%u specifies an invalid intra sector offset.  Must be 0 - 255.", 
                  pThis->insert.intraSectorOffset);
}

static void closeTextFile(DiskImageScriptEngine* pThis)
{
    TextFile_Free(pThis->pTextFile);
    pThis->pTextFile = NULL;
}


static void DiskImageScriptEngine_ProcessScript(DiskImageScriptEngine* pThis, 
                                                DiskImage*             pDiskImage,
                                                char*                  pScriptText);
__throws void DiskImage_ProcessScript(DiskImage* pThis, char* pScriptText)
{
    DiskImageScriptEngine_ProcessScript(&pThis->script, pThis, pScriptText);
}

static void DiskImageScriptEngine_ProcessScript(DiskImageScriptEngine* pThis, 
                                                DiskImage*             pDiskImage,
                                                char*                  pScriptText)
{
    __try
    {
        pThis->pDiskImage = pDiskImage;
        pThis->pScriptFilename = "<null>";
        pThis->pTextFile = TextFile_CreateFromString(pScriptText);
    }
    __catch
    {
        __rethrow;
    }

    processScriptFromTextFile(pThis);
}


static FILE* openFile(const char* pFilename, const char* pMode);
static void determineObjectSizeFromFileHeader(DiskImage* pThis, FILE* pFile);
static int wasSAVedFromAssembler(const char* pSignature);
static int wasRW18SAVedFromAssembler(const char* pSignature);
static void readInRW18SavHeaderToSetDefaultInsertOptions(DiskImage* pThis, FILE* pFile, void* pvPartialHeader);
static RW18SavFileHeader readInRestOfRW18FileHeader(DiskImage* pThis, FILE* pFile, void* pPartialHeader);
static long getFileSize(FILE* pFile);
static unsigned int roundUpLengthToBlockSize(unsigned int length);
__throws void DiskImage_ReadObjectFile(DiskImage* pThis, const char* pFilename)
{
    FILE*         pFile = NULL;
    unsigned int  roundedObjectSize;
    
    __try
    {
        pFile = openFile(pFilename, "r");
        memset(&pThis->insert, 0, sizeof(pThis->insert));
        determineObjectSizeFromFileHeader(pThis, pFile);
        roundedObjectSize = roundUpLengthToBlockSize(pThis->objectFileLength);
        ByteBuffer_Allocate(&pThis->object, roundedObjectSize);
        ByteBuffer_ReadPartialFromFile(&pThis->object, pThis->objectFileLength, pFile);
    }
    __catch
    {
        if (pFile)
            fclose(pFile);
        __rethrow;
    }
    
    fclose(pFile);    
}

static FILE* openFile(const char* pFilename, const char* pMode)
{
    FILE* pFile = fopen(pFilename, pMode);
    if (!pFile)
        __throw(fileException);
    return pFile;
}

static void determineObjectSizeFromFileHeader(DiskImage* pThis, FILE* pFile)
{
    SavFileHeader     header;
    size_t            bytesRead;
    
    bytesRead = fread(&header, 1, sizeof(header), pFile);
    if (bytesRead == sizeof(header) && wasSAVedFromAssembler(header.signature))
    {
        pThis->objectFileLength = header.length;
    }
    else if (bytesRead == sizeof(header) && wasRW18SAVedFromAssembler(header.signature))
    {
        readInRW18SavHeaderToSetDefaultInsertOptions(pThis, pFile, &header);
    }
    else
    {
        pThis->objectFileLength = getFileSize(pFile);
        fseek(pFile, 0, SEEK_SET);
    }
}

static int wasSAVedFromAssembler(const char* pSignature)
{
    return 0 == memcmp(pSignature, BINARY_BUFFER_SAV_SIGNATURE, 4);
}

static int wasRW18SAVedFromAssembler(const char* pSignature)
{
    return 0 == memcmp(pSignature, BINARY_BUFFER_RW18SAV_SIGNATURE, 4);
}

static void readInRW18SavHeaderToSetDefaultInsertOptions(DiskImage* pThis, FILE* pFile, void* pPartialHeader)
{
    __try
    {
        RW18SavFileHeader rw18Header = readInRestOfRW18FileHeader(pThis, pFile, pPartialHeader);

        pThis->objectFileLength = rw18Header.length;
        pThis->insert.type = DISK_IMAGE_INSERTION_RWTS18;
        pThis->insert.length = rw18Header.length;
        pThis->insert.side = rw18Header.side;
        pThis->insert.track = rw18Header.track;
        pThis->insert.sector = rw18Header.offset / DISK_IMAGE_BYTES_PER_SECTOR;
        pThis->insert.intraSectorOffset = rw18Header.offset % DISK_IMAGE_BYTES_PER_SECTOR;
    }
    __catch
    {
        __rethrow;
    }
}

static RW18SavFileHeader readInRestOfRW18FileHeader(DiskImage* pThis, FILE* pFile, void* pPartialHeader)
{
    RW18SavFileHeader rw18Header;
    size_t            sizeDiffBetweenHeaders = sizeof(rw18Header) - sizeof(SavFileHeader);
    size_t            bytesRead;

    assert ( sizeof(rw18Header) >= sizeof(SavFileHeader) );
    memcpy(&rw18Header, pPartialHeader, sizeof(SavFileHeader));
    bytesRead = fread((char*)&rw18Header + sizeof(SavFileHeader), 1, sizeDiffBetweenHeaders, pFile);
    if (bytesRead != sizeDiffBetweenHeaders)
        __throw(fileException);
        
    return rw18Header;
}

static long getFileSize(FILE* pFile)
{
    fseek(pFile, 0, SEEK_END);
    long size = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);
    return size;
}

static unsigned int roundUpLengthToBlockSize(unsigned int length)
{
    return (length + (DISK_IMAGE_BLOCK_SIZE - 1)) & ~(DISK_IMAGE_BLOCK_SIZE - 1);
}


static void validateSourceObjectParameters(DiskImage* pThis, DiskImageInsert* pInsert);
__throws void DiskImage_InsertObjectFile(DiskImage* pThis, DiskImageInsert* pInsert)
{
    __try
        validateSourceObjectParameters(pThis, pInsert);
    __catch
        __rethrow;
    
    pThis->pVTable->insertData(pThis, pThis->object.pBuffer, pInsert);
}

static void validateSourceObjectParameters(DiskImage* pThis, DiskImageInsert* pInsert)
{
    if (pInsert->sourceOffset >= pThis->objectFileLength)
        __throw(invalidSourceOffsetException);
    if (pInsert->sourceOffset + pInsert->length > pThis->object.bufferSize)
        __throw(invalidLengthException);
}


__throws void DiskImage_WriteImage(DiskImage* pThis, const char* pImageFilename)
{
    FILE* pFile = NULL;

    __try
    {
        pFile = openFile(pImageFilename, "w");
        ByteBuffer_WriteToFile(&pThis->image, pFile);
    }
    __catch
    {
        fclose(pFile);
        __rethrow;
    }
    
    fclose(pFile);
}


unsigned char* DiskImage_GetImagePointer(DiskImage* pThis)
{
    return pThis->image.pBuffer;
}


size_t DiskImage_GetImageSize(DiskImage* pThis)
{
    return pThis->image.bufferSize;
}
