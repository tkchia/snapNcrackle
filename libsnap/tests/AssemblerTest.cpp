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
#include <stdlib.h>
extern "C"
{
    #include "SnapCommandLine.h"
    #include "Assembler.h"
    #include "../src/AssemblerPriv.h"
    #include "MallocFailureInject.h"
    #include "printfSpy.h"
    #include "BinaryBuffer.h"
    #include "util.h"
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


const char* g_sourceFilename = "AssemblerTest.S";
const char* g_objectFilename = "AssemblerTest.sav";

TEST_GROUP(Assembler)
{
    Assembler*      m_pAssembler;
    const char*     m_argv[10];
    FILE*           m_pFile;
    char*           m_pReadBuffer;
    SnapCommandLine m_commandLine;
    int             m_argc;
    int             m_isInvalidMode;
    int             m_isZeroPageTreatedAsAbsolute;
    char            m_buffer[128];
    unsigned char   m_expectedOpcode;
    
    void setup()
    {
        clearExceptionCode();
        printfSpy_Hook(512);
        m_pAssembler = NULL;
        m_pFile = NULL;
        m_pReadBuffer = NULL;
    }

    void teardown()
    {
        MallocFailureInject_Restore();
        printfSpy_Unhook();
        Assembler_Free(m_pAssembler);
        if (m_pFile)
            fclose(m_pFile);
        free(m_pReadBuffer);
        remove(g_sourceFilename);
        remove(g_objectFilename);
        LONGS_EQUAL(noException, getExceptionCode());
    }
    
    char* dupe(const char* pString)
    {
        strcpy(m_buffer, pString);
        return m_buffer;
    }
    
    void createSourceFile(const char* pString)
    {
        createThisSourceFile(g_sourceFilename, pString);
    }

    void createThisSourceFile(const char* pFilename, const char* pString)
    {
        FILE* pFile = fopen(pFilename, "w");
        fwrite(pString, 1, strlen(pString), pFile);
        fclose(pFile);
    }

    void addArg(const char* pArg)
    {
        CHECK(m_argc < (int)ARRAYSIZE(m_argv));
        m_argv[m_argc++] = pArg;
    }
    
    void validateFileNotFoundExceptionThrown()
    {
        validateExceptionThrown(fileNotFoundException);
    }
    
    void validateOutOfMemoryExceptionThrown()
    {
        validateExceptionThrown(outOfMemoryException);
    }
    
    void validateExceptionThrown(int expectedException)
    {
        POINTERS_EQUAL(NULL, m_pAssembler);
        LONGS_EQUAL(expectedException, getExceptionCode());
        clearExceptionCode();
    }
    
    void runAssemblerAndValidateOutputIs(const char* pExpectedOutput)
    {
        runAssemblerAndValidateLastLineIs(pExpectedOutput, 1);
    }
    
    void runAssemblerAndValidateLastLineIs(const char* pExpectedOutput, int expectedPrintfCalls)
    {
        Assembler_Run(m_pAssembler);
        LONGS_EQUAL(expectedPrintfCalls, printfSpy_GetCallCount());
        STRCMP_EQUAL(pExpectedOutput, printfSpy_GetLastOutput());
        LONGS_EQUAL(0, Assembler_GetErrorCount(m_pAssembler) );
    }
    
    void runAssemblerAndValidateOutputIsTwoLinesOf(const char* pExpectedOutput1, const char* pExpectedOutput2, int expectedPrintfCalls = 2)
    {
        Assembler_Run(m_pAssembler);
        LONGS_EQUAL(expectedPrintfCalls, printfSpy_GetCallCount());
        STRCMP_EQUAL(pExpectedOutput1, printfSpy_GetPreviousOutput());
        STRCMP_EQUAL(pExpectedOutput2, printfSpy_GetLastOutput());
        LONGS_EQUAL(0, Assembler_GetErrorCount(m_pAssembler) );
    }
    
    void runAssemblerAndValidateFailure(const char* pExpectedFailureMessage, const char* pExpectedListOutput)
    {
        runAssemblerAndValidateFailure(pExpectedFailureMessage, pExpectedListOutput, 2);
    }

    void runAssemblerAndValidateFailure(const char* pExpectedFailureMessage, const char* pExpectedListOutput, long expectedPrintfCalls)
    {
        Assembler_Run(m_pAssembler);
        LONGS_EQUAL(expectedPrintfCalls, printfSpy_GetCallCount());
        STRCMP_EQUAL(pExpectedFailureMessage, printfSpy_GetLastErrorOutput());
        STRCMP_EQUAL(pExpectedListOutput, printfSpy_GetLastOutput());
        LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler) );
    }
    
    void test6502Instruction(const char* pInstruction, const char* pInstructionTableEntry)
    {
        const char*        pCurr = pInstructionTableEntry;
    
        CHECK_TRUE(strlen(pInstructionTableEntry) >= 14*3-1);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testImmediateAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testAbsoluteAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testZeroPageAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testImpliedAddressing(pInstruction);

        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testZeroPageIndexedIndirectAddressing(pInstruction);

        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testIndirectIndexedAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testZeroPageIndexedXAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testZeroPageIndexedYAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testAbsoluteIndexedXAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testAbsoluteIndexedYAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testRelativeAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testAbsoluteIndirectAddressing(pInstruction);

        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testAbsoluteIndexedIndirectAddressing(pInstruction);
        
        pCurr = parseEntryAndAdvanceToNextEntry(pCurr);
        testZeroPageIndirectAddressing(pInstruction);
        
        CHECK_TRUE(*pCurr == '\0');
    }
    
    const char* parseEntryAndAdvanceToNextEntry(const char* pCurr)
    {
        m_isInvalidMode = FALSE;
        m_isZeroPageTreatedAsAbsolute = FALSE;
        if (0 == strncmp(pCurr, "XX", 2))
            m_isInvalidMode = TRUE;
        else
        {
            if (*pCurr == '^')
            {
                m_isZeroPageTreatedAsAbsolute = TRUE;
                pCurr++;
            }

            m_expectedOpcode = strtoul(pCurr, NULL, 16);
        }
            
        if (pCurr[2] == ',')
            return pCurr + 3;
        else
            return pCurr + 2;
    }
    
    void testImmediateAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        
        sprintf(testString, " %s #$ff\n", pInstruction);
        runAssembler(testString);
        if (!m_isInvalidMode)
        {
            sprintf(expectedListOutput, "8000: %02X FF        1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '#$ff' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testAbsoluteAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        
        sprintf(testString, " %s $100\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            sprintf(expectedListOutput, "8000: %02X 00 01     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '$100' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testZeroPageAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        sprintf(testString, " %s $ff\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            if (!m_isZeroPageTreatedAsAbsolute)
                sprintf(expectedListOutput, "8000: %02X FF        1 %s\n", m_expectedOpcode, testString);
            else
                sprintf(expectedListOutput, "8000: %02X FF 00     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '$ff' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testImpliedAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        
        sprintf(testString, " %s\n", pInstruction);
        runAssembler(testString);
        if (!m_isInvalidMode)
        {
            sprintf(expectedListOutput, "8000: %02X           1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '(null)' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testZeroPageIndexedIndirectAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        sprintf(testString, " %s ($ff,x)\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            if (!m_isZeroPageTreatedAsAbsolute)
                sprintf(expectedListOutput, "8000: %02X FF        1 %s\n", m_expectedOpcode, testString);
            else
                sprintf(expectedListOutput, "8000: %02X FF 00     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '($ff,x)' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testIndirectIndexedAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        
        sprintf(testString, " %s ($ff),y\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            sprintf(expectedListOutput, "8000: %02X FF        1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '($ff),y' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testZeroPageIndexedXAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        sprintf(testString, " %s $ff,x\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            if (!m_isZeroPageTreatedAsAbsolute)
                sprintf(expectedListOutput, "8000: %02X FF        1 %s\n", m_expectedOpcode, testString);
            else
                sprintf(expectedListOutput, "8000: %02X FF 00     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '$ff,x' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testZeroPageIndexedYAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        sprintf(testString, " %s $ff,y\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            if (!m_isZeroPageTreatedAsAbsolute)
                sprintf(expectedListOutput, "8000: %02X FF        1 %s\n", m_expectedOpcode, testString);
            else
                sprintf(expectedListOutput, "8000: %02X FF 00     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '$ff,y' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testAbsoluteIndexedXAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        
        sprintf(testString, " %s $100,x\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            sprintf(expectedListOutput, "8000: %02X 00 01     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '$100,x' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testAbsoluteIndexedYAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        
        sprintf(testString, " %s $100,y\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            sprintf(expectedListOutput, "8000: %02X 00 01     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '$100,y' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testRelativeAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        if (m_isInvalidMode)
            return;
        
        sprintf(testString, " %s *\n", pInstruction);
        runAssembler(testString);
        sprintf(expectedListOutput, "8000: %02X FE        1 %s\n", m_expectedOpcode, testString);
        validateSuccessfulOutput(expectedListOutput);
    }
    
    void testAbsoluteIndirectAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        
        sprintf(testString, " %s ($100)\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            sprintf(expectedListOutput, "8000: %02X 00 01     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '($100)' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testAbsoluteIndexedIndirectAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        CHECK_FALSE(m_isZeroPageTreatedAsAbsolute);
        
        sprintf(testString, " %s ($100,x)\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            sprintf(expectedListOutput, "8000: %02X 00 01     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '($100,x)' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void testZeroPageIndirectAddressing(const char* pInstruction)
    {
        char               testString[64];
        char               expectedListOutput[128];
        char               expectedErrorOutput[128];

        sprintf(testString, " %s ($ff)\n", pInstruction);
        runAssembler(testString);
        
        if (!m_isInvalidMode)
        {
            if (!m_isZeroPageTreatedAsAbsolute)
                sprintf(expectedListOutput, "8000: %02X FF        1 %s\n", m_expectedOpcode, testString);
            else
                sprintf(expectedListOutput, "8000: %02X FF 00     1 %s\n", m_expectedOpcode, testString);
            validateSuccessfulOutput(expectedListOutput);
        }
        else
        {
            sprintf(expectedErrorOutput, "filename:1: error: Addressing mode of '($ff)' is not supported for '%s' instruction.\n", pInstruction);
            sprintf(expectedListOutput, "    :              1 %s\n", testString);
            validateFailureOutput(expectedErrorOutput, expectedListOutput);
        }
    }
    
    void runAssembler(char* pTestString)
    {
        printfSpy_Unhook();
        printfSpy_Hook(512);
        m_pAssembler = Assembler_CreateFromString(pTestString);
        Assembler_Run(m_pAssembler);
        Assembler_Free(m_pAssembler);
        m_pAssembler = NULL;
    }
    
    void validateSuccessfulOutput(const char* pExpectedListOutput)
    {
        STRCMP_EQUAL(pExpectedListOutput, printfSpy_GetLastOutput());
        LONGS_EQUAL(1, printfSpy_GetCallCount());
    }
    
    void validateFailureOutput(const char* pExpectedErrorOutput, const char* pExpectedListOutput)
    {
        STRCMP_EQUAL(pExpectedErrorOutput, printfSpy_GetLastErrorOutput());
        STRCMP_EQUAL(pExpectedListOutput, printfSpy_GetLastOutput());
        LONGS_EQUAL(2, printfSpy_GetCallCount());
    }
    
    void validateLineInfo(const LineInfo*      pLineInfo, 
                          unsigned short       expectedAddress, 
                          size_t               expectedMachineCodeSize,
                          const char*          pExpectedMachineCode)
    {
        LONGS_EQUAL(expectedMachineCodeSize, pLineInfo->machineCodeSize);
        CHECK_TRUE(0 == memcmp(pLineInfo->pMachineCode, pExpectedMachineCode, expectedMachineCodeSize));
        LONGS_EQUAL(expectedAddress, pLineInfo->address);
    }
    
    void validateObjectFileContains(unsigned short expectedAddress, const char* pExpectedContent, long expectedContentSize)
    {
        SavFileHeader header;
        
        m_pFile = fopen(g_objectFilename, "r");
        CHECK(m_pFile != NULL);
        LONGS_EQUAL(expectedContentSize + sizeof(header), getFileSize(m_pFile));
        
        LONGS_EQUAL(sizeof(header), fread(&header, 1, sizeof(header), m_pFile));
        CHECK(0 == memcmp(header.signature, BINARY_BUFFER_SAV_SIGNATURE, sizeof(header.signature)));
        LONGS_EQUAL(expectedAddress, header.address);
        LONGS_EQUAL(expectedContentSize, header.length);
        
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
    
    void test6502RelativeBranchInstruction(const char* pInstruction, unsigned char opcode)
    {
        char               testString[64];
        char               expectedListOutput[128];

        sprintf(testString, " %s *+129\n", pInstruction);
        sprintf(expectedListOutput, "8000: %02X 7F        1 %s", opcode, testString);
        runAssembler(testString);
        validateSuccessfulOutput(expectedListOutput);
        
        m_isInvalidMode = TRUE;
        m_expectedOpcode = 0x00;
        m_isZeroPageTreatedAsAbsolute = FALSE;
        testImmediateAddressing(pInstruction);
        testImpliedAddressing(pInstruction);
        testZeroPageIndexedIndirectAddressing(pInstruction);
        testIndirectIndexedAddressing(pInstruction);
        testZeroPageIndexedXAddressing(pInstruction);
        testZeroPageIndexedYAddressing(pInstruction);
        testAbsoluteIndexedXAddressing(pInstruction);
        testAbsoluteIndexedYAddressing(pInstruction);
        testRelativeAddressing(pInstruction);
        testAbsoluteIndirectAddressing(pInstruction);
        testAbsoluteIndexedIndirectAddressing(pInstruction);
        testZeroPageIndirectAddressing(pInstruction);
    }
};


TEST(Assembler, FailAllInitAllocations)
{
    static const int allocationsToFail = 11;
    for (int i = 1 ; i <= allocationsToFail ; i++)
    {
        MallocFailureInject_FailAllocation(i);
        m_pAssembler = Assembler_CreateFromString(dupe(""));
        POINTERS_EQUAL(NULL, m_pAssembler);
        validateOutOfMemoryExceptionThrown();
    }

    MallocFailureInject_FailAllocation(allocationsToFail + 1);
    m_pAssembler = Assembler_CreateFromString(dupe(""));
    CHECK_TRUE(m_pAssembler != NULL);
}

TEST(Assembler, EmptyString)
{
    m_pAssembler = Assembler_CreateFromString(dupe(""));
    CHECK(m_pAssembler != NULL);
    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(0, printfSpy_GetCallCount());
}

TEST(Assembler, InitFromEmptyFile)
{
    createSourceFile("");
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    CHECK(m_pAssembler != NULL);
}

TEST(Assembler, InitFromNonExistantFile)
{
    m_pAssembler = Assembler_CreateFromFile("foo.noexist.bar");
    validateFileNotFoundExceptionThrown();
}

TEST(Assembler, FailFirstAllocationDuringFileInit)
{
    createSourceFile(" ORG $800\r\n");
    MallocFailureInject_FailAllocation(1);
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    validateOutOfMemoryExceptionThrown();
}

TEST(Assembler, FailSecondAllocationDuringFileInit)
{
    createSourceFile(" ORG $800\r\n");
    MallocFailureInject_FailAllocation(2);
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    validateOutOfMemoryExceptionThrown();
}

TEST(Assembler, FailThirdAllocationDuringFileInit)
{
    createSourceFile(" ORG $800\r\n");
    MallocFailureInject_FailAllocation(3);
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    validateOutOfMemoryExceptionThrown();
}

TEST(Assembler, FailFourthAllocationDuringFileInit)
{
    createSourceFile(" ORG $800\r\n");
    MallocFailureInject_FailAllocation(4);
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    validateOutOfMemoryExceptionThrown();
}

TEST(Assembler, FailFifthAllocationDuringFileInit)
{
    createSourceFile(" ORG $800\r\n");
    MallocFailureInject_FailAllocation(5);
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    validateOutOfMemoryExceptionThrown();
}

TEST(Assembler, FailSixthAllocationDuringFileInit)
{
    createSourceFile(" ORG $800\r\n");
    MallocFailureInject_FailAllocation(6);
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    validateOutOfMemoryExceptionThrown();
}

TEST(Assembler, InitAndRunFromShortFile)
{
    createSourceFile("* Symbols\n"
                     "SYM1 = $1\n"
                     "SYM2 EQU $2\n");
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    CHECK(m_pAssembler != NULL);
    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(3, printfSpy_GetCallCount());
}

TEST(Assembler, FailAllocationOnLongLine)
{
    char longLine[257];
    
    memset(longLine, ' ', sizeof(longLine));
    longLine[ARRAYSIZE(longLine)-1] = '\0';
    m_pAssembler = Assembler_CreateFromString(longLine);
    CHECK(m_pAssembler != NULL);

    MallocFailureInject_FailAllocation(1);
    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(0, printfSpy_GetCallCount());
    LONGS_EQUAL(outOfMemoryException, getExceptionCode());
    clearExceptionCode();
}

TEST(Assembler, FailAllocationOnLineInfoAllocation)
{
    m_pAssembler = Assembler_CreateFromString(dupe("* Comment Line."));
    CHECK(m_pAssembler != NULL);

    MallocFailureInject_FailAllocation(1);
    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(0, printfSpy_GetCallCount());
    LONGS_EQUAL(outOfMemoryException, getExceptionCode());
    clearExceptionCode();
}

TEST(Assembler, RunOnLongLine)
{
    char longLine[257];
    
    memset(longLine, ' ', sizeof(longLine));
    longLine[ARRAYSIZE(longLine)-1] = '\0';
    m_pAssembler = Assembler_CreateFromString(longLine);
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(1, printfSpy_GetCallCount());
    LONGS_EQUAL(0, Assembler_GetErrorCount(m_pAssembler) );
}

TEST(Assembler, LabelTooLong)
{
    char longLine[257];
    
    memset(longLine, 'a', sizeof(longLine));
    longLine[ARRAYSIZE(longLine)-1] = '\0';
    m_pAssembler = Assembler_CreateFromString(longLine);
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    CHECK(NULL != strstr(printfSpy_GetPreviousOutput(), " label is too long.") );
}

TEST(Assembler, LocalLabelTooLong)
{
    char longLine[255+1+2+1];
    
    memset(longLine, 'a', 255);
    strcpy(&longLine[sizeof(longLine) - 4], "\n:b");
    m_pAssembler = Assembler_CreateFromString(longLine);
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(3, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    CHECK(NULL != strstr(printfSpy_GetLastErrorOutput(), " label is too long.") );
}

TEST(Assembler, SpecifySameLabelTwice)
{
    m_pAssembler = Assembler_CreateFromString(dupe("entry lda #$60\n"
                                                   "entry lda #$61\n"));
    runAssemblerAndValidateFailure("filename:2: error: 'entry' symbol has already been defined.\n",
                                   "8002: A9 61        2 entry lda #$61\n", 3);
}

TEST(Assembler, LocalLabelDefineBeforeGlobalLabel)
{
    m_pAssembler = Assembler_CreateFromString(dupe(":local_label\n"));
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    STRCMP_EQUAL("filename:1: error: ':local_label' local label isn't allowed before first global label.\n", printfSpy_GetPreviousOutput());
}

TEST(Assembler, LocalLabelReferenceBeforeGlobalLabel)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" sta :local_label\n"));
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    STRCMP_EQUAL("filename:1: error: ':local_label' local label isn't allowed before first global label.\n", printfSpy_GetPreviousOutput());
}

TEST(Assembler, LabelStartsWithInvalidCharacter)
{
    m_pAssembler = Assembler_CreateFromString(dupe("9Label sta $23\n"));
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    STRCMP_EQUAL("filename:1: error: '9Label' label starts with invalid character.\n", printfSpy_GetPreviousOutput());
}

TEST(Assembler, LabelContainsInvalidCharacter)
{
    m_pAssembler = Assembler_CreateFromString(dupe("Label. sta $23\n"));
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    STRCMP_EQUAL("filename:1: error: 'Label.' label contains invalid character, '.'.\n", printfSpy_GetPreviousOutput());
}

TEST(Assembler, ForwardReferenceLabel)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " sta label\n"
                                                   "label sta $2b\n"));
    CHECK(m_pAssembler != NULL);
    runAssemblerAndValidateOutputIsTwoLinesOf("0800: 8D 03 08     2  sta label\n",
                                              "0803: 85 2B        3 label sta $2b\n", 3);
}

TEST(Assembler, FailLineInfoAllocationDuringForwardReferenceLabelFixup)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " sta label\n"
                                                   "label sta $2b\n"));
    CHECK(m_pAssembler != NULL);

    MallocFailureInject_FailAllocation(8);
    runAssemblerAndValidateFailure("filename:3: error: Failed to allocate space for updating forward references to 'label' symbol.\n",
                                   "0803: 85 2B        3 label sta $2b\n", 4);
    MallocFailureInject_Restore();
}

TEST(Assembler, FailToDefineLabelTwiceAfterForwardReferenced)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " sta label\n"
                                                   "label sta $2b\n"
                                                   "label sta $2c\n"));
    CHECK(m_pAssembler != NULL);
    runAssemblerAndValidateFailure("filename:4: error: 'label' symbol has already been defined.\n",
                                   "0805: 85 2C        4 label sta $2c\n", 5);
}

TEST(Assembler, LocalLabelPlusOffsetBackwardReference)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0000\n"
                                                   "func1 sta $20\n"
                                                   ":local sta $20\n"
                                                   "func2 sta $21\n"
                                                   ":local sta $22\n"
                                                   " sta :local+1\n"));
    runAssemblerAndValidateLastLineIs("0008: 85 07        6  sta :local+1\n", 6);
}

TEST(Assembler, LocalLabelPlusOffsetForwardReference)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   "func1 sta $20\n"
                                                   ":local sta $20\n"
                                                   "func2 sta $21\n"
                                                   " sta :local+1\n"
                                                   ":local sta $22\n"));
    
    runAssemblerAndValidateOutputIsTwoLinesOf("0806: 8D 0A 08     5  sta :local+1\n",
                                              "0809: 85 22        6 :local sta $22\n", 6);
}

TEST(Assembler, GlobalLabelPlusOffsetForwardReference)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " sta globalLabel+1\n"
                                                   "globalLabel sta $22\n"));
    
    runAssemblerAndValidateOutputIsTwoLinesOf("0800: 8D 04 08     2  sta globalLabel+1\n",
                                              "0803: 85 22        3 globalLabel sta $22\n", 3);
}

TEST(Assembler, CascadedForwardReferenceOfEQUToLabel)
{
    LineInfo* pSecondLine;
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " sta 1+equLabel\n"
                                                   "equLabel equ lineLabel\n"
                                                   "lineLabel sta $22\n"));
    Assembler_Run(m_pAssembler);
    pSecondLine = m_pAssembler->linesHead.pNext->pNext;
    LONGS_EQUAL(3, pSecondLine->machineCodeSize);
    CHECK(0 == memcmp(pSecondLine->pMachineCode, "\x8d\x04\x08", 3));
}

TEST(Assembler, MultipleForwardReferencesToSameLabel)
{
    LineInfo* pSecondLine;
    LineInfo* pThirdLine;
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " sta 1+label\n"
                                                   " sta label+1\n"
                                                   "label sta $22\n"));
    Assembler_Run(m_pAssembler);
    pSecondLine = m_pAssembler->linesHead.pNext->pNext;
    LONGS_EQUAL(3, pSecondLine->machineCodeSize);
    CHECK(0 == memcmp(pSecondLine->pMachineCode, "\x8d\x07\x08", 3));
    pThirdLine = pSecondLine->pNext;
    LONGS_EQUAL(3, pSecondLine->machineCodeSize);
    CHECK(0 == memcmp(pSecondLine->pMachineCode, "\x8d\x07\x08", 3));
}

