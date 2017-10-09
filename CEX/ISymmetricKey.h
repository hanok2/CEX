#ifndef CEX_ISYMMETRICKEY_H
#define CEX_ISYMMETRICKEY_H

#include "CexDomain.h"
#include "CryptoProcessingException.h"
#include "MemoryStream.h"
#include "SymmetricKeySize.h"

NAMESPACE_SYMMETRICKEY

using Exception::CryptoProcessingException;
using IO::MemoryStream;

/// <summary>
/// Symmetric Key virtual interface class.
/// <para>Provides virtual interfaces for the symmetric key classes.</para>
/// </summary>
class ISymmetricKey
{
public:

	//~~~Constructor~~~//

	/// <summary>
	/// Initialize the ISymmetricKey virtual interface class
	/// </summary>
	ISymmetricKey() {}

	/// <summary>
	/// Finalize objects
	/// </summary>
	virtual ~ISymmetricKey() noexcept {}

	//~~~Properties~~~//

	/// <summary>
	/// Get: The primary key
	/// </summary>
	virtual const std::vector<byte> Key() = 0;

	/// <summary>
	/// Get: The SymmetricKeySize containing the byte sizes of the key, nonce, and info state members
	/// </summary>
	virtual const SymmetricKeySize KeySizes() = 0;

	/// <summary>
	/// Get: The nonce or initialization vector
	/// </summary>
	virtual const std::vector<byte> Nonce() = 0;

	/// <summary>
	/// Get/Set: The personalization string; can used as an optional source of entropy
	/// </summary>
	virtual const std::vector<byte> Info() = 0;

	//~~~Public Functions~~~//

	/// <summary>
	/// Release all resources associated with the object; optional, called by the finalizer
	/// </summary>
	virtual void Destroy() = 0;

	/// <summary>
	/// Compare this Key instance with another
	/// </summary>
	/// 
	/// <param name="Input">Key to compare</param>
	/// 
	/// <returns>Returns true if equal</returns>
	virtual bool Equals(ISymmetricKey &Input) = 0;
};

NAMESPACE_SYMMETRICKEYEND
#endif
