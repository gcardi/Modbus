//---------------------------------------------------------------------------

#ifndef ModbusH
#define ModbusH

#include <System.SysUtils.hpp>

#include <cstdint>

#include <string>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

//#include <boost/utility.hpp>



//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------

extern void DebugBytesToHex( String Prologue, TBytes Data );

enum class FunctionCode {
    ReadCoilStatus = 1,
    ReadInputStatus = 2,
    ReadHoldingRegisters = 3,
    ReadInputRegisters = 4,
    ForceSingleCoil = 5,
    PresetSingleRegister = 6,
    ReadExceptionStatus = 7,
    Diagnostics = 8,
    Program484 = 9,
    Poll484 = 10,
    FetchCommEventCtr = 11,
    FetchCommEventLog = 12,
    ProgramController = 13,
    PollController = 14,
    ForceMultipleCoils = 15,
    PresetMultipleRegisters = 16,
    ReportSlave = 17,
    Program884_M84 = 18,
    ResetCommLink = 19,
    ReadGeneralReference = 20,
    WriteGeneralReference = 21,
    MaskWrite4XRegister = 22,
    ReadWrite4XRegisters = 23,
    ReadFIFOQueue = 24
};

enum class ExceptionCode {
   IllegalFunction = 1,      // The function code received in the query
                             // is not an allowable action for the slave.
                             // If a Poll Program Complete command
                             // was issued, this code indicates that no
                             // program function preceded it.

   IllegalDataAddress = 2,   // The data address received in the query
                             // is not an allowable address for the
                             // slave.

   IllegalDataValue = 3,     // A value contained in the query data
                             // field is not an allowable value for the
                             // slave.

   SlaveDeviceFailure = 4,   // An unrecoverable error occurred while
                             // the slave was attempting to perform the
                             // requested action.

   Acknowledge = 5,          // The slave has accepted the request
                             // and is processing it, but a long duration
                             // of time will be required to do so. This
                             // response is returned to prevent a
                             // timeout error from occurring in the
                             // master. The master can next issue a
                             // Poll Program Complete message to
                             // determine if processing is completed.

   SlaveDeviceBusy = 6,      // The slave is engaged in processing a
                             // long–duration program command. The
                             // master should retransmit the message
                             // later when the slave is free.

   NegativeAcknowledge = 7,  // The slave cannot perform the program
                             // function received in the query. This
                             // code is returned for an unsuccessful
                             // programming request using function
                             // code 13 or 14 decimal. The master
                             // should request diagnostic or error
                             // information from the slave.

   MemoryParityError = 8,    // The slave attempted to read extended
                             // memory, but detected a parity error in
                             // the memory. The master can retry the
                             // request, but service may be required on
                             // the slave device.
};

//---------------------------------------------------------------------------

class Context {
public:
    using SlaveAddrType = uint8_t;
    using TransactionIdType = uint16_t;

    explicit Context( SlaveAddrType SlaveAddr ) : slaveAddr_( SlaveAddr ) {}
    virtual ~Context() = default;
    SlaveAddrType GetSlaveAddr() const { return DoGetSlaveAddr(); }
    TransactionIdType GetTransactionIdentifier() const {
        return DoGetTransactionIdentifier();
    }
protected:
    virtual SlaveAddrType DoGetSlaveAddr() const { return slaveAddr_; }
    virtual TransactionIdType DoGetTransactionIdentifier() const {
        return TransactionIdType();
    }
private:
    SlaveAddrType slaveAddr_;
};
//---------------------------------------------------------------------------

class EBaseException : public Exception {
public:
    explicit EBaseException( String Message )
      : Exception(
            _T( "Modbus exception %s" )
          , ARRAYOFCONST( ( Message ) )
        ) {}
	EBaseException( String Msg, TVarRec *Args, int Args_High )
      : Exception(
            _T( "Modbus exception %s" )
          , ARRAYOFCONST( ( Format( Msg, Args, Args_High ) ) )
        ) {}
};
//---------------------------------------------------------------------------