TEST(Assembler, FailZeroPageForwardReference)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0000\n"
                                                   " sta globalLabel\n"
                                                   "globalLabel sta $22\n"));
    
    runAssemblerAndValidateFailure("filename:2: error: Couldn't properly infer size of a forward reference in 'globalLabel' operand.\n",
                                   "0003: 85 22        3 globalLabel sta $22\n", 4);
}

TEST(Assembler, ReferenceNonExistantLabel)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" sta badLabel\n"));
    runAssemblerAndValidateFailure("filename:1: error: The 'badLabel' label is undefined.\n",
                                   "8000: 8D 00 00     1  sta badLabel\n", 2);
}

TEST(Assembler, EQULabelStartsWithInvalidCharacter)
{
    m_pAssembler = Assembler_CreateFromString(dupe("9Label EQU $23\n"));
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    STRCMP_EQUAL("filename:1: error: '9Label' label starts with invalid character.\n", printfSpy_GetPreviousOutput());
}

TEST(Assembler, EQULabelContainsInvalidCharacter)
{
    m_pAssembler = Assembler_CreateFromString(dupe("Label. EQU $23\n"));
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    STRCMP_EQUAL("filename:1: error: 'Label.' label contains invalid character, '.'.\n", printfSpy_GetPreviousOutput());
}

