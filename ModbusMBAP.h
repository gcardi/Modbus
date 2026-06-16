/**
 * @file ModbusMBAP.h
 * @brief Modbus::MBAP — MBAP (Modbus Application Protocol) header framing for TCP/UDP.
 *
 * @details Shared, transport-agnostic helpers for building, parsing, and validating the
 *  7-byte MBAP header (Transaction Identifier, Protocol Identifier, Length, Unit Identifier)
 *  used by Modbus TCP and UDP. Extracted from TCPIPProtocol so the same framing logic can be
 *  reused by both Modbus::Master and a future Modbus::Server implementation.
 */

//---------------------------------------------------------------------------

#ifndef ModbusMBAPH
#define ModbusMBAPH

#include <cstdint>

#include "Modbus.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace MBAP {
//---------------------------------------------------------------------------

/** @brief Length in bytes of the MBAP header (Transaction Id + Protocol Id + Length + Unit Id). */
constexpr int HeaderLength = 7;

/** @brief Type of the MBAP Transaction Identifier field. */
using TransactionIdType = Context::TransactionIdType;

/** @brief Type of the MBAP Protocol Identifier field (always 0 for Modbus). */
using ProtocolIdType = uint16_t;

/** @brief Type of the MBAP Length field (Unit Identifier + PDU byte count). */
using DataLengthType = uint16_t;

/** @brief Type of the MBAP Unit Identifier field. */
using UnitIdType = Context::SlaveAddrType;

/**
 * @brief Writes the 7-byte MBAP header into @p OutBuffer at @p StartIdx.
 * @details The Length field is computed from <tt>OutBuffer.Length - 6</tt>, so @p OutBuffer
 *  must already be sized to its final length before calling this function.
 * @param OutBuffer Destination buffer (must be at least @p StartIdx + HeaderLength bytes).
 * @param StartIdx  Index of the first byte of the header.
 * @param Context   Provides the Transaction Identifier and Unit (slave) Identifier.
 * @return Index of the first byte following the header (i.e. @p StartIdx + HeaderLength).
 */
int WriteHeader( TBytes & OutBuffer, int StartIdx, Context const & Context );

/** @brief Reads the MBAP Transaction Identifier field. */
[[ nodiscard ]] TransactionIdType GetTransactionIdentifier( TBytes const Buffer ) noexcept;

/** @brief Reads the MBAP Protocol Identifier field (0 for Modbus). */
[[ nodiscard ]] ProtocolIdType GetProtocolIdentifier( TBytes const Buffer ) noexcept;

/** @brief Reads the MBAP Length field (Unit Identifier + following PDU bytes). */
[[ nodiscard ]] DataLengthType GetDataLength( TBytes const Buffer ) noexcept;

/** @brief Reads the MBAP Unit Identifier field. */
[[ nodiscard ]] UnitIdType GetUnitIdentifier( TBytes const Buffer ) noexcept;

/** @brief Throws EContextException if @p Buffer is shorter than HeaderLength. */
void RaiseExceptionIfHeaderIsNotValid( Context const & Context, TBytes const Buffer );

/** @brief Throws EContextException if @p DataLength is outside the valid MBAP Length range [3, 254]. */
void RaiseExceptionIfDataLengthIsNotValid( Context const & Context, DataLengthType DataLength );

/**
 * @brief Validates that @p RBuffer's MBAP header is a correct reply to @p LBuffer's request.
 * @details Checks header lengths, matching Transaction Identifier, Protocol Identifier == 0,
 *  matching Unit Identifier, and that @p RBuffer's Length field is within the valid range.
 */
void RaiseExceptionIfHeadersAreNotEQ( Context const & Context, TBytes const LBuffer, TBytes const RBuffer );

//---------------------------------------------------------------------------
}; // End of namespace MBAP
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
