//---------------------------------------------------------------------------

#ifndef ModbusDummyH
#define ModbusDummyH

#include "Modbus.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

class DummyProtocol : public Protocol {
public:
    DummyProtocol() {}
protected:
    virtual String DoGetProtocolName() const override { return _T( "Dummy Modbus" ); }
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