TEST(Assembler, EQULabelIsLocal)
{
    m_pAssembler = Assembler_CreateFromString(dupe(":Label EQU $23\n"));
    CHECK(m_pAssembler != NULL);

    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(2, printfSpy_GetCallCount());
    LONGS_EQUAL(1, Assembler_GetErrorCount(m_pAssembler));
    STRCMP_EQUAL("filename:1: error: ':Label' can't be a local label when used with EQU.\n", printfSpy_GetPreviousOutput());
}

TEST(Assembler, ForwardReferenceEQULabel)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" sta label\n"
                                                   "label equ $ffff\n"));
    CHECK(m_pAssembler != NULL);
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 8D FF FF     1  sta label\n",
                                              "    :    =FFFF     2 label equ $ffff\n");
}

TEST(Assembler, FailToDefineEquLabelTwiceAfterForwardReferenced)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" sta label\n"
                                                   "label equ $ff2b\n"
                                                   "label equ $ff2c\n"));
    CHECK(m_pAssembler != NULL);
    runAssemblerAndValidateFailure("filename:3: error: 'label' symbol has already been defined.\n",
                                   "    :              3 label equ $ff2c\n", 4);
}

TEST(Assembler, CommentLine)
{
    m_pAssembler = Assembler_CreateFromString(dupe("*  boot\n"));
    runAssemblerAndValidateOutputIs("    :              1 *  boot\n");
}

TEST(Assembler, InvalidOperatorFromStringSource)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" foo bar\n"));
    runAssemblerAndValidateFailure("filename:1: error: 'foo' is not a recognized mnemonic or macro.\n", 
                                   "    :              1  foo bar\n");
}

TEST(Assembler, InvalidOperatorFromFileSource)
{
    createSourceFile(" foo bar\n");
    m_pAssembler = Assembler_CreateFromFile(g_sourceFilename);
    runAssemblerAndValidateFailure("AssemblerTest.S:1: error: 'foo' is not a recognized mnemonic or macro.\n", 
                                   "    :              1  foo bar\n");
}

TEST(Assembler, EqualSignDirective)
{
    m_pAssembler = Assembler_CreateFromString(dupe("org = $800\n"));
    runAssemblerAndValidateOutputIs("    :    =0800     1 org = $800\n");
}

TEST(Assembler, EQUDirective)
{
    m_pAssembler = Assembler_CreateFromString(dupe("org EQU $800\n"));
    runAssemblerAndValidateOutputIs("    :    =0800     1 org EQU $800\n");
}

TEST(Assembler, MultipleDefinedSymbolFailure)
{
    m_pAssembler = Assembler_CreateFromString(dupe("org = $800\n"
                                                   "org EQU $900\n"));
    runAssemblerAndValidateFailure("filename:2: error: 'org' symbol has already been defined.\n", 
                                   "    :              2 org EQU $900\n", 3);
}

