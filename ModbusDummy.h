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
 *    - DoReadHoldingRegisters / DoReadInputRegisters: leave @p Data buffer unchanged.
 *    - DoPresetSingleRegister / DoPresetMultipleRegisters / DoMaskWrite4XRegister: discarded.
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
    virtual void DoOpen() override { active_ = true; }
    virtual void DoClose() override { active_ = false; }
    virtual bool DoIsConnected() const override { return active_; }

//    DoReadCoilStatus
//    DoReadInputStatus

    virtual void DoReadHoldingRegisters( Context const & Context,
                                         RegAddrType StartAddr,
                                         RegCountType PointCount,
                                         RegDataType* Data ) override {}
    virtual void DoReadInputRegisters( Context const & Context,
                                       RegAddrType StartAddr,
                                       RegCountType PointCount,
                                       RegDataType* Data ) override {}
//    DoForceSingleCoil
    virtual void DoPresetSingleRegister( Context const & Context,
                                         RegAddrType Addr, RegDataType Data ) override {}
//    DoReadExceptionStatus
//    DoDiagnostics
//    DoProgram484
//    DoPoll484
//    DoFetchCommEventCtr
//    DoFetchCommEventLog
//    DoProgramController
//    DoPollController
//    DoForceMultipleCoils
//    DoPresetMultipleRegisters
    void DoPresetMultipleRegisters( Context const & Context,
                                    RegAddrType StartAddr,
                                    RegCountType PointCount,
                                    const RegDataType* Data ) override {}

//    DoReportSlave
//    DoProgram884_M84
//    DoResetCommLink
//    DoReadGeneralReference
//    DoWriteGeneralReference
//    DoMaskWrite4XRegister
    virtual void DoMaskWrite4XRegister( Context const & Context,
                                        RegAddrType Addr,
                                        RegDataType AndMask,
                                        RegDataType OrMask ) override {}
//    DoReadWrite4XRegisters
//    DoReadFIFOQueue
private:
    bool active_ { false };
};


//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif



