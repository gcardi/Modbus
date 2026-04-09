/**
 * @file ModbusTCP_IP.h
 * @brief TCPIPContext and TCPIPProtocol — MBAP (Modbus Application Protocol) framing over TCP/UDP.
 *
 * @details Defines:
 *  - Modbus::TCPIPContext: extends Context with an explicit MBAP transaction identifier.
 *  - Modbus::Master::TCPIPProtocol: abstract base implementing all Modbus function codes
 *    (FC01, FC02, FC03, FC04, FC05, FC06, FC15, FC16, FC22, FC23) over a byte-stream/datagram
 *    transport.  Concrete subclasses provide the actual I/O by implementing DoWrite() and DoRead().
 */

//---------------------------------------------------------------------------

#ifndef ModbusTCP_IPH
#define ModbusTCP_IPH

#include <vector>
#include <cstdint>

//#include "ExceptUtils.h"
#include "Modbus.h"

/** @brief Default Modbus TCP/UDP server hostname used when none is specified. */
#define  DEFAULT_MODBUS_TCPIP_HOST  "localhost"

/** @brief Default Modbus TCP/UDP port number (IANA assigned Modbus port). */
#define  DEFAULT_MODBUS_TCPIP_PORT  502

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------


/**
 * @brief Modbus context extended with an explicit MBAP transaction identifier.
 *
 * @details In Modbus TCP/UDP the MBAP header contains a two-byte Transaction Identifier
 *  that the master sets and the slave echoes back.  TCPIPContext stores this value so that
 *  TCPIPProtocol can validate that a response corresponds to the request that was sent.
 *
 *  The transaction identifier is typically a monotonically increasing sequence number
 *  managed by the caller.
 */
class TCPIPContext : public Context {
public:
    /**
     * @brief Constructs a TCP/IP context.
     * @param SlaveAddr     Modbus unit identifier (slave address).
     * @param TransactionId MBAP transaction identifier (default 0).
     */
    TCPIPContext( SlaveAddrType SlaveAddr, TransactionIdType TransactionId = 0 )
        : Context( SlaveAddr ), transactionId_( TransactionId ) {}
protected:
    /** @brief Returns the stored MBAP transaction identifier. */
    virtual TransactionIdType DoGetTransactionIdentifier() const noexcept override {
        return transactionId_;
    }
private:
    TransactionIdType transactionId_;
};

//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

/**
 * @brief Abstract Modbus master protocol implementing MBAP framing over a byte-stream or datagram transport.
 *
 * @details TCPIPProtocol is an abstract semi-concrete transport that handles the complete
 *  MBAP (Modbus Application Protocol) layer following the NVI pattern. It:
 *  - Builds Modbus request frames with proper MBAP headers.
 *  - Delegates I/O to pure virtual DoWrite() and DoRead() hooks.
 *  - Validates MBAP response headers (transaction ID, protocol ID = 0, unit identifier).
 *  - Implements all Modbus function codes (FC01, FC02, FC03, FC04, FC05, FC06, FC15, FC16, FC22, FC23)
 *    inherited by TCP/UDP transports.
 *
 *  **NVI Architecture:**
 *  - Concrete TCP/UDP subclasses (e.g., TCPProtocolIndy, UDPProtocolWinSock) override the
 *    pure virtual Do…() hooks: DoOpen(), DoClose(), DoIsConnected(), DoWrite(), DoRead().
 *  - Public methods (Open, Close, ReadHoldingRegisters, etc.) are inherited from Protocol
 *    and are NOT overridden in subclasses—they remain non-virtual throughout the hiearchy.
 *  - Subclasses must implement:
 *    - DoGetHost() / DoSetHost()
 *    - DoGetPort() / DoSetPort()
 *    - DoOpen() / DoClose() / DoIsConnected()
 *    - DoWrite() — sends the entire request datagram or stream segment.
 *    - DoRead()  — reads exactly @p Length bytes from the response stream or datagram cache.
 *    (other Do…() methods for FC03, FC04, etc. are defined in Protocol and inherited here).
 *
 *  All Modbus function codes supported by this library
 *  (FC01, FC02, FC03, FC04, FC05, FC06, FC15, FC16, FC22, FC23) are
 *  fully implemented in Protocol and inherited by TCPIPProtocol; concrete TCP/UDP subclasses
 *  do not need to reimplement function code logic.
 */