TEST(Assembler, FailAllocationDuringSymbolCreation)
{
    m_pAssembler = Assembler_CreateFromString(dupe("org = $800\n"));
    MallocFailureInject_FailAllocation(2);
    runAssemblerAndValidateFailure("filename:1: error: Failed to allocate space for 'org' symbol.\n", 
                                   "    :              1 org = $800\n");
}

TEST(Assembler, InvalidExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe("org EQU (800\n"));
    runAssemblerAndValidateFailure("filename:1: error: Unexpected prefix in '(800' expression.\n", 
                                   "    :              1 org EQU (800\n");
}

TEST(Assembler, IgnoreLSTDirective)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" lst off\n"));
    runAssemblerAndValidateOutputIs("    :              1  lst off\n");
}

TEST(Assembler, HEXDirectiveWithSingleValue)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex 01\n"));
    runAssemblerAndValidateOutputIs("8000: 01           1  hex 01\n");
}

TEST(Assembler, HEXDirectiveWithMixedCase)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex cD,Cd\n"));
    runAssemblerAndValidateOutputIs("8000: CD CD        1  hex cD,Cd\n");
}

TEST(Assembler, HEXDirectiveWithThreeValuesAndCommas)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex 0e,0c,0a\n"));
    runAssemblerAndValidateOutputIs("8000: 0E 0C 0A     1  hex 0e,0c,0a\n");
}

TEST(Assembler, HEXDirectiveWithThreeValues)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex 0e0c0a\n"));
    runAssemblerAndValidateOutputIs("8000: 0E 0C 0A     1  hex 0e0c0a\n");
}

TEST(Assembler, HEXDirectiveWithFourValues)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex 01,02,03,04\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 01 02 03     1  hex 01,02,03,04\n",
                                              "8003: 04      \n");
}

TEST(Assembler, HEXDirectiveWithSixValues)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex 01,02,03,04,05,06\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 01 02 03     1  hex 01,02,03,04,05,06\n",
                                              "8003: 04 05 06\n");
}

TEST(Assembler, HEXDirectiveWithMaximumOf32Value)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex 0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20\n"));
    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(0, Assembler_GetErrorCount(m_pAssembler) );
    LONGS_EQUAL(11, printfSpy_GetCallCount());
    STRCMP_EQUAL("801B: 1C 1D 1E\n", printfSpy_GetPreviousOutput());
    STRCMP_EQUAL("801E: 1F 20   \n", printfSpy_GetLastOutput());
}

TEST(Assembler, HEXDirectiveWith33Values_1MoreThanSupported)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex 0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f2021\n"));
    runAssemblerAndValidateFailure("filename:1: error: '0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f2021' contains more than 32 values.\n", 
                                   "801E: 1F 20   \n", 12);
}

TEST(Assembler, HEXDirectiveOnTwoLines)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex 01\n"
                                                   " hex 02\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 01           1  hex 01\n",
                                              "8001: 02           2  hex 02\n");
}

TEST(Assembler, HEXDirectiveWithUpperCaseHex)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex FA\n"));
    runAssemblerAndValidateOutputIs("8000: FA           1  hex FA\n");
}

TEST(Assembler, HEXDirectiveWithLowerCaseHex)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex fa\n"));
    runAssemblerAndValidateOutputIs("8000: FA           1  hex fa\n");
}

TEST(Assembler, HEXDirectiveWithOddDigitCount)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex fa0\n"));
    runAssemblerAndValidateFailure("filename:1: error: 'fa0' doesn't contain an even number of hex digits.\n",
                                   "    :              1  hex fa0\n");
}

TEST(Assembler, HEXDirectiveWithInvalidDigit)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex fg\n"));
    runAssemblerAndValidateFailure("filename:1: error: 'fg' contains an invalid hex digit.\n",
                                   "    :              1  hex fg\n");
}

TEST(Assembler, FailBinaryBufferAllocationInHEXDirective)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" hex ff\n"));
    BinaryBuffer_FailAllocation(m_pAssembler->pCurrentBuffer, 1);
    runAssemblerAndValidateFailure("filename:1: error: Exceeded the 65536 allowed bytes in the object file.\n",
                                   "    :              1  hex ff\n");
}

TEST(Assembler, ORGDirectiveWithLiteralValue)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $900\n"
                                                   " hex 01\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("    :              1  org $900\n", 
                                              "0900: 01           2  hex 01\n");
}

TEST(Assembler, ORGDirectiveWithInvalidExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org +900\n"));
    runAssemblerAndValidateFailure("filename:1: error: Unexpected prefix in '+900' expression.\n",
                                   "    :              1  org +900\n");
}

TEST(Assembler, ORGDirectiveWithSymbolValue)
{
    m_pAssembler = Assembler_CreateFromString(dupe("org = $800\n"
                                                   " org org\n"
                                                   " hex 01\n"));
    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(3, printfSpy_GetCallCount());
    STRCMP_EQUAL("    :              2  org org\n", printfSpy_GetPreviousOutput());
    STRCMP_EQUAL("0800: 01           3  hex 01\n", printfSpy_GetLastOutput());
    LONGS_EQUAL(0, Assembler_GetErrorCount(m_pAssembler) );
}

TEST(Assembler, ORGDirectiveWithInvalidImmediate)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org #$00\n"));
    runAssemblerAndValidateFailure("filename:1: error: '#$00' doesn't specify an absolute address.\n",
                                   "    :              1  org #$00\n");
}

TEST(Assembler, DUMandDEND_Directive)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " dum $00\n"
                                                   " hex ff\n"
                                                   " dend\n"
                                                   " hex fe\n"));
    Assembler_Run(m_pAssembler);
    
    LineInfo* pThirdLine = m_pAssembler->linesHead.pNext->pNext->pNext;
    LineInfo* pFifthLine = pThirdLine->pNext->pNext;
    
    validateLineInfo(pThirdLine, 0x0000, 1, "\xff");
    validateLineInfo(pFifthLine, 0x0800, 1, "\xfe");
}

TEST(Assembler, TwoDUMandDEND_Directive)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " dum $00\n"
                                                   " hex ff\n"
                                                   " dum $100\n"
                                                   " hex fe\n"
                                                   " dend\n"
                                                   " hex fd\n"));
    Assembler_Run(m_pAssembler);
    
    LineInfo* pThirdLine = m_pAssembler->linesHead.pNext->pNext->pNext;
    LineInfo* pFifthLine = pThirdLine->pNext->pNext;
    LineInfo* pSeventhLine = pFifthLine->pNext->pNext;
    
    validateLineInfo(pThirdLine, 0x0000, 1, "\xff");
    validateLineInfo(pFifthLine, 0x0100, 1, "\xfe");
    validateLineInfo(pSeventhLine, 0x0800, 1, "\xfd");
}

TEST(Assembler, DUM_ORG_andDEND_Directive)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " dum $00\n"
                                                   " hex ff\n"
                                                   " org $100\n"
                                                   " hex fe\n"
                                                   " dend\n"
                                                   " hex fd\n"));
    Assembler_Run(m_pAssembler);
    
    LineInfo* pThirdLine = m_pAssembler->linesHead.pNext->pNext->pNext;
    LineInfo* pFifthLine = pThirdLine->pNext->pNext;
    LineInfo* pSeventhLine = pFifthLine->pNext->pNext;
    
    validateLineInfo(pThirdLine, 0x0000, 1, "\xff");
    validateLineInfo(pFifthLine, 0x0100, 1, "\xfe");
    validateLineInfo(pSeventhLine, 0x0800, 1, "\xfd");
}

