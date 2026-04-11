/**
 * @file ModbusDummy.h
 * @brief Modbus::Master::DummyProtocol — no-op stub implementation for unit testing.
 *
 * @details DummyProtocol implements the full Protocol interface with empty bodies.
 *  All read operations leave their output buffers unchanged; all write operations are
 *  silently discarded.  This allows Modbus master logic to be exercised in unit tests
 *  without requiring physical hardware or a live network connection.
 */

//---------------------------------------------------------------------------

#ifndef ModbusDummyH
#define ModbusDummyH

#include "Modbus.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

/**
 * @brief No-op Modbus master protocol stub for unit testing without hardware.
 *
 * @details Concrete NVI implementation that provides a safe no-op stub for unit testing.
 *
 *  **NVI Pattern:** Implements all protected Do…() virtual hooks defined in Protocol
 *  (DoOpen, DoClose, DoIsConnected, DoReadHoldingRegisters, etc.).
 *  The public API methods are inherited non-virtually from Protocol.
 *
 *  Behavior:
 *  - All Modbus request methods are safe no-ops:
 *    - DoReadCoilStatus / DoReadInputStatus / DoReadHoldingRegisters / DoReadInputRegisters /
 *      DoReadGeneralReference / DoReadFIFOQueue: leave @p Data buffer unchanged.
 *    - DoForceSingleCoil / DoPresetSingleRegister / DoForceMultipleCoils /
 *      DoPresetMultipleRegisters / DoMaskWrite4XRegister / DoReadWrite4XRegisters /
 *      DoWriteGeneralReference: discarded.
 *    - DoReadExceptionStatus returns 0; DoDiagnostics echoes input data.
 *  - DoOpen() sets the internal connected flag to @c true.
 *  - DoClose() sets the internal connected flag to @c false.
 *  - DoIsConnected() returns the current flag state.
 *
 *  Use case: Exercise Modbus master business logic in unit tests without requiring
 *  physical hardware, a live network, or a Modbus slave.
 *
 *  @note No exceptions are ever thrown by DummyProtocol.
 */
class DummyProtocol : public Protocol {
public:
    /** @brief Default constructor. */
    DummyProtocol() {}
protected:
    virtual String DoGetProtocolName() const override { return _D( "Dummy Modbus" ); }
    virtual String DoGetProtocolParamsStr() const override { return String(); }
    virtual void DoOpen() noexcept override { active_ = true; }
    virtual void DoClose() noexcept override { active_ = false; }
    virtual bool DoIsConnected() const noexcept override { return active_; }

    virtual void DoReadCoilStatus( Context const & Context,
                                   CoilAddrType StartAddr,
                                   CoilCountType PointCount,
                                   CoilDataType* Data ) noexcept override {}
    virtual void DoReadInputStatus( Context const & Context,
                                    CoilAddrType StartAddr,
                                    CoilCountType PointCount,
                                    CoilDataType* Data ) noexcept override {}

    virtual void DoReadHoldingRegisters( Context const & Context,
                                         RegAddrType StartAddr,
                                         RegCountType PointCount,
                                         RegDataType* Data ) noexcept override {}
    virtual void DoReadInputRegisters( Context const & Context,
                                       RegAddrType StartAddr,
                                       RegCountType PointCount,
                                       RegDataType* Data ) noexcept override {}
    virtual void DoForceSingleCoil( Context const & Context,
                                    CoilAddrType Addr,
                                    bool Value ) noexcept override {}
    virtual void DoPresetSingleRegister( Context const & Context,
                                         RegAddrType Addr, RegDataType Data ) noexcept override {}
    virtual ExceptionStatusDataType DoReadExceptionStatus(
                                        Context const & Context ) noexcept override
    { return 0; }
    virtual RegDataType DoDiagnostics( Context const & Context,
                                       DiagSubFnType SubFunction,
                                       RegDataType Data ) noexcept override
    { return Data; }
//    DoProgram484
//    DoPoll484
//    DoFetchCommEventCtr
//    DoFetchCommEventLog
//    DoProgramController
//    DoPollController
    virtual void DoForceMultipleCoils( Context const & Context,
                                       CoilAddrType StartAddr,
                                       CoilCountType PointCount,
                                       const CoilDataType* Data ) noexcept override {}
    void DoPresetMultipleRegisters( Context const & Context,
                                    RegAddrType StartAddr,
                                    RegCountType PointCount,
                                    const RegDataType* Data ) noexcept override {}

//    DoReportSlave
//    DoProgram884_M84
//    DoResetCommLink
    virtual void DoReadGeneralReference( Context const & Context,
                                         const FileSubRequest* SubRequests,
                                         size_t SubReqCount,
                                         RegDataType* Data ) noexcept override {}
    virtual void DoWriteGeneralReference( Context const & Context,
                                          const FileSubRequest* SubRequests,
                                          size_t SubReqCount,
                                          const RegDataType* Data ) noexcept override {}
    virtual void DoMaskWrite4XRegister( Context const & Context,
                                        RegAddrType Addr,
                                        RegDataType AndMask,
                                        RegDataType OrMask ) noexcept override {}
    virtual void DoReadWrite4XRegisters( Context const & Context,
                                         RegAddrType ReadStartAddr,
                                         RegCountType ReadPointCount,
                                         RegDataType* ReadData,
                                         RegAddrType WriteStartAddr,
                                         RegCountType WritePointCount,
                                         const RegDataType* WriteData ) noexcept override {}
    virtual FIFOCountType DoReadFIFOQueue( Context const & Context,
                                          FIFOAddrType FIFOAddr,
                                          RegDataType* Data ) noexcept override
    { return 0; }
private:
    bool active_ { false };
};


//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif



