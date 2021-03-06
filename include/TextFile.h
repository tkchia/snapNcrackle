/*  Copyright (C) 2013  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#ifndef _TEXT_FILE_H_
#define _TEXT_FILE_H_

#include "try_catch.h"
#include "SizedString.h"

typedef struct TextFile TextFile;


__throws TextFile*    TextFile_CreateFromString(const char* pText);
__throws TextFile*    TextFile_CreateFromFile(const SizedString* pDirectoryName, 
                                              const SizedString* pFilename, 
                                              const char*        pFilenameSuffix);
__throws TextFile*    TextFile_CreateFromTextFile(const TextFile* pTextFile);
         void         TextFile_Free(TextFile* pThis);
         void         TextFile_Reset(TextFile* pThis);
         void         TextFile_SetEndOfFile(TextFile* pThis);
         void         TextFile_AdvanceTo(TextFile* pThis, const TextFile* pAdvanceToMatch);

         SizedString  TextFile_GetNextLine(TextFile* pThis);
         int          TextFile_IsEndOfFile(TextFile* pThis);
         unsigned int TextFile_GetLineNumber(TextFile* pThis);
         const char*  TextFile_GetFilename(TextFile* pThis);

#endif /* _TEXT_FILE_H_ */