TEST(Assembler, DUMDirectiveWithInvalidImmediateExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" dum #$00\n"));
    runAssemblerAndValidateFailure("filename:1: error: '#$00' doesn't specify an absolute address.\n",
                                   "    :              1  dum #$00\n");
}

TEST(Assembler, DEND_DirectiveWithoutDUM)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" dend\n"));
    runAssemblerAndValidateFailure("filename:1: error: dend isn't allowed without a preceding DUM directive.\n",
                                   "    :              1  dend\n");
}

TEST(Assembler, DS_DirectiveWithSmallRepeatValue)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" ds 1\n"
                                                   " hex ff\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 00           1  ds 1\n", 
                                              "8001: FF           2  hex ff\n");
}

TEST(Assembler, DS_DirectiveWithRepeatValueGreaterThan32)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" ds 42\n"
                                                   " hex ff\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8027: 00 00 00\n", 
                                              "802A: FF           2  hex ff\n", 15);
}

TEST(Assembler, DS_DirectiveWithBackSlashWhenAlreadyOnPageBoundary)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" ds \\\n"));
    runAssemblerAndValidateOutputIs("    :              1  ds \\\n");
}

TEST(Assembler, DS_DirectiveWithBackSlashWhenOneByteFromPageBoundary)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" ds 255\n"
                                                   " ds \\,$ff\n"));
    runAssemblerAndValidateLastLineIs("80FF: FF           2  ds \\,$ff\n", 86);
}

TEST(Assembler, DS_DirectiveWithSecondExpressionToSpecifyFillValue)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" ds 2,$ff\n"));
    runAssemblerAndValidateOutputIs("8000: FF FF        1  ds 2,$ff\n");
}

TEST(Assembler, DS_DirectiveWithInvalidExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" ds ($800\n"));
    runAssemblerAndValidateFailure("filename:1: error: Unexpected prefix in '($800' expression.\n",
                                   "    :              1  ds ($800\n");
}

TEST(Assembler, DS_DirectiveWithForwardReference)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" ds \\,Fill\n"
                                                   "Fill equ $ff\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("    :              1  ds \\,Fill\n",
                                              "    :    =00FF     2 Fill equ $ff\n");
}

TEST(Assembler, FailBinaryBufferAllocationInDSDirective)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" ds 1\n"));
    BinaryBuffer_FailAllocation(m_pAssembler->pCurrentBuffer, 1);
    runAssemblerAndValidateFailure("filename:1: error: Exceeded the 65536 allowed bytes in the object file.\n",
                                   "    :              1  ds 1\n");
}

TEST(Assembler, ASC_DirectiveInDoubleQuotes)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" asc \"Tst\"\n"));
    runAssemblerAndValidateOutputIs("8000: D4 F3 F4     1  asc \"Tst\"\n");
}

TEST(Assembler, ASC_DirectiveInSingleQuotes)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" asc 'Tst'\n"));
    runAssemblerAndValidateOutputIs("8000: 54 73 74     1  asc 'Tst'\n");
}

TEST(Assembler, ASC_DirectiveWithNoSpacesBetweenQuotes)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" asc 'a b'\n"));
    runAssemblerAndValidateOutputIs("8000: 61 20 62     1  asc 'a b'\n");
}

TEST(Assembler, ASC_DirectiveWithNoEndingDelimiter)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" asc 'Tst\n"));
    runAssemblerAndValidateFailure("filename:1: error: 'Tst didn't end with the expected ' delimiter.\n",
                                   "8000: 54 73 74     1  asc 'Tst\n");
}

TEST(Assembler, SAV_DirectiveOnEmptyObjectFile)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" sav AssemblerTest.sav\n"));
    CHECK(m_pAssembler != NULL);
    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(0, Assembler_GetErrorCount(m_pAssembler));
    validateObjectFileContains(0x8000, "", 0);
}

TEST(Assembler, SAV_DirectiveOnSmallObjectFile)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " hex 00,ff\n"
                                                   " sav AssemblerTest.sav\n"));
    CHECK(m_pAssembler != NULL);
    Assembler_Run(m_pAssembler);
    LONGS_EQUAL(0, Assembler_GetErrorCount(m_pAssembler));
    validateObjectFileContains(0x800, "\x00\xff", 2);
}

TEST(Assembler, DB_DirectiveWithSingleExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe("Value EQU $fe\n"
                                                   " db Value+1\n"));
    runAssemblerAndValidateLastLineIs("8000: FF           2  db Value+1\n", 2);
}

TEST(Assembler, DB_DirectiveWithThreeExpressions)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" db 2,0,1\n"));
    runAssemblerAndValidateOutputIs("8000: 02 00 01     1  db 2,0,1\n");
}

TEST(Assembler, DB_DirectiveWithImmediateExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" db #$ff\n"));
    runAssemblerAndValidateOutputIs("8000: FF           1  db #$ff\n");
}

TEST(Assembler, DB_DirectiveWithForwardReference)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" db Label\n"
                                                   "Label db $12\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 01           1  db Label\n",
                                              "8001: 12           2 Label db $12\n");
}

TEST(Assembler, DB_DirectiveWithInvalidExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" db ($800\n"));
    runAssemblerAndValidateFailure("filename:1: error: Unexpected prefix in '($800' expression.\n",
                                   "    :              1  db ($800\n");
}

TEST(Assembler, DFB_DirectiveSameAsDB)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" dfb 2,0,1\n"));
    runAssemblerAndValidateOutputIs("8000: 02 00 01     1  dfb 2,0,1\n");
}

TEST(Assembler, TR_DirectiveIsIgnored)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" tr on\n"));
    runAssemblerAndValidateOutputIs("    :              1  tr on\n");
}

TEST(Assembler, DA_DirectiveWithOneExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" da $ff+1\n"));
    runAssemblerAndValidateOutputIs("8000: 00 01        1  da $ff+1\n");
}

TEST(Assembler, DA_DirectiveWithThreeExpressions)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" da $ff+1,$ff,$1233+1\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 00 01 FF     1  da $ff+1,$ff,$1233+1\n",
                                              "8003: 00 34 12\n");
}

TEST(Assembler, DA_DirectiveWithForwardReference)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" da Label\n"
                                                   "Label da $1234\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 02 80        1  da Label\n",
                                              "8002: 34 12        2 Label da $1234\n");
}

TEST(Assembler, DA_DirectiveWithInvalidExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" da ($800\n"));
    runAssemblerAndValidateFailure("filename:1: error: Unexpected prefix in '($800' expression.\n",
                                   "    :              1  da ($800\n");
}

TEST(Assembler, DW_DirectiveSameAsDA)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" dw $ff+1,$ff,$1233+1\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("8000: 00 01 FF     1  dw $ff+1,$ff,$1233+1\n",
                                              "8003: 00 34 12\n");
}

TEST(Assembler, VerifyObjectFileWithForwardReferenceLabel)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   " sta label\n"
                                                   "label sta $2b\n"
                                                   " sav AssemblerTest.sav\n"));
    CHECK(m_pAssembler != NULL);
    Assembler_Run(m_pAssembler);
    validateObjectFileContains(0x800, "\x8d\x03\x08\x85\x2b", 5);
}

TEST(Assembler, FailBinaryBufferAllocationInASCDirective)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" asc 'Tst'\n"));
    BinaryBuffer_FailAllocation(m_pAssembler->pCurrentBuffer, 1);
    runAssemblerAndValidateFailure("filename:1: error: Exceeded the 65536 allowed bytes in the object file.\n",
                                   "    :              1  asc 'Tst'\n");
}

TEST(Assembler, FailWithInvalidImmediateValue)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" lda #$100\n"));
    runAssemblerAndValidateFailure("filename:1: error: Immediate expression '$100' doesn't fit in 8-bits.\n",
                                   "    :              1  lda #$100\n");
}

TEST(Assembler, FailWithInvalidExpression)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" sta +ff\n"));
    runAssemblerAndValidateFailure("filename:1: error: Unexpected prefix in '+ff' expression.\n",
                                   "    :              1  sta +ff\n");
}

TEST(Assembler, FailBinaryBufferAllocationOnEmitSingleByteInstruction)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" clc\n"));
    BinaryBuffer_FailAllocation(m_pAssembler->pCurrentBuffer, 1);
    runAssemblerAndValidateFailure("filename:1: error: Exceeded the 65536 allowed bytes in the object file.\n",
                                   "    :              1  clc\n");
}

TEST(Assembler, FailBinaryBufferAllocationOnEmitTwoByteInstruction)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" lda #1\n"));
    BinaryBuffer_FailAllocation(m_pAssembler->pCurrentBuffer, 1);
    runAssemblerAndValidateFailure("filename:1: error: Exceeded the 65536 allowed bytes in the object file.\n",
                                   "    :              1  lda #1\n");
}

TEST(Assembler, FailBinaryBufferAllocationOnEmitThreeByteInstruction)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" lda $800\n"));
    BinaryBuffer_FailAllocation(m_pAssembler->pCurrentBuffer, 1);
    runAssemblerAndValidateFailure("filename:1: error: Exceeded the 65536 allowed bytes in the object file.\n",
                                   "    :              1  lda $800\n");
}

TEST(Assembler, FailWriteFileQueueDuringSAVDirective)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" sav AssemblerTest.sav\n"));
    CHECK(m_pAssembler != NULL);
    MallocFailureInject_FailAllocation(2);
    runAssemblerAndValidateFailure("filename:1: error: Failed to queue up save to 'AssemblerTest.sav'.\n",
                                   "    :              1  sav AssemblerTest.sav\n");
}

TEST(Assembler, STAAbsoluteViaLabel)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $800\n"
                                                   "entry lda #$60\n"
                                                   " sta entry\n"));
    runAssemblerAndValidateLastLineIs("0802: 8D 00 08     3  sta entry\n", 3);
}

TEST(Assembler, STAZeroPageAbsoluteViaLabel)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0000\n"
                                                   "entry lda #$60\n"
                                                   " sta entry\n"));
    runAssemblerAndValidateLastLineIs("0002: 85 00        3  sta entry\n", 3);
}



TEST(Assembler, BEQ_ZeroPageMaxNegativeTarget)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0090\n"
                                                   " beq *-126\n"));
    runAssemblerAndValidateLastLineIs("0090: F0 80        2  beq *-126\n", 2);
}

TEST(Assembler, BEQ_ZeroPageMaxPositiveTarget)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0000\n"
                                                   " beq *+129\n"));
    runAssemblerAndValidateLastLineIs("0000: F0 7F        2  beq *+129\n", 2);
}

TEST(Assembler, BEQ_ZeroPageInvalidNegativeTarget)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0090\n"
                                                   " beq *-127\n"));
    runAssemblerAndValidateFailure("filename:2: error: Relative offset of '*-127' exceeds the allowed -128 to 127 range.\n",
                                   "    :              2  beq *-127\n", 3);
}

TEST(Assembler, BEQ_ZeroPageInvalidPositiveTarget)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0000\n"
                                                   " beq *+130\n"));
    runAssemblerAndValidateFailure("filename:2: error: Relative offset of '*+130' exceeds the allowed -128 to 127 range.\n",
                                   "    :              2  beq *+130\n", 3);
}

TEST(Assembler, BEQ_AbsoluteTarget)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0800\n"
                                                   " beq *+2\n"));
    runAssemblerAndValidateLastLineIs("0800: F0 00        2  beq *+2\n", 2);
}

TEST(Assembler, BEQ_ForwardLabelReference)
{
    m_pAssembler = Assembler_CreateFromString(dupe(" org $0800\n"
                                                   " beq label\n"
                                                   "label\n"));
    runAssemblerAndValidateOutputIsTwoLinesOf("0800: F0 00        2  beq label\n",
                                              "    :              3 label\n", 3);
}



TEST(Assembler, ADC_TableDrivenTest)
{
    test6502Instruction("adc", "69,6D,65,XX,61,71,75,^79,7D,79,XX,XX,XX,72");
}

TEST(Assembler, AND_TableDrivenTest)
{
    test6502Instruction("and", "29,2D,25,XX,21,31,35,^39,3D,39,XX,XX,XX,32");
}

TEST(Assembler, ASL_TableDrivenTest)
{
    test6502Instruction("asl", "XX,0E,06,0A,XX,XX,16,XX,1E,XX,XX,XX,XX,XX");
}

TEST(Assembler, BCC_AllAddressingMode)
{
    test6502RelativeBranchInstruction("bcc", 0x90);
}

TEST(Assembler, BCS_AllAddressingMode)
{
    test6502RelativeBranchInstruction("bcs", 0xB0);
}

TEST(Assembler, BEQ_AllAddressingModes)
{
    test6502RelativeBranchInstruction("beq", 0xF0);
}

TEST(Assembler, BIT_TableDrivenTest)
{
    test6502Instruction("bit", "89,2C,24,XX,XX,XX,34,XX,3C,XX,XX,XX,XX,XX");
}

TEST(Assembler, BMI_AllAddressingMode)
{
    test6502RelativeBranchInstruction("bmi", 0x30);
}

TEST(Assembler, BNE_AllAddressingMode)
{
    test6502RelativeBranchInstruction("bne", 0xD0);
}

TEST(Assembler, BPL_AllAddressingMode)
{
    test6502RelativeBranchInstruction("bpl", 0x10);
}

TEST(Assembler, BRA_AllAddressingMode)
{
    test6502RelativeBranchInstruction("bra", 0x80);
}

