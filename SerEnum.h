//---------------------------------------------------------------------------

#ifndef SerEnumH
#define SerEnumH

#include <Setupapi.h>

#include <System.hpp>
#include <System.SysUtils.hpp>

#include <boost/tuple/tuple.hpp>

//---------------------------------------------------------------------------
namespace SvcApp {
//---------------------------------------------------------------------------
namespace Utils {
//---------------------------------------------------------------------------

struct BuildSerialPortInfoTupleFnctr {
    template<typename T1, typename T2>
    boost::tuple<T1,T2> operator()( T1 const & Name, T2 const & Descr ) {
        return boost::make_tuple( Name, Descr );
    }
};

template<typename OutputIterator, typename TF>
void EnumSerialPort( OutputIterator Out, TF Fn = BuildSerialPortInfoTupleFnctr() )
{
    struct DevInfoSetMngr {
        DevInfoSetMngr()
        {
            static const GUID GuidDevintfComport =
                { 0x86E0D1E0L, 0x8089, 0x11D0,
                  0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73
                };
            Handle=
                ::SetupDiGetClassDevs(
                    &GuidDevintfComport, NULL, NULL,
                    DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
                );
            if ( Handle == INVALID_HANDLE_VALUE ) {
                RaiseLastOSError();
            }
        }
        ~DevInfoSetMngr() {
            if ( Handle != INVALID_HANDLE_VALUE ) {
                ::SetupDiDestroyDeviceInfoList( Handle );
            }
        }
        HDEVINFO Handle;
    }
    DevInfoSet;

    SP_DEVINFO_DATA DevInfo = { 0 };
    DevInfo.cbSize = sizeof( SP_DEVINFO_DATA );
    for ( int Idx = 0 ; ::SetupDiEnumDeviceInfo( DevInfoSet.Handle, Idx, &DevInfo ) ; ++Idx ) {
        struct RegKeyMngr {
            explicit RegKeyMngr( HKEY Key ) : Handle( Key ) {}
            ~RegKeyMngr() { ::RegCloseKey( Handle ); }
            HKEY Handle;
        }
        DeviceKey(
            ::SetupDiOpenDevRegKey(
                DevInfoSet.Handle, &DevInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV,
                KEY_QUERY_VALUE
            )
        );

        if ( DeviceKey.Handle != INVALID_HANDLE_VALUE ) {

            extern String RegQueryValueString( HKEY Key, String Name );

            String const PortName =
                RegQueryValueString( DeviceKey.Handle, _T( "PortName" ) );
            if ( !PortName.IsEmpty() ) {
                BYTE Buffer[2048];
                String Description;
                DWORD Size = 0;
                DWORD Type = 0;
                bool PropDescOk =
                    ::SetupDiGetDeviceRegistryProperty(
                         DevInfoSet.Handle, &DevInfo, SPDRP_DEVICEDESC, &Type,
                         Buffer, sizeof Buffer, &Size
                    );
                if ( PropDescOk && Type == REG_SZ ) {
                    Description = reinterpret_cast<LPTSTR>( Buffer );
                }
                *Out++ = Fn( PortName, Description );
            }
        }
    }
}

//---------------------------------------------------------------------------
} // End of namespace Utils
//---------------------------------------------------------------------------
} // End of namespace SvcApp
//---------------------------------------------------------------------------
#endif