class EContextException : public EBaseException {
public:
    explicit EContextException( Context const & Context, String Message )
       : EBaseException( Message )
       , context_( Context ) {}
    EContextException( Context const & Context, String Msg, TVarRec *Args,
                       int Args_High )
       : EBaseException( Msg, Args, Args_High )
       , context_( Context ) {}
    Context const & GetContext() const { return context_; }
public:
    Context context_;
};
//---------------------------------------------------------------------------

class EProtocolException : public EContextException
{
public:
    EProtocolException( Context const & Context, ExceptionCode Code, String Message )
       : EContextException( Context, Message )
       , code_( Code ) {}
    EProtocolException( Context const & Context, ExceptionCode Code, String Msg,
                        TVarRec *Args, int Args_High )
       : EContextException( Context, Msg, Args, Args_High )
       , code_( Code ) {}
    ExceptionCode GetCode() const { return code_; }
private:
    ExceptionCode code_;
};
//---------------------------------------------------------------------------

extern String GetExceptionCodeText( ExceptionCode Code );
extern String GetExceptionCodeDescription( ExceptionCode Code );

#if !defined(__BORLANDC__) || defined(__clang__) // BCC32C, BCC64
[[ noreturn ]] extern void RaiseFunctionCodeNotImplementedException( FunctionCode Code );
[[ noreturn ]] extern void RaiseStandardException( Context const & Context,
                                                   ExceptionCode Code,
                                                   String Prefix = String() );
  #else // BCC32
extern void RaiseFunctionCodeNotImplementedException( FunctionCode Code ) [[noreturn]];
extern void RaiseStandardException( Context const & Context,
                                    ExceptionCode Code,
                                    String Prefix = String() ) [[noreturn]];
  #endif
//#endif

//---------------------------------------------------------------------------

template<ExceptionCode Code>
class EProtocolStdException : public EProtocolException
{
public:
    EProtocolStdException( Context const & Context, String Prefix = String() )
        : EProtocolException(
            Context,
            Code,
            Prefix.IsEmpty() ?
              GetExceptionCodeText( Code )
            :
              Format( _T( "%s: %s" ), ARRAYOFCONST( ( Prefix, GetExceptionCodeText( Code ) ) ) )
          )
    {}
};

using
  EIllegalFunction =
    EProtocolStdException<ExceptionCode::IllegalFunction>;

using
  EIllegalDataAddress =
    EProtocolStdException<ExceptionCode::IllegalDataAddress>;

using
  EIllegalDataValue =
    EProtocolStdException<ExceptionCode::IllegalDataValue>;

using
  ESlaveDeviceFailure =
    EProtocolStdException<ExceptionCode::SlaveDeviceFailure>;

using
  EAcknowledge =
    EProtocolStdException<ExceptionCode::Acknowledge>;

using
  ESlaveDeviceBusy =
    EProtocolStdException<ExceptionCode::SlaveDeviceBusy>;

using
  ENegativeAcknowledge =
    EProtocolStdException<ExceptionCode::NegativeAcknowledge>;

using
  EMemoryParityError =
    EProtocolStdException<ExceptionCode::MemoryParityError>;

using CoilAddrType = uint16_t;
using CoilCountType = uint16_t;
using CoilDataType = uint8_t;

using RegAddrType = uint16_t;
using RegCountType = uint16_t;
using RegDataType = uint16_t;

//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

class Protocol {
public:
    Protocol();
    virtual ~Protocol();

    void Open();
    void Close();
    bool IsConnected() const { return DoIsConnected(); }

    String GetProtocolName() const { return DoGetProtocolName(); }
    String GetProtocolParamsStr() const { return DoGetProtocolParamsStr(); }

//    ReadCoilStatus
//    ReadInputStatus
    void ReadHoldingRegisters( Context const & Context,
                               RegAddrType StartAddr, RegCountType PointCount,
                               RegDataType* Data )
    {
        DoReadHoldingRegisters( Context, StartAddr, PointCount, Data );
    }