class TCPIPProtocol : public Protocol {
public:
    /** @brief Returns the hostname or IP address of the Modbus server. */
    [[ nodiscard ]] String GetHost() const;

    /** @brief Sets the hostname or IP address of the Modbus server. */
    void SetHost( String Val );

    /** @brief Returns the TCP/UDP port number (default 502). */
    [[ nodiscard ]] uint16_t GetPort() const;

    /** @brief Sets the TCP/UDP port number. */
    void SetPort( uint16_t Val );
protected:
    using BMAPTransactionIdType = uint16_t;  ///< MBAP Transaction Identifier field type.
    using BMAPProtocolType      = uint16_t;  ///< MBAP Protocol Identifier field type (always 0 for Modbus).
    using BMAPDataLengthType    = uint16_t;  ///< MBAP Length field type.
    using BMAPUnitIdType        = uint8_t;   ///< MBAP Unit Identifier field type.

    /** @brief Returns a string describing host and port for diagnostic purposes. */
    virtual String DoGetProtocolParamsStr() const override;

    /** @brief Returns the current server hostname (implemented by concrete subclass). */
    virtual String DoGetHost() const = 0;
    /** @brief Sets the server hostname (implemented by concrete subclass). */
    virtual void DoSetHost( String Val ) = 0;

    /** @brief Returns the current server port (implemented by concrete subclass). */
    virtual uint16_t DoGetPort() const = 0;
    /** @brief Sets the server port (implemented by concrete subclass). */
    virtual void DoSetPort( uint16_t Val ) = 0;

    /**
     * @brief Clears the receive-side input buffer (optional; no-op by default).
     * @details UDP subclasses override this to discard stale cached datagrams.
     */
    virtual void DoInputBufferClear() {}

    /**
     * @brief Sends the complete MBAP request frame to the server.
     * @param OutBuffer Byte array containing the fully assembled MBAP request.
     */
    virtual void DoWrite( TBytes const OutBuffer ) = 0;

    /**
     * @brief Reads exactly @p Length bytes from the server response into @p InBuffer.
     * @param[out] InBuffer Buffer to receive the data; resized to @p Length.
     * @param      Length   Number of bytes to read.
     * @throws EBaseException on timeout or I/O error.
     */
    virtual void DoRead( TBytes& InBuffer, size_t Length ) = 0;

