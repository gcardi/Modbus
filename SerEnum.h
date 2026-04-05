/**
 * @file SerEnum.h
 * @brief Serial port enumeration utilities using the Windows SetupAPI.
 *
 * @details Provides SvcApp::Utils::EnumSerialPort(), a function template that enumerates
 *  all COM ports present in the system by querying the Windows SetupAPI device information
 *  set for GUID_DEVINTERFACE_COMPORT.  For each port found, a caller-supplied functor is
 *  invoked with the port name and device description, and its return value is appended to
 *  the output iterator.
 *
 *  The default functor, BuildSerialPortInfoTupleFnctr, packages the name and description
 *  into a boost::tuple<String,String>.
 */

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

/**
 * @brief Default functor for EnumSerialPort that packages the port name and description into a boost::tuple.
 *
 * @details When passed as the @p Fn argument to EnumSerialPort(), each discovered port is
 *  represented as a @c boost::tuple<String,String> where the first element is the port name
 *  (e.g., "COM3") and the second element is the human-readable device description.
 */
struct BuildSerialPortInfoTupleFnctr {
    /**
     * @brief Creates a tuple from the port name and device description.
     * @tparam T1 Type of the port name (typically @c String).
     * @tparam T2 Type of the device description (typically @c String).
     * @param Name  COM port name (e.g., "COM3").
     * @param Descr Device description from the Windows registry.
     * @return A boost::tuple containing (@p Name, @p Descr).
     */
    template<typename T1, typename T2>
    boost::tuple<T1,T2> operator()( T1 const & Name, T2 const & Descr ) {
        return boost::make_tuple( Name, Descr );
    }
};

/**
 * @brief Enumerates all COM ports currently present in the system.
 *
 * @details Uses the Windows SetupAPI (SetupDiGetClassDevs with GUID_DEVINTERFACE_COMPORT)
 *  to iterate over installed COM port devices.  For each device that has a valid "PortName"
 *  registry value, the functor @p Fn is called with the port name and device description,
 *  and the return value is written to the output iterator @p Out.
 *
 * @tparam OutputIterator Output iterator type whose value type matches the return type of @p Fn.
 * @tparam TF             Functor type callable as @c Fn(String portName, String description).
 * @param Out Output iterator that receives one entry per discovered COM port.
 * @param Fn  Functor invoked for each port (default: BuildSerialPortInfoTupleFnctr).
 *
 * @throws std::runtime_error (via RaiseLastOSError) if SetupDiGetClassDevs fails.
 *
 * @par Example
 * @code
 *   std::vector<boost::tuple<String,String>> ports;
 *   SvcApp::Utils::EnumSerialPort( std::back_inserter(ports) );
 *   for ( auto& p : ports )
 *       ShowMessage( p.get<0>() + " — " + p.get<1>() );
 * @endcode
 */
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
                RegQueryValueString( DeviceKey.Handle, _D( "PortName" ) );
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
