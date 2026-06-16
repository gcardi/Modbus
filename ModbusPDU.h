/**
 * @file ModbusPDU.h
 * @brief Modbus::PDU — function-code-specific PDU (Protocol Data Unit) body helpers.
 *
 * @details Shared, transport-agnostic helpers for building Modbus request PDUs and for
 *  parsing/validating response PDUs. A PDU consists of a one-byte Function Code followed by
 *  function-code-specific data; it is independent of the surrounding frame (the MBAP header
 *  for TCP/UDP, or the slave address + CRC for RTU). Extracted from TCPIPProtocol so the same
 *  logic can be reused by both Modbus::Master and a future Modbus::Server implementation.
 */

//---------------------------------------------------------------------------

#ifndef ModbusPDUH
#define ModbusPDUH

#include <cstdint>

#include "Modbus.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace PDU {
//---------------------------------------------------------------------------

/** @brief Length in bytes of an Address+PointCount field pair (FC01-FC04, FC15, FC16, FC23). */
constexpr int AddressPointCountPairLength = 4;

/** @brief Reads the Function Code byte (offset 0) from a PDU buffer. */
[[ nodiscard ]] FunctionCode GetFunctionCode( TBytes const Buffer ) noexcept;

/** @brief Reads the Exception Code byte (offset 1) from an exception response PDU. */
[[ nodiscard ]] ExceptionCode GetExceptionCode( TBytes const Buffer ) noexcept;

/** @brief Reads the Byte Count field (offset 1) from a "read" response PDU (e.g. FC01-FC04). */
[[ nodiscard ]] uint8_t GetByteCount( TBytes const Buffer ) noexcept;

/**
 * @brief Validates a response PDU against the expected function code.
 * @details Throws the appropriate EProtocolStdException if the slave returned an exception
 *  response (Function Code with bit 0x80 set), throws EContextException if the Function Code
 *  does not match @p ExpectedFunctionCode, or if @p Buffer is too short to contain a PDU.
 */
void RaiseExceptionIfReplyIsNotValid( Context const & Context,
                                      TBytes const Buffer,
                                      FunctionCode ExpectedFunctionCode );

/** @brief Writes a big-endian Starting Address + Point Count field pair, returns the next index. */
int WriteAddressPointCountPair( TBytes & OutBuffer, int StartIdx,
                                RegAddrType StartAddr, RegCountType PointCount ) noexcept;

/** @brief Writes a single big-endian 16-bit word, returns the next index. */
int WriteWord( TBytes & OutBuffer, int StartIdx, uint16_t Data ) noexcept;

/**
 * @brief Returns the number of register-data bytes following @p BufferOffset in @p Buffer.
 * @details Validates that the PDU's Byte Count field (offset 1) matches
 *  <tt>Buffer.Length - 2</tt> and that the data following @p BufferOffset has an even length.
 *  Throws EContextException if either check fails.
 */
int GetPayloadLength( Context const & Context, TBytes const Buffer, int BufferOffset );

/**
 * @brief Copies the 16-bit register words following @p BufferOffset in @p Buffer into @p Data.
 * @details The number of words copied is determined by GetPayloadLength().
 */
void CopyDataWords( Context const & Context, TBytes const Buffer,
                    int BufferOffset, uint16_t* Data );

//---------------------------------------------------------------------------
}; // End of namespace PDU
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
