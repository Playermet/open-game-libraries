/*
===========================================================================
The Open Game Libraries.
Copyright (C) 2007-2010 Lusito Software

Author:  Santo Pfingsten (TTK-Bandit)
Purpose: Helper Utilities
-----------------------------------------

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
===========================================================================
*/


#include <stdarg.h>
#include "FileSystemEx.h"

#ifdef OG_WIN32
	#include <windows.h>
	#include <io.h>
#else
	#include <glob.h>
#endif

namespace og {

/*
==============================================================================

  FileReadWriteError

==============================================================================
*/
/*
================
FileReadWriteError::FileReadWriteError
================
*/
FileReadWriteError::FileReadWriteError( const char *msg ) {
	customMsg = strdup(msg);
	type = CUSTOM;
}
FileReadWriteError::FileReadWriteError( ErrorType type ) {
	customMsg = NULL;
	this->type = type;
}

/*
================
FileReadWriteError::~FileReadWriteError
================
*/
FileReadWriteError::~FileReadWriteError() {
	if ( customMsg )
		free( (void*)customMsg );
}

/*
================
FileReadWriteError::ToString
================
*/
const char *FileReadWriteError::ToString( void ) {
	switch(type) {
			case CUSTOM:		return customMsg;
			case DECOMPRESS:	return "Decompression failed";
			case READ:			return "Read failed";
			case WRITE:			return "Write failed";
			case REWIND:		return "Rewind failed";
			case SEEK:			return "Seek failed";
			case FLUSH:			return "Flush failed";
			case END_OF_FILE:	return "Unexpected end of file";
	}
	return "Unknown read/write error";
}

/*
==============================================================================

  FileFinder

==============================================================================
*/
class FileFinder {
public:
	FileFinder( const char *_baseDir, const char *_extension, StringList *_list, int flags ) {
#ifdef OG_WIN32
		StringToWide( _baseDir, baseDir );
		StringToWide( _extension, extension );
#else
		baseDir = _baseDir;
		extension = _extension;
#endif
		list		= _list;
		addDirs		= (flags & LF_DIRS) != 0;
		addFiles	= (flags & LF_FILES) != 0;
		recursive	= (flags & LF_CHECK_SUBDIRS) != 0;
	}
#ifdef OG_WIN32
	bool	SearchDir( const wchar_t *dir ) {
		WIN32_FIND_DATA findData;
		DWORD dwError=0;

		std::wstring strMatch = baseDir.data;
		strMatch += L"/";
		strMatch += dir;
		if ( !dir[0] )
			strMatch += L"*";
		else 
			strMatch += L"/*";

		HANDLE hFind = FindFirstFile(strMatch.c_str(), &findData);
		if ( hFind == INVALID_HANDLE_VALUE )
			return false;

		int len;
		int extLen = extension.size-1;
		std::wstring wfilename;
		String filename;
		do {
			len = wcslen( findData.cFileName );
			if ( len > 0 && wcsicmp( findData.cFileName + len-extLen, extension.data ) != 0 )
				continue;

			if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
				// Skip "." and ".." directories
				if ( findData.cFileName[0] == '.' )
					continue;

				wfilename = dir;
				wfilename += L"\\";
				wfilename += findData.cFileName;
				wfilename += L"\\";
				if ( addDirs ) {
					filename.FromWide( wfilename.c_str() );
					list->Append( filename );
				}
				if ( recursive )
					SearchDir( wfilename.c_str() );
			}
			else if ( addFiles ) {
				wfilename = dir;
				wfilename += L"\\";
				wfilename += findData.cFileName;
				filename.FromWide( wfilename.c_str() );
				list->Append( filename );
			}
		} while( FindNextFile(hFind, &findData) != 0 );

#if 0 //! @todo	warning ?
		if ( GetLastError() != ERROR_NO_MORE_FILES )
			;
#endif
		FindClose(hFind);
		return true;
	}
#else
	bool	SearchDir( const char *dir ) {
		// Set the search pattern
		String findname = baseDir;
		findname += "/";
		findname += dir;
		int nameOffset = findname.length();
		if ( !dir[0] )
			findname += "*";
		else {
			find->nameOffset += 1;
			findname += "/*";
		}

		// Find all files
		glob_t findResult;
		if ( glob( findname.c_str(), 0, NULL, &find->findResult ) != 0 )
			return false;

		const char *name;
		int len, extLen = extension.length();
		String filename;
		struct stat stat_Info;
		// Cycle through all files found
		for ( int i=0; i<findResult.gl_pathc; i++ ) {
			// Get file information about the current file.
			if ( stat ( findResult.gl_pathv[i], &stat_Info ) == -1 )
				continue;

			name = findResult.gl_pathv[i] + baseDir.length();
			len = strlen( name );
			if ( len > 0 && String::Icmp( name + len-extLen, extension.c_str() ) != 0 )
				continue;

			if ( stat_Info.st_mode & S_IFDIR ) {
				// Skip "." and ".." directories
				if ( name[0] == '.' )
					continue;

				filename = name;
				filename += "\\";
				if ( addDirs )
					list->push_back( filename );
				if ( recursive )
					SearchDir( filename.c_str() );
			}
			else if ( addFiles )
				list->push_back( name );
		}

		globfree( &findResult );
		return true;
	}
#endif

private:
	bool			addDirs, addFiles;
	bool			recursive;
	StringList *	list;

#ifdef OG_WIN32
	DynBuffer<wchar_t>	baseDir;
	DynBuffer<wchar_t>	extension;
#else
	String			baseDir;
	String			extension;
#endif
};
/*
================
LocalFileSearch
================
*/
bool LocalFileSearch( const char *baseDir, const char *dir, const char *extension, StringList *list, int flags ) {
	FileFinder finder( baseDir, extension, list, flags );

#ifdef OG_WIN32
	DynBuffer<wchar_t> strDir;
	StringToWide( dir, strDir );
	return finder.SearchDir( strDir.data );
#else
	return finder.SearchDir( dir );
#endif
}

/*
==============================================================================

  FileListEx

==============================================================================
*/
/*
================
FileListEx::GetNext
================
*/
bool FileListEx::GetNext( void ) {
	if ( (position+1) < files.Num() ) {
		position++;
		return true;
	}
	return false;
}

// Win32 needs help to open utf-8 filenames.
#ifdef OG_WIN32
/*
================
fopenwin32
================
*/
FILE *fopenwin32( const char *filename, const char *mode ) {
	DynBuffer<wchar_t> strFilename, strMode;
	StringToWide( filename, strFilename );
	StringToWide( mode, strMode );
	return _wfopen( strFilename.data, strMode.data );
}
#endif

}