//---------------------------------------------------------------------------

#ifndef ModbusTCP_IPH
#define ModbusTCP_IPH

#include <vector>
#include <cstdint>

//#include "ExceptUtils.h"
#include "Modbus.h"

#define  DEFAULT_MODBUS_TCPIP_HOST  "localhost"
#define  DEFAULT_MODBUS_TCPIP_PORT  502

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------


class TCPIPContext : public Context {
public:
    TCPIPContext( SlaveAddrType SlaveAddr, TransactionIdType TransactionId = 0 )
        : Context( SlaveAddr ), transactionId_( TransactionId ) {}
protected:
    virtual TransactionIdType DoGetTransactionIdentifier() const override {
        return transactionId_;
    }
private:
    TransactionIdType transactionId_;
};

//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

class TCPIPProtocol : public Protocol {
public:
    String GetHost() const;
    void SetHost( String Val );
    uint16_t GetPort() const;
    void SetPort( uint16_t Val );
protected:
    using BMAPTransactionIdType = uint16_t;
    using BMAPProtocolType = uint16_t;
    using BMAPDataLengthType = uint16_t;
    using BMAPUnitIdType = uint8_t;

    virtual String DoGetProtocolParamsStr() const override;

    virtual String DoGetHost() const = 0;
    virtual void DoSetHost( String Val ) = 0;

    virtual uint16_t DoGetPort() const = 0;
    virtual void DoSetPort( uint16_t Val ) = 0;

    virtual void DoInputBufferClear() {}
    virtual void DoWrite( TBytes const OutBuffer ) = 0;
    virtual void DoRead( TBytes& InBuffer, size_t Length ) = 0;

//    DoReadCoilStatus
//    DoReadInputStatus
    virtual void DoReadHoldingRegisters( Context const & Context,
                                         RegAddrType StartAddr,
                                         RegCountType PointCount,
                                         RegDataType* Data ) override;
    virtual void DoReadInputRegisters( Context const & Context,
                                       RegAddrType StartAddr,
                                       RegCountType PointCount,
                                       RegDataType* Data ) override;
//    DoForceSingleCoil
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
//    DoForceMultipleCoils
      void DoPresetMultipleRegisters( Context const & Context,
                                      RegAddrType StartAddr,
                                      RegCountType PointCount,
                                      RegDataType const * Data ) override;
//    DoReportSlave
//    DoProgram884_M84
//    DoResetCommLink
//    DoReadGeneralReference
//    DoWriteGeneralReference
//    DoMaskWrite4XRegister
      virtual void DoMaskWrite4XRegister( Context const & Context,
                                          RegAddrType Addr,
                                          RegDataType AndMask,
                                          RegDataType OrMask ) override;
//    DoReadWrite4XRegisters
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
    static FunctionCode GetFunctionCode( TBytes const Buffer );
    static ExceptionCode GetExceptCode( TBytes const Buffer );
    static BMAPDataLengthType GetDataLength( TBytes const Buffer );
    static BMAPTransactionIdType GetBMAPTransactionIdentifier( TBytes const Buffer );
    static BMAPProtocolType GetBMAPProtocol( TBytes const Buffer );
    static BMAPDataLengthType GetBMAPDataLength( TBytes const Buffer );
    static BMAPUnitIdType GetBMAPUnitIdentifier( TBytes const Buffer );
    static BMAPDataLengthType GetBMAPHeaderLength() { return 7; }
    static int WriteBMAPHeader( TBytes & OutBuffer, int StartIdx,
                                Context const & Context );
    static int GetAddressPointCountPairLength() { return 4; }
    static int WriteAddressPointCountPair( TBytes & OutBuffer, int StartIdx,
                                           RegAddrType StartAddr,
                                           RegCountType PointCount );
    static int WriteData( TBytes & OutBuffer, int StartIdx, RegAddrType Data );

    static void CopyDataWord( Context const & Context, TBytes const Buffer,
                              int BufferOffset, uint16_t* Data );

    static int GetPayloadLength( Context const & Context,
                                 TBytes const  Buffer,
                                 int BufferOffset );

    void ReadRegisters( FunctionCode FnCode, Context const & Context,
                        RegAddrType StartAddr, RegCountType PointCount,
                        RegDataType* Data );

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
