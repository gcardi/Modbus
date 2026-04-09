/**
 * @file CommPort.h
 * @brief ECommError and TCommPort — Win32 serial (COM) port wrapper.
 *
 * @details Provides a C++ class encapsulating the Win32 serial communications API
 *  (CreateFile, SetCommState, ReadFile, WriteFile, etc.).
 *
 *  Original author: Harold Howe, bcbdev.com, 1996-1999.
 *  Redistribution permitted for program development; may not be resold as part of
 *  a software development library.
 *
 *  ECommError is thrown by TCommPort whenever an API call fails; inspect the
 *  @c Error member for the ErrorType and @c Errno for the Win32 last-error code.
 */

//---------------------------------------------------------------------------

#ifndef CommPortH
#define CommPortH
///// comm.h
/////     purpose : prototypes for for TCommPort, serial communictaions API encapsulation
/////    copyright: Harold Howe, bcbdev.com 1996-1999.
/////    notice   : This file provides an object that encapsulates the win32 serial port routines.
/////               This file may be distributed and used freely for program development,
/////               but it may not be resold as part of a software development library.
/////               In other words, don't take the source and attempt to sell it to other developers.

#include <windows.h>
#include <string>

#include <System.SysUtils.hpp>

// When the comm port class encounters an error, it throws an ECommError exception.
// The Error member of the exception object describes what went wrong. Some of the
// items should never happen. Others are fairly common place. You should pay special
// attention to the OPEN_ERROR type. This is the error that occurs when opening the port
// fails because another app already has the port open.

/**
 * @brief Exception thrown by TCommPort when a Win32 serial API call fails.
 *
 * @details Inspect the public @c Error member to determine which operation failed, and
 *  @c Errno (GetLastError() value) for the underlying OS error code.  @c Errno can be
 *  passed to FormatMessage() to obtain a human-readable description.
 */
class ECommError : public Exception
{
public:
    /**
     * @brief Categorises the failed COM port operation.
     */
    enum class ErrorType {
        BAD_SERIAL_PORT    ,  ///< Invalid or empty port name.
        BAD_BAUD_RATE      ,  ///< Baud rate not supported by the hardware.
        BAD_PORT_NUMBER    ,  ///< Port number out of valid range.
        BAD_STOP_BITS      ,  ///< Invalid stop-bit count.
        BAD_PARITY         ,  ///< Invalid parity value.
        BAD_BYTESIZE       ,  ///< Invalid data-bit count.
        PORT_ALREADY_OPEN  ,  ///< Port is already open; cannot open again.
        PORT_NOT_OPEN      ,  ///< Operation attempted on a closed port.
        OPEN_ERROR         ,  ///< CreateFile() failed (port in use by another process?).
        WRITE_ERROR        ,  ///< WriteFile() failed.
        READ_ERROR         ,  ///< ReadFile() failed.
        CLOSE_ERROR        ,  ///< CloseHandle() failed.
        PURGECOMM          ,  ///< PurgeComm() failed.
        FLUSHFILEBUFFERS   ,  ///< FlushFileBuffers() failed.
        GETCOMMSTATE       ,  ///< GetCommState() failed.
        SETCOMMSTATE       ,  ///< SetCommState() failed.
        SETUPCOMM          ,  ///< SetupComm() failed.
        SETCOMMTIMEOUTS    ,  ///< SetCommTimeouts() failed.
        CLEARCOMMERROR        ///< ClearCommError() failed.
    };

    /**
     * @brief Constructs the exception from an ErrorType.
     * @param error Identifies the failing operation.
     */
    ECommError( ErrorType error );

    ErrorType Error;  ///< Category of the failed operation.
    DWORD     Errno;  ///< Win32 error code from GetLastError() at the time of failure.
private:
    static String FormatErrorMessage( ErrorType Err );
    static const String ErrorString[19];
};

/**
 * @brief Win32 serial communications port wrapper.
 *
 * @details TCommPort encapsulates the Win32 serial API (CreateFile, SetCommState,
 *  ReadFile, WriteFile, CloseHandle, etc.) behind a simple C++ interface.
 *
 *  Usage pattern:
 *  @code
 *    TCommPort port;
 *    port.SetCommPort( L"COM3" );
 *    port.SetBaudRate( 9600 );
 *    port.OpenCommPort();
 *    // ... read/write ...
 *    port.CloseCommPort();
 *  @endcode
 *
 *  On any error, methods throw ECommError with an appropriate ErrorType.
 *
 *  @note TCommPort is non-copyable (copy constructor and assignment operator are deleted).
 */
class TCommPort
{
  public:
    /**
     * @brief Constructs a TCommPort with the specified timeouts.
     * @param ReadTimeOut   Per-character read timeout in milliseconds (default 1000 ms).
     * @param WriteTimeOut  Write timeout in milliseconds (default 1000 ms).
     */
    TCommPort( DWORD ReadTimeOut = 1000, DWORD WriteTimeOut = 1000 );

    /** @brief Destructor; automatically closes the port if still open. */
    ~TCommPort();

    /** @brief Opens the COM port using the currently configured parameters. @throws ECommError on failure. */
    void OpenCommPort();

    /** @brief Closes the COM port. @throws ECommError on failure. */
    void CloseCommPort() noexcept;