    void ReadInputRegisters( Context const & Context,
                             RegAddrType StartAddr, RegCountType PointCount,
                             RegDataType* Data )
    {
        DoReadInputRegisters( Context, StartAddr, PointCount, Data );
    }

//    ForceSingleCoil
//    PresetSingleRegister
    void PresetSingleRegister( Context const & Context,
                               RegAddrType Addr, RegDataType Data )
    {
        DoPresetSingleRegister( Context, Addr, Data );
    }
//    ReadExceptionStatus
//    Diagnostics
//    Program484
//    Poll484
//    FetchCommEventCtr
//    FetchCommEventLog
//    ProgramController
//    PollController
//    ForceMultipleCoils
    void PresetMultipleRegisters( Context const & Context,
                                  RegAddrType StartAddr, RegCountType PointCount,
                                  const RegDataType* Data )
    {
        DoPresetMultipleRegisters( Context, StartAddr, PointCount, Data );
    }
//    ReportSlave
//    Program884_M84
//    ResetCommLink
//    ReadGeneralReference
//    WriteGeneralReference
//    MaskWrite4XRegister
    void MaskWrite4XRegister( Context const & Context,
                              RegAddrType Addr, RegDataType AndMask,
                              RegDataType OrMask )
    {
        DoMaskWrite4XRegister( Context, Addr, AndMask, OrMask );
    }
//    ReadWrite4XRegisters
//    ReadFIFOQueue
protected:
    virtual String DoGetProtocolName() const = 0;
    virtual String DoGetProtocolParamsStr() const = 0;
    virtual void DoOpen() = 0;
    virtual void DoClose() = 0;
    virtual bool DoIsConnected() const = 0;

//    DoReadInputStatus
    virtual void DoReadHoldingRegisters( Context const & Context,
                                         RegAddrType StartAddr,
                                         RegCountType PointCount,
                                         RegDataType* Data ) = 0;
    virtual void DoReadInputRegisters( Context const & Context,
                                       RegAddrType StartAddr,
                                       RegCountType PointCount,
                                       RegDataType* Data ) = 0;

//    DoForceSingleCoil
    virtual void DoPresetSingleRegister( Context const & Context,
                                         RegAddrType Addr,
                                         RegDataType Data ) = 0;
//    DoReadExceptionStatus
//    DoDiagnostics
//    DoProgram484
//    DoPoll484
//    DoFetchCommEventCtr
//    DoFetchCommEventLog
//    DoProgramController
//    DoPollController
//    DoForceMultipleCoils
      virtual void DoPresetMultipleRegisters( Context const & Context,
                                              RegAddrType StartAddr,
                                              RegCountType PointCount,
                                              const RegDataType* Data ) = 0;
//    DoReportSlave
//    DoProgram884_M84
//    DoResetCommLink
//    DoReadGeneralReference
//    DoWriteGeneralReference
    virtual void DoMaskWrite4XRegister( Context const & Context,
                                        RegAddrType Addr,
                                        RegDataType AndMask,
                                        RegDataType OrMask ) = 0;
//    DoReadWrite4XRegisters
//    DoReadFIFOQueue

    void RaiseExceptionIfIsConnected( String SubMsg ) const;
    void RaiseExceptionIfIsNotConnected( String SubMsg ) const;
private:
};
//---------------------------------------------------------------------------

class SessionManager  {
public:
    explicit SessionManager( Protocol& Protocol )
      : protocol_( Protocol ) { protocol_.Open(); }
    ~SessionManager() { protocol_.Close(); }
    SessionManager( SessionManager const & Rhs ) = delete;
    SessionManager& operator=( SessionManager const & Rhs ) = delete;
    SessionManager( SessionManager&& Rhs ) = delete;
    SessionManager& operator=( SessionManager&& Rhs ) = delete;
private:
    Protocol& protocol_;
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