    virtual void DoReadCoilStatus( Context const & Context,
                                   CoilAddrType StartAddr,
                                   CoilCountType PointCount,
                                   CoilDataType* Data ) override;
    virtual void DoReadInputStatus( Context const & Context,
                                    CoilAddrType StartAddr,
                                    CoilCountType PointCount,
                                    CoilDataType* Data ) override;
    virtual void DoReadHoldingRegisters( Context const & Context,
                                         RegAddrType StartAddr,
                                         RegCountType PointCount,
                                         RegDataType* Data ) override;
    virtual void DoReadInputRegisters( Context const & Context,
                                       RegAddrType StartAddr,
                                       RegCountType PointCount,
                                       RegDataType* Data ) override;
    virtual void DoForceSingleCoil( Context const & Context,
                                    CoilAddrType Addr,
                                    bool Value ) override;
    virtual void DoPresetSingleRegister( Context const & Context,
                                         RegAddrType Addr,
                                         RegDataType Data ) override;
//    DoReadExceptionStatus
//    DoDiagnostics
//    DoProgram484
//    DoPoll484
//    DoFetchCommEventCtr
//    DoFetchCommEventLog
//    DoProgramController
//    DoPollController
    virtual void DoForceMultipleCoils( Context const & Context,
                                       CoilAddrType StartAddr,
                                       CoilCountType PointCount,
                                       const CoilDataType* Data ) override;
      void DoPresetMultipleRegisters( Context const & Context,
                                      RegAddrType StartAddr,
                                      RegCountType PointCount,
                                      RegDataType const * Data ) override;
//    DoReportSlave
//    DoProgram884_M84
//    DoResetCommLink
//    DoReadGeneralReference
//    DoWriteGeneralReference
      virtual void DoMaskWrite4XRegister( Context const & Context,
                                          RegAddrType Addr,
                                          RegDataType AndMask,
                                          RegDataType OrMask ) override;
    virtual void DoReadWrite4XRegisters( Context const & Context,
                                         RegAddrType ReadStartAddr,
                                         RegCountType ReadPointCount,
                                         RegDataType* ReadData,
                                         RegAddrType WriteStartAddr,
                                         RegCountType WritePointCount,
                                         const RegDataType* WriteData ) override;
//    DoReadFIFOQueue
private:
    static void RaiseExceptionIfBMAPIsNotValid( Context const & Context,
                                                TBytes const Buffer );
    static void RaiseExceptionIfBMAPIsNotEQ( Context const & Context,
                                             TBytes const LBuffer,
                                             TBytes const RBuffer );
    static void RaiseExceptionIfReplyIsNotValid( Context const & Context,
                                                 TBytes const Buffer,
                                                 FunctionCode ExpectedFunctionCode );
    static FunctionCode GetFunctionCode( TBytes const Buffer ) noexcept;
    static ExceptionCode GetExceptCode( TBytes const Buffer ) noexcept;
    static BMAPDataLengthType GetDataLength( TBytes const Buffer ) noexcept;
    static BMAPTransactionIdType GetBMAPTransactionIdentifier( TBytes const Buffer ) noexcept;
    static BMAPProtocolType GetBMAPProtocol( TBytes const Buffer ) noexcept;
    static BMAPDataLengthType GetBMAPDataLength( TBytes const Buffer ) noexcept;
    static BMAPUnitIdType GetBMAPUnitIdentifier( TBytes const Buffer ) noexcept;
    static BMAPDataLengthType GetBMAPHeaderLength() noexcept { return 7; }
    static int WriteBMAPHeader( TBytes & OutBuffer, int StartIdx,
                                Context const & Context );
    static int GetAddressPointCountPairLength() noexcept { return 4; }
    static int WriteAddressPointCountPair( TBytes & OutBuffer, int StartIdx,
                                           RegAddrType StartAddr,
                                           RegCountType PointCount ) noexcept;
    static int WriteData( TBytes & OutBuffer, int StartIdx, RegAddrType Data ) noexcept;

    static void CopyDataWord( Context const & Context, TBytes const Buffer,
                              int BufferOffset, uint16_t* Data );

    static int GetPayloadLength( Context const & Context,
                                 TBytes const  Buffer,
                                 int BufferOffset );

    void ReadRegisters( FunctionCode FnCode, Context const & Context,
                        RegAddrType StartAddr, RegCountType PointCount,
                        RegDataType* Data );

    void ReadBits( FunctionCode FnCode, Context const & Context,
                   CoilAddrType StartAddr, CoilCountType PointCount,
                   CoilDataType* Data );

protected:
    template<typename T>
    static uint16_t GetLength( T const & Data ) {
        //return static_cast<BMAPDataLengthType>( Data.size() );
        return static_cast<BMAPDataLengthType>( Data.Length );
    }

    template<typename T>
    static void SetLength( T& OutBuffer, uint16_t Length ) {
        //OutBuffer.resize( static_cast<typename T::size_type>( Length ) );
        OutBuffer.Length = Length;
    }

    template<typename T>
    static uint8_t const * GetData( T const & OutBuffer ) {
    	return &OutBuffer[0];
    }

    template<typename T>
    static uint8_t* GetData( T& OutBuffer ) {
    	return &OutBuffer[0];
    }
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