    /** @brief Sets the COM port device name (e.g., L"COM3" or L"\\\\.\\COM10"). */
    void SetCommPort(const std::wstring & port);
    /** @brief Returns the currently configured COM port device name. */
    [[ nodiscard ]] std::wstring GetCommPort();

    /** @brief Sets the baud rate (e.g., 9600, 19200, 115200). */
    void SetBaudRate(unsigned int newBaud);
    /** @brief Returns the configured baud rate. */
    [[ nodiscard ]] unsigned int GetBaudRate() const noexcept;

    /** @brief Sets the parity (NOPARITY, ODDPARITY, EVENPARITY, MARKPARITY, SPACEPARITY). */
    void SetParity(BYTE newParity);
    /** @brief Returns the configured parity. */
    [[ nodiscard ]] BYTE GetParity() const noexcept;

    /** @brief Sets the data bits (5–8; typically 8 for Modbus RTU). */
    void SetByteSize(BYTE newByteSize);
    /** @brief Returns the configured data bits. */
    [[ nodiscard ]] BYTE GetByteSize() const noexcept;

    /** @brief Sets the stop bits (ONESTOPBIT, ONE5STOPBITS, TWOSTOPBITS). */
    void SetStopBits(BYTE newStopBits);
    /** @brief Returns the configured stop bits. */
    [[ nodiscard ]] BYTE GetStopBits() const noexcept;

    /** @brief Applies a raw DCB structure (prefer the Set* methods instead). */
    void SetCommDCBProperties(DCB &properties);
    /** @brief Retrieves the current DCB structure. */
    void GetCommDCBProperties(DCB &properties) const noexcept;

    /** @brief Retrieves the COMMPROP structure for the open port. */
    void GetCommProperties(COMMPROP &properties);

    /** @brief Writes a null-terminated ASCII string to the port. @throws ECommError on failure. */
    void WriteString(const char *outString);

    /** @brief Writes @p ByteCount bytes from @p buffer to the port. @throws ECommError on failure. */
    void WriteBuffer(BYTE *buffer, unsigned int ByteCount);

    /**
     * @brief Writes bytes one at a time with inter-byte delay (for slow devices).
     * @throws ECommError on failure.
     */
    void WriteBufferSlowly(BYTE *buffer, unsigned int ByteCount);

    /** @brief Reads up to @p MaxBytes bytes into @p string. @return Actual bytes read. */
    [[ nodiscard ]] unsigned int ReadString(char *string, unsigned int MaxBytes);

    /**
     * @brief Reads exactly @p byteCount bytes into @p bytes.
     * @return Number of bytes actually read (0 on timeout).
     * @throws ECommError on I/O error (non-timeout).
     */
    [[ nodiscard ]] unsigned int ReadBytes(BYTE *bytes, unsigned int byteCount);

    /** @brief Discards up to @p MaxBytes bytes from the receive buffer. */
    void DiscardBytes(unsigned int MaxBytes);

    /** @brief Purges both TX and RX buffers. @throws ECommError on failure. */
    void PurgeCommPort();

    /** @brief Flushes the TX buffer to the hardware. @throws ECommError on failure. */
    void FlushCommPort();

    /** @brief Writes a single byte to the port. @throws ECommError on failure. */
    void  PutByte(BYTE value);

    /** @brief Reads and returns a single byte. @throws ECommError on timeout or error. */
    [[ nodiscard ]] BYTE  GetByte();

    /** @brief Returns the number of bytes currently waiting in the receive buffer. */
    [[ nodiscard ]] unsigned int BytesAvailable();

    /** @brief Returns @c true if the port is currently open. */
    [[ nodiscard ]] bool GetConnected() const noexcept
    {
        return m_CommOpen;
    }

    /**
     * @brief Returns the raw Win32 HANDLE to the COM port.
     * @details Use only when lower-level access is required; prefer the class methods.
     */
    [[ nodiscard ]] HANDLE GetHandle() const noexcept
    {
        return m_hCom;
    }

    /**
     * @brief Asserts or de-asserts the RTS (Request To Send) signal.
     * @param State @c true to assert RTS, @c false to de-assert.
     */
    void SetRTS( bool State );

  private:
    // Note: the destructor of the commport class automatically closes the
    //       port. This makes copy construction and assignment impossible.
    //       That is why I privatize them, and don't define them. In order
    //       to make copy construction and assignment feasible, we would need
    //       to employ a reference counting scheme.
    TCommPort(const TCommPort &);            // privatize copy construction
    TCommPort & operator=(const TCommPort&);  // and assignment.

    void VerifyOpen()
    {
        if(!m_CommOpen)
            throw ECommError(ECommError::ErrorType::PORT_NOT_OPEN) ;
    }
    void VerifyClosed()
    {
        if(m_CommOpen)
            throw ECommError(ECommError::ErrorType::PORT_ALREADY_OPEN) ;
    }

  // this stuff is private because we want to hide these details from clients
    bool           m_CommOpen;
    COMMTIMEOUTS   m_TimeOuts;
    std::wstring   m_CommPort;
    DCB            m_dcb;        // a DCB is a windows structure used for configuring the port
    HANDLE         m_hCom;       // handle to the comm port.
    DWORD          m_readTimeOut;
    DWORD          m_writeTimeOut;
};

//---------------------------------------------------------------------------
#endif
