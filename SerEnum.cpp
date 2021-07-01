//---------------------------------------------------------------------------

#pragma hdrstop

#include <vector>

#include <System.hpp>
#include <System.SysUtils.hpp>

#include "SerEnum.h"

using std::vector;

//---------------------------------------------------------------------------
#pragma package(smart_init)

//---------------------------------------------------------------------------
namespace SvcApp {
//---------------------------------------------------------------------------
namespace Utils {
//---------------------------------------------------------------------------

String RegQueryValueString( HKEY Key, String Name )
{
    DWORD Type = 0;
    DWORD DataSize = 0;
    LONG Error =
        ::RegQueryValueEx(
            Key, Name.c_str(), NULL, &Type, NULL, &DataSize
        );
    if ( Error != ERROR_SUCCESS ) {
        ::SetLastError( Error );
        RaiseLastOSError();
    }
    if ( Type != REG_SZ ) {
        ::SetLastError( ERROR_INVALID_DATA );
        RaiseLastOSError();
    }

    vector<BYTE> Buffer( DataSize + sizeof( TCHAR ) );

    DWORD ReturnedSize = DataSize;
    Error = ::RegQueryValueEx(
                Key, Name.c_str(), NULL, &Type, &Buffer[0], &ReturnedSize
            );
    if ( Error != ERROR_SUCCESS ) {
        ::SetLastError( Error );
        RaiseLastOSError();
    }

    if ( ReturnedSize > DataSize ) {
        ::SetLastError( ERROR_INVALID_DATA );
        RaiseLastOSError();
    }
    return String( reinterpret_cast<LPTSTR>( &Buffer[0] ),
                   DataSize / sizeof( TCHAR ) - 1 );
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
} // End of namespace Utils
//---------------------------------------------------------------------------
} // End of namespace SvcApp
//---------------------------------------------------------------------------