TEST(Assembler, BRK_TableDrivenTest)
{
    test6502Instruction("brk", "XX,XX,XX,00,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, BVC_AllAddressingMode)
{
    test6502RelativeBranchInstruction("bvc", 0x50);
}

TEST(Assembler, BVS_AllAddressingMode)
{
    test6502RelativeBranchInstruction("bvs", 0x70);
}

TEST(Assembler, CLC_TableDrivenTest)
{
    test6502Instruction("clc", "XX,XX,XX,18,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, CLD_TableDrivenTest)
{
    test6502Instruction("cld", "XX,XX,XX,D8,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, CLI_TableDrivenTest)
{
    test6502Instruction("cli", "XX,XX,XX,58,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, CLV_TableDrivenTest)
{
    test6502Instruction("clv", "XX,XX,XX,B8,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, CMP_TableDrivenTest)
{
    test6502Instruction("cmp", "C9,CD,C5,XX,C1,D1,D5,^D9,DD,D9,XX,XX,XX,D2");
}

TEST(Assembler, CPX_TableDrivenTest)
{
    test6502Instruction("cpx", "E0,EC,E4,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, CPY_TableDrivenTest)
{
    test6502Instruction("cpy", "C0,CC,C4,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, DEA_TableDrivenTest)
{
    test6502Instruction("dea", "XX,XX,XX,3A,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, DEC_TableDrivenTest)
{
    test6502Instruction("dec", "XX,CE,C6,XX,XX,XX,D6,XX,DE,XX,XX,XX,XX,XX");
}

TEST(Assembler, DEX_TableDrivenTest)
{
    test6502Instruction("dex", "XX,XX,XX,CA,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, DEY_TableDrivenTest)
{
    test6502Instruction("dey", "XX,XX,XX,88,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, EOR_TableDrivenTest)
{
    test6502Instruction("eor", "49,4D,45,XX,41,51,55,^59,5D,59,XX,XX,XX,52");
}

TEST(Assembler, INA_TableDrivenTest)
{
    test6502Instruction("ina", "XX,XX,XX,1A,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, INC_TableDrivenTest)
{
    test6502Instruction("inc", "XX,EE,E6,XX,XX,XX,F6,XX,FE,XX,XX,XX,XX,XX");
}

TEST(Assembler, INX_TableDrivenTest)
{
    test6502Instruction("inx", "XX,XX,XX,E8,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, INY_TableDrivenTest)
{
    test6502Instruction("iny", "XX,XX,XX,C8,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, JMP_TableDrivenTest)
{
    test6502Instruction("jmp", "XX,4C,^4C,XX,^7C,XX,XX,XX,XX,XX,XX,6C,7C,^6C");
}

TEST(Assembler, JSR_TableDrivenTest)
{
    test6502Instruction("jsr", "XX,20,^20,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, LDA_TableDrivenTest)
{
    test6502Instruction("lda", "A9,AD,A5,XX,A1,B1,B5,^B9,BD,B9,XX,XX,XX,B2");
}

TEST(Assembler, LDX_TableDrivenTest)
{
    test6502Instruction("ldx", "A2,AE,A6,XX,XX,XX,XX,B6,XX,BE,XX,XX,XX,XX");
}

TEST(Assembler, LDY_TableDrivenTest)
{
    test6502Instruction("ldy", "A0,AC,A4,XX,XX,XX,B4,XX,BC,XX,XX,XX,XX,XX");
}

TEST(Assembler, LSR_TableDrivenTest)
{
    test6502Instruction("lsr", "XX,4E,46,4A,XX,XX,56,XX,5E,XX,XX,XX,XX,XX");
}

TEST(Assembler, NOP_TableDrivenTest)
{
    test6502Instruction("nop", "XX,XX,XX,EA,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, ORA_TableDrivenTest)
{
    test6502Instruction("ora", "09,0D,05,XX,01,11,15,^19,1D,19,XX,XX,XX,12");
}

TEST(Assembler, PHA_TableDrivenTest)
{
    test6502Instruction("pha", "XX,XX,XX,48,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, PHP_TableDrivenTest)
{
    test6502Instruction("php", "XX,XX,XX,08,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, PHX_TableDrivenTest)
{
    test6502Instruction("phx", "XX,XX,XX,DA,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, PHY_TableDrivenTest)
{
    test6502Instruction("phy", "XX,XX,XX,5A,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, PLA_TableDrivenTest)
{
    test6502Instruction("pla", "XX,XX,XX,68,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, PLP_TableDrivenTest)
{
    test6502Instruction("plp", "XX,XX,XX,28,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, PLX_TableDrivenTest)
{
    test6502Instruction("plx", "XX,XX,XX,FA,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, PLY_TableDrivenTest)
{
    test6502Instruction("ply", "XX,XX,XX,7A,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, ROL_TableDrivenTest)
{
    test6502Instruction("rol", "XX,2E,26,2A,XX,XX,36,XX,3E,XX,XX,XX,XX,XX");
}

TEST(Assembler, ROR_TableDrivenTest)
{
    test6502Instruction("ror", "XX,6E,66,6A,XX,XX,76,XX,7E,XX,XX,XX,XX,XX");
}

TEST(Assembler, RTI_TableDrivenTest)
{
    test6502Instruction("rti", "XX,XX,XX,40,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, RTS_TableDrivenTest)
{
    test6502Instruction("rts", "XX,XX,XX,60,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, SBC_TableDrivenTest)
{
    test6502Instruction("sbc", "E9,ED,E5,XX,E1,F1,F5,^F9,FD,F9,XX,XX,XX,F2");
}

TEST(Assembler, SEC_TableDrivenTest)
{
    test6502Instruction("sec", "XX,XX,XX,38,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, SED_TableDrivenTest)
{
    test6502Instruction("sed", "XX,XX,XX,F8,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, SEI_TableDrivenTest)
{
    test6502Instruction("sei", "XX,XX,XX,78,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, STA_TableDrivenTest)
{
    test6502Instruction("sta", "XX,8D,85,XX,81,91,95,^99,9D,99,XX,XX,XX,92");
}

TEST(Assembler, STX_TableDrivenTest)
{
    test6502Instruction("stx", "XX,8E,86,XX,XX,XX,XX,96,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, STY_TableDrivenTest)
{
    test6502Instruction("sty", "XX,8C,84,XX,XX,XX,94,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, STZ_TableDrivenTest)
{
    test6502Instruction("stz", "XX,9C,64,XX,XX,XX,74,XX,9E,XX,XX,XX,XX,XX");
}

TEST(Assembler, TAX_TableDrivenTest)
{
    test6502Instruction("tax", "XX,XX,XX,AA,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, TAY_TableDrivenTest)
{
    test6502Instruction("tay", "XX,XX,XX,A8,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, TRB_TableDrivenTest)
{
    test6502Instruction("trb", "XX,1C,14,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, TSB_TableDrivenTest)
{
    test6502Instruction("tsb", "XX,0C,04,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, TSX_TableDrivenTest)
{
    test6502Instruction("tsx", "XX,XX,XX,BA,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, TXA_TableDrivenTest)
{
    test6502Instruction("txa", "XX,XX,XX,8A,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, TXS_TableDrivenTest)
{
    test6502Instruction("txs", "XX,XX,XX,9A,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}

TEST(Assembler, TYA_TableDrivenTest)
{
    test6502Instruction("tya", "XX,XX,XX,98,XX,XX,XX,XX,XX,XX,XX,XX,XX,XX");
}
