/*  Copyright (C) 2013  Adam Green (https://github.com/adamgreen)
    Copyright (C) 2013  Tee-Kiah Chia

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#ifndef _ASSEMBLER_PRIV_H_
#define _ASSEMBLER_PRIV_H_

#include <stdio.h>
#include "Assembler.h"
#include "AssemblerTest.h"
#include "TextFile.h"
#include "TextSource.h"
#include "SymbolTable.h"
#include "ParseLine.h"
#include "ListFile.h"
#include "SizedString.h"
#include "BinaryBuffer.h"
#include "ParseCSV.h"
#include "util.h"


#define NUMBER_OF_SYMBOL_TABLE_HASH_BUCKETS 511
#define SIZE_OF_OBJECT_AND_DUMMY_BUFFERS    (64 * 1024)

/* Bits in the Conditional::flags field. */
#define CONDITIONAL_SKIP_SOURCE           1
#define CONDITIONAL_INHERITED_SKIP_SOURCE 2
#define CONDITIONAL_SEEN_ELSE             4
#define CONDITIONAL_SKIP_STATES_MASK      (CONDITIONAL_SKIP_SOURCE | CONDITIONAL_INHERITED_SKIP_SOURCE)

typedef struct OpCodeEntry
{
    const char* pOperator;
    void (*directiveHandler)(Assembler *pThis);
    unsigned char opcodeImmediate;
    unsigned char opcodeAbsolute;
    unsigned char opcodeZeroPage;
    unsigned char opcodeImplied;
    unsigned char opcodeZeroPageIndexedIndirect;
    unsigned char opcodeIndirectIndexed;
    unsigned char opcodeZeroPageIndexedX;
    unsigned char opcodeZeroPageIndexedY;
    unsigned char opcodeAbsoluteIndexedX;
    unsigned char opcodeAbsoluteIndexedY;
    unsigned char opcodeRelative;
    unsigned char opcodeAbsoluteIndirect;
    unsigned char opcodeAbsoluteIndexedIndirect;
    unsigned char opcodeZeroPageIndirect;
    unsigned char longImmediateIfLongA : 1,
                  longImmediateIfLongXY : 1;
} OpCodeEntry;


typedef struct Conditional
{
    struct Conditional* pPrev;
    LineInfo*           pLineInfo;
    unsigned int        flags;
} Conditional;

typedef struct MacroDefinition
{
    SizedString         macroName;
    unsigned int        startingSourceLine;
    SizedString*        macroExpansionLines;
    unsigned short      numberOfLines;
    struct MacroDefinition* pNext;
} MacroDefinition;

struct Assembler
{
    TextSource*                pTextSourceStack;
    SymbolTable*               pSymbols;
    const AssemblerInitParams* pInitParams;
    ListFile*                  pListFile;
    FILE*                      pFileForListing;
    ParseCSV*                  pPutSearchPath;
    LineInfo*                  pLineInfo;
    SizedString                globalLabel;
    Conditional*               pConditionals;
    BinaryBuffer*              pObjectBuffer;
    BinaryBuffer*              pDummyBuffer;
    BinaryBuffer*              pCurrentBuffer;
    OpCodeEntry*               instructionSets[INSTRUCTION_SET_INVALID];
    size_t                     instructionSetSizes[INSTRUCTION_SET_INVALID];
    MacroDefinition*           pMacroDefinitionsList;
    ParsedLine                 parsedLine;
    LineInfo                   linesHead;
    InstructionSetSupported    instructionSet;
    unsigned int               seenLUP : 1,
                               longA : 1,
                               longXY : 1;
    unsigned int               errorCount;
    unsigned int               warningCount;
    unsigned short             programCounter;
    unsigned short             programCounterBeforeDUM;
};


#define LOG_ERROR(pASSEMBLER, FORMAT, ...) LOG_ISSUE(pASSEMBLER->pLineInfo, "error", FORMAT, __VA_ARGS__), \
                                           pASSEMBLER->errorCount++

#define LOG_LINE_ERROR(pASSEMBLER, pLINEINFO, FORMAT, ...) LOG_ISSUE(pLINEINFO, "error", FORMAT, __VA_ARGS__), \
                                           pASSEMBLER->errorCount++

#define LOG_WARNING(pASSEMBLER, FORMAT, ...) LOG_ISSUE(pASSEMBLER->pLineInfo, "warning", FORMAT, __VA_ARGS__), \
                                           pASSEMBLER->warningCount++

#define LOG_LINE_WARNING(pASSEMBLER, pLINEINFO, FORMAT, ...) LOG_ISSUE(pLINEINFO, "warning", FORMAT, __VA_ARGS__), \
                                           pASSEMBLER->warningCount++

#define LOG_ISSUE(pLINEINFO, TYPE, FORMAT, ...) fprintf(stderr, \
                                       "%s:%d: " TYPE ": " FORMAT LINE_ENDING, \
                                       TextSource_GetFilename(pLINEINFO->pTextSource), \
                                       pLINEINFO->lineNumber, \
                                       __VA_ARGS__)


__throws Symbol* Assembler_FindLabel(Assembler* pThis, SizedString* pLabelName);

#endif /* _ASSEMBLER_PRIV_H_ */
