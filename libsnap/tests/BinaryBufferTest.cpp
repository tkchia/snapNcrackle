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

// Include headers from C modules under test.
extern "C"
{
    #include "BinaryBuffer.h"
    #include "MallocFailureInject.h"
    #include "FileFailureInject.h"
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


static const char*         g_filename = "BinaryBufferTest.test";
static const char*         g_filename2 = "BinaryBufferTest2.test";
static const unsigned char g_testData[2] = { 0x00, 0xff };

TEST_GROUP(BinaryBuffer)
{
    BinaryBuffer*   m_pBinaryBuffer;
    unsigned char*  m_pAlloc;
    size_t          m_allocSize;
    FILE*           m_pFile;
    char*           m_pReadBuffer;
    
    void setup()
    {
        clearExceptionCode();
        m_pBinaryBuffer = NULL;
        m_pAlloc = NULL;
        m_allocSize = 0;
        m_pFile = NULL;
        m_pReadBuffer = NULL;
    }

    void teardown()
    {
        MallocFailureInject_Restore();
        fopenRestore();
        fwriteRestore();
        LONGS_EQUAL(noException, getExceptionCode());
        if (m_pFile)
            fclose(m_pFile);
        free(m_pReadBuffer);
        BinaryBuffer_Free(m_pBinaryBuffer);
        remove(g_filename);
        remove(g_filename2);
    }
    
    void validateOutOfMemoryExceptionThrown()
    {
        LONGS_EQUAL(outOfMemoryException, getExceptionCode());
        clearExceptionCode();
    }
    
    void placeDataInBuffer(const unsigned char* pData, size_t dataSize)
    {
        m_allocSize = dataSize;
        if (!m_pBinaryBuffer)
            m_pBinaryBuffer = BinaryBuffer_Create(64*1024);
        m_pAlloc = BinaryBuffer_Alloc(m_pBinaryBuffer, m_allocSize);
        memcpy(m_pAlloc, pData, dataSize);
    }
    
    void validateObjectFileContains(const char* pFilename, const unsigned char* pExpectedContent, long expectedContentSize)
    {
        m_pFile = fopen(pFilename, "r");
        CHECK(m_pFile != NULL);
        LONGS_EQUAL(expectedContentSize, getFileSize(m_pFile));
        
        m_pReadBuffer = (char*)malloc(expectedContentSize);
        CHECK(m_pReadBuffer != NULL);
        
        LONGS_EQUAL(expectedContentSize, fread(m_pReadBuffer, 1, expectedContentSize, m_pFile));
        CHECK(0 == memcmp(pExpectedContent, m_pReadBuffer, expectedContentSize));
        free(m_pReadBuffer);
        m_pReadBuffer = NULL;
        fclose(m_pFile);
        m_pFile = NULL;
    }
    
    long getFileSize(FILE* pFile)
    {
        fseek(pFile, 0, SEEK_END);
        long size = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);
        return size;
    }
};


TEST(BinaryBuffer, FailFirstAllocationDuringCreate)
{
    MallocFailureInject_FailAllocation(1);
    m_pBinaryBuffer = BinaryBuffer_Create(64*1024);
    POINTERS_EQUAL(NULL, m_pBinaryBuffer);
    validateOutOfMemoryExceptionThrown();
}

TEST(BinaryBuffer, FailSecondAllocationDuringCreate)
{
    MallocFailureInject_FailAllocation(2);
    m_pBinaryBuffer = BinaryBuffer_Create(64*1024);
    LONGS_EQUAL(NULL, m_pBinaryBuffer);
    validateOutOfMemoryExceptionThrown();
}

TEST(BinaryBuffer, Allocate1Item)
{
    m_pBinaryBuffer = BinaryBuffer_Create(64*1024);
    CHECK_TRUE(NULL != m_pBinaryBuffer);    
    unsigned char* pAlloc = BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
    CHECK_TRUE(NULL != pAlloc);
}

TEST(BinaryBuffer, Allocate2Items)
{
    m_pBinaryBuffer = BinaryBuffer_Create(64);
    unsigned char* pAlloc1 = BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
    unsigned char* pAlloc2 = BinaryBuffer_Alloc(m_pBinaryBuffer, 2);
    CHECK_TRUE(NULL != pAlloc2);
    CHECK_TRUE(pAlloc2 == pAlloc1+1);
}

TEST(BinaryBuffer, FailToAllocateItem)
{
    m_pBinaryBuffer = BinaryBuffer_Create(1);
    unsigned char* pAlloc = BinaryBuffer_Alloc(m_pBinaryBuffer, 2);
    validateOutOfMemoryExceptionThrown();
    POINTERS_EQUAL(NULL, pAlloc);
}

TEST(BinaryBuffer, ReallocFromNULLShouldBeSameAsAlloc)
{
    m_pBinaryBuffer = BinaryBuffer_Create(64);
    unsigned char* pAlloc = BinaryBuffer_Realloc(m_pBinaryBuffer, NULL, 1);
    CHECK_TRUE(NULL != pAlloc);
}

