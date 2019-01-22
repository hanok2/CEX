#ifndef CEX_AEADMODES_H
#define CEX_AEADMODES_H

#include "CexDomain.h"

NAMESPACE_ENUMERATION

/// <summary>
/// Symmetric AEAD cipher mode enumeration names
/// </summary>
enum class AeadModes : byte
{
	/// <summary>
	/// No cipher mode is specified
	/// </summary>
	None = 0,
	/// <summary>
	/// Encrypt and Authenticate AEAD Mode
	/// </summary>
	EAX = 5,
	/// <summary>
	/// Galois Counter AEAD Mode
	/// </summary>
	GCM = 6
};

NAMESPACE_ENUMERATIONEND
#endif