TEST(BinaryBuffer, ReallocToGrowBufferByOneByte)
{
    m_pBinaryBuffer = BinaryBuffer_Create(64);
    unsigned char* pAlloc1 = BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
    unsigned char* pAlloc2 = BinaryBuffer_Realloc(m_pBinaryBuffer, pAlloc1, 2);
    CHECK_TRUE(pAlloc1 == pAlloc2);
    unsigned char* pAlloc3 = BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
    CHECK_TRUE(pAlloc3 == pAlloc2 + 2);
}

TEST(BinaryBuffer, FailReallocBySpecifyingPointerOtherThanLastAllocated)
{
    m_pBinaryBuffer = BinaryBuffer_Create(64);
    unsigned char* pAlloc1 = BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
                             BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
    unsigned char* pAlloc3 = BinaryBuffer_Realloc(m_pBinaryBuffer, pAlloc1, 2);
    POINTERS_EQUAL(NULL, pAlloc3);
    LONGS_EQUAL(invalidArgumentException, getExceptionCode());
    clearExceptionCode();
}

TEST(BinaryBuffer, ForceFirstAllocToFail)
{
    m_pBinaryBuffer = BinaryBuffer_Create(64);
    BinaryBuffer_FailAllocation(m_pBinaryBuffer, 1);
    unsigned char* pAlloc1 = BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
    POINTERS_EQUAL(NULL, pAlloc1);
    validateOutOfMemoryExceptionThrown();
}

TEST(BinaryBuffer, ForceSecondAllocToFail)
{
    m_pBinaryBuffer = BinaryBuffer_Create(64);
    BinaryBuffer_FailAllocation(m_pBinaryBuffer, 2);
    unsigned char* pAlloc1 = BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
    CHECK_TRUE(NULL != pAlloc1);
    unsigned char* pAlloc2 = BinaryBuffer_Alloc(m_pBinaryBuffer, 1);
    POINTERS_EQUAL(NULL, pAlloc2);
    validateOutOfMemoryExceptionThrown();
}

TEST(BinaryBuffer, QueueWriteToFile)
{
    placeDataInBuffer(g_testData, sizeof(g_testData));
    BinaryBuffer_QueueWriteToFile(m_pBinaryBuffer, g_filename);
    BinaryBuffer_ProcessWriteFileQueue(m_pBinaryBuffer);
    validateObjectFileContains(g_filename, g_testData, sizeof(g_testData));
}

TEST(BinaryBuffer, FailFOpenDuringWriteToFile)
{
    placeDataInBuffer(g_testData, sizeof(g_testData));
    
    fopenFail(NULL);
    BinaryBuffer_QueueWriteToFile(m_pBinaryBuffer, g_filename);
    BinaryBuffer_ProcessWriteFileQueue(m_pBinaryBuffer);
    LONGS_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(BinaryBuffer, FailFWriteDuringWriteToFile)
{
    placeDataInBuffer(g_testData, sizeof(g_testData));
    
    fwriteFail(0);
    BinaryBuffer_QueueWriteToFile(m_pBinaryBuffer, g_filename);
    BinaryBuffer_ProcessWriteFileQueue(m_pBinaryBuffer);
    LONGS_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(BinaryBuffer, FailMemoryAllocationDuringWriteQueue)
{
    placeDataInBuffer(g_testData, sizeof(g_testData));
    MallocFailureInject_FailAllocation(1);
    BinaryBuffer_QueueWriteToFile(m_pBinaryBuffer, g_filename);
    validateOutOfMemoryExceptionThrown();
}

TEST(BinaryBuffer, FailToQueueALongFilename)
{
    char longFilename[512];
    
    memset(longFilename, 'A', sizeof(longFilename)-1);
    longFilename[sizeof(longFilename)-1] = '\0';
    
    placeDataInBuffer(g_testData, sizeof(g_testData));
    BinaryBuffer_QueueWriteToFile(m_pBinaryBuffer, longFilename);
    LONGS_EQUAL(invalidArgumentException, getExceptionCode());
    clearExceptionCode();
}

TEST(BinaryBuffer, SetOriginBeforeAnyAllocations)
{
    m_pBinaryBuffer = BinaryBuffer_Create(64);
    LONGS_EQUAL(0, BinaryBuffer_GetOrigin(m_pBinaryBuffer));
    BinaryBuffer_SetOrigin(m_pBinaryBuffer, 0x8000);
    LONGS_EQUAL(0x8000, BinaryBuffer_GetOrigin(m_pBinaryBuffer));
}

TEST(BinaryBuffer, QueueUPTwoWritesFromBuffer)
{
    static const unsigned char testData1[2] = { 1, 2 };
    static const unsigned char testData2[2] = { 3, 4 };
    
    m_pBinaryBuffer = BinaryBuffer_Create(4);
    BinaryBuffer_SetOrigin(m_pBinaryBuffer, 0x800);
    placeDataInBuffer(testData1, sizeof(testData1));
    BinaryBuffer_QueueWriteToFile(m_pBinaryBuffer, g_filename);
    
    BinaryBuffer_SetOrigin(m_pBinaryBuffer, 0x900);
    placeDataInBuffer(testData2, sizeof(testData2));
    BinaryBuffer_QueueWriteToFile(m_pBinaryBuffer, g_filename2);


    BinaryBuffer_ProcessWriteFileQueue(m_pBinaryBuffer);
    validateObjectFileContains(g_filename, testData1, sizeof(testData1));
    validateObjectFileContains(g_filename2, testData2, sizeof(testData2));
}
