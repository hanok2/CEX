#include "KMAC.h"
#include "ArrayTools.h"
#include "IntegerTools.h"
#include "MemoryTools.h"
#include "Keccak.h"

NAMESPACE_MAC

using Utility::ArrayTools;
using Utility::IntegerTools;
using Utility::MemoryTools;

const std::string KMAC::CLASS_NAME("KMAC");

//~~~Constructor~~~//

KMAC::KMAC(ShakeModes ShakeModeType)
	:
	m_blockSize((ShakeModeType == ShakeModes::SHAKE128) ? 168 : (ShakeModeType == ShakeModes::SHAKE256) ? 136 : 72),
	m_distCode { 0x4B, 0x4D, 0x41, 0x43 },
	m_isDestroyed(false),
	m_isInitialized(false),
	m_legalKeySizes(0),
	m_macSize((ShakeModeType == ShakeModes::SHAKE128) ? 16 : (ShakeModeType == ShakeModes::SHAKE256) ? 32 :
		(ShakeModeType == ShakeModes::SHAKE512) ? 64 : 128),
	m_msgLength(0),
	m_shakeMode(ShakeModeType != ShakeModes::None ? ShakeModeType :
		throw CryptoMacException(CLASS_NAME, std::string("Constructor"), std::string("The SHAKE mode type can not ne none!"), ErrorCodes::InvalidParam))
{
	Scope();
}

KMAC::~KMAC()
{
	if (!m_isDestroyed)
	{
		m_blockSize = 0;
		m_isDestroyed = true;
		m_isInitialized = false;
		m_macSize = 0;
		m_msgLength = 0;
		m_shakeMode = ShakeModes::None;

		IntegerTools::Clear(m_distCode);
		IntegerTools::Clear(m_legalKeySizes);
		IntegerTools::Clear(m_msgBuffer);
	}
}

//~~~Accessors~~~//

const size_t KMAC::BlockSize()
{
	return m_blockSize;
}

std::vector<byte> &KMAC::DistributionCode()
{
	return m_distCode;
}

const size_t KMAC::DistributionCodeMax()
{
	return m_blockSize;
}

const Macs KMAC::Enumeral()
{
	Macs mname;

	if (m_shakeMode == ShakeModes::SHAKE1024)
	{ 
		mname = Macs::KMAC1024;
	}
	else if (m_shakeMode == ShakeModes::SHAKE512)
	{
		mname = Macs::KMAC512;
	}
	else if (m_shakeMode == ShakeModes::SHAKE256)
	{
		mname = Macs::KMAC256;
	}
	else
	{
		mname = Macs::KMAC128;
	}

	return mname;
}

const bool KMAC::IsInitialized()
{
	return m_isInitialized;
}

std::vector<SymmetricKeySize> KMAC::LegalKeySizes() const
{
	return m_legalKeySizes;
}

const std::string KMAC::Name()
{
	return  CLASS_NAME + "-" + IntegerTools::ToString(m_macSize * 8);
}

const ShakeModes KMAC::ShakeMode()
{
	return m_shakeMode;
}

const size_t KMAC::TagSize()
{
	return m_macSize;
}

//~~~Public Functions~~~//

void KMAC::Compute(const std::vector<byte> &Input, std::vector<byte> &Output)
{
	if (!m_isInitialized)
	{
		throw CryptoMacException(Name(), std::string("Compute"), std::string("The MAC has not been initialized!"), ErrorCodes::NotInitialized);
	}
	if (Output.size() < TagSize())
	{
		throw CryptoMacException(Name(), std::string("Compute"), std::string("The Output buffer is too short!"), ErrorCodes::InvalidSize);
	}

	Update(Input, 0, Input.size());
	Finalize(Output, 0);
}

size_t KMAC::Finalize(std::vector<byte> &Output, size_t OutOffset)
{
	if (!m_isInitialized)
	{
		throw CryptoMacException(Name(), std::string("Finalize"), std::string("The MAC has not been initialized!"), ErrorCodes::NotInitialized);
	}
	if ((Output.size() - OutOffset) < TagSize())
	{
		throw CryptoMacException(Name(), std::string("Finalize"), std::string("The Output buffer is too short!"), ErrorCodes::InvalidSize);
	}

	std::vector<byte> buf(sizeof(size_t) + 1);
	size_t i;
	size_t outBits;
	ulong outLen;

	if (m_msgLength != m_msgBuffer.size())
	{
		MemoryTools::Clear(m_msgBuffer, m_msgLength, m_msgBuffer.size() - m_msgLength);
	}

	outLen = Output.size() - OutOffset;
	outBits = ArrayTools::RightEncode(buf, 0, outLen * 8);

	for (i = 0; i < outBits; i++)
	{
		m_msgBuffer[m_msgLength + i] = buf[i];
	}

	m_msgLength += outBits;
	m_msgBuffer[m_msgLength] = DOMAIN_CODE;
	m_msgBuffer[m_blockSize - 1] |= 128;

	ArrayTools::AbsorbBlock8to64(m_msgBuffer, 0, m_kdfState, m_blockSize);
	Squeeze(m_kdfState, Output, OutOffset, static_cast<size_t>(outLen));

	return outLen;
}

void KMAC::Initialize(ISymmetricKey &KeyParams)
{
	if (KeyParams.Key().size() < MIN_KEYSIZE)
	{
		throw CryptoMacException(Name(), std::string("Initialize"), std::string("Key size is invalid; must be a legal key size!"), ErrorCodes::InvalidKey);
	}
	if (KeyParams.Info().size() > m_blockSize)
	{
		throw CryptoMacException(Name(), std::string("Initialize"), std::string("Info size is invalid; must be a legal info size!"), ErrorCodes::InvalidKey);
	}

	size_t keyLen = KeyParams.Key().size();

	if (m_isInitialized)
	{
		Reset();
	}

	if (KeyParams.Info().size() > 0)
	{
		m_distCode = KeyParams.Info();
	}

	Customize(KeyParams.Nonce(), m_distCode);
	LoadKey(KeyParams.Key());

	m_isInitialized = true;
}

void KMAC::Reset()
{
	MemoryTools::Clear(m_kdfState, 0, STATE_SIZE * sizeof(ulong));
	MemoryTools::Clear(m_msgBuffer, 0, BUFFER_SIZE);
	m_msgLength = 0;
	m_isInitialized = false;
}

void KMAC::Update(byte Input)
{
	std::vector<byte> one(1, Input);
	Update(one, 0, 1);
}

void KMAC::Update(const std::vector<byte> &Input, size_t InOffset, size_t Length)
{
	CEXASSERT(Input.size() - InOffset >= Length, "The input buffer is too short!");
	CEXASSERT(m_isInitialized, "The mac is not initialized!");

	if (Length != 0)
	{
		if (m_msgLength != 0 && (m_msgLength + Length >= m_blockSize))
		{
			const size_t RMDLEN = m_blockSize - m_msgLength;
			if (RMDLEN != 0)
			{
				MemoryTools::Copy(Input, InOffset, m_msgBuffer, m_msgLength, RMDLEN);
			}

			ArrayTools::AbsorbBlock8to64(m_msgBuffer, 0, m_kdfState, m_blockSize);
			Permute(m_kdfState);
			m_msgLength = 0;
			InOffset += RMDLEN;
			Length -= RMDLEN;
		}

		// sequential loop through blocks
		while (Length >= m_blockSize)
		{
			ArrayTools::AbsorbBlock8to64(Input, InOffset, m_kdfState, m_blockSize);
			Permute(m_kdfState);
			InOffset += m_blockSize;
			Length -= m_blockSize;
		}

		// store unaligned bytes
		if (Length != 0)
		{
			MemoryTools::Copy(Input, InOffset, m_msgBuffer, m_msgLength, Length);
			m_msgLength += Length;
		}
	}
}

//~~~Private Functions~~~//

void KMAC::Customize(const std::vector<byte> &Customization, const std::vector<byte> &Name)
{
	CEXASSERT(!m_isInitialized, "The domain string must be set before initialization");
	CEXASSERT(Customization.size() + Name.size() <= 196, "The input buffer is too large");

	std::array<byte, BUFFER_SIZE> pad;
	size_t i;
	ulong offset;

	MemoryTools::Clear(pad, 0, pad.size());
	offset = ArrayTools::LeftEncode(pad, 0, static_cast<ulong>(m_blockSize));
	offset += ArrayTools::LeftEncode(pad, offset, static_cast<ulong>(Name.size() * 8));

	if (Name.size() != 0)
	{
		for (i = 0; i < Name.size(); i++)
		{
			if (offset == m_blockSize)
			{
				for (size_t i = 0; i < BUFFER_SIZE; i += 8)
				{
					m_kdfState[i / 8] ^= IntegerTools::LeBytesTo64(pad, i);
				}

				Permute(m_kdfState);
				offset = 0;
			}

			pad[offset] = Name[i];
			++offset;
		}
	}

	offset += ArrayTools::LeftEncode(pad, offset, static_cast<ulong>(Customization.size() * 8));

	if (Customization.size() != 0)
	{
		for (i = 0; i < Customization.size(); ++i)
		{
			if (offset == m_blockSize)
			{
				for (size_t i = 0; i < BUFFER_SIZE; i += 8)
				{
					m_kdfState[i / 8] ^= IntegerTools::LeBytesTo64(pad, i);
				}

				Permute(m_kdfState);
				offset = 0;
			}

			pad[offset] = Customization[i];
			++offset;
		}
	}

	MemoryTools::Clear(pad, offset, BUFFER_SIZE - offset);
	offset = (offset % sizeof(ulong) == 0) ? offset : offset + (sizeof(ulong) - (offset % sizeof(ulong)));

	for (size_t i = 0; i < offset; i += 8)
	{
		m_kdfState[i / 8] ^= IntegerTools::LeBytesTo64(pad, i);
	}

	Permute(m_kdfState);
}

void KMAC::LoadKey(const std::vector<byte> &Key)
{
	CEXASSERT(!m_isInitialized, "The domain string must be set before initialization");

	std::array<byte, BUFFER_SIZE> pad;
	size_t i;
	ulong offset;

	MemoryTools::Clear(pad, 0, pad.size());
	offset = ArrayTools::LeftEncode(pad, 0, static_cast<ulong>(m_blockSize));
	offset += ArrayTools::LeftEncode(pad, offset, static_cast<ulong>(Key.size() * 8));

	if (Key.size() != 0)
	{
		for (i = 0; i < Key.size(); i++)
		{
			if (offset == m_blockSize)
			{
				for (size_t i = 0; i < BUFFER_SIZE; i += 8)
				{
					m_kdfState[i / 8] ^= IntegerTools::LeBytesTo64(pad, i);
				}

				Permute(m_kdfState);
				offset = 0;
			}

			pad[offset] = Key[i];
			++offset;
		}
	}

	MemoryTools::Clear(pad, offset, BUFFER_SIZE - offset);
	offset = (offset % sizeof(ulong) == 0) ? offset : offset + (sizeof(ulong) - (offset % sizeof(ulong)));

	for (size_t i = 0; i < offset; i += 8)
	{
		m_kdfState[i / 8] ^= IntegerTools::LeBytesTo64(pad, i);
	}

	Permute(m_kdfState);
}

void KMAC::Permute(std::array<ulong, 25> &State)
{
	if (m_shakeMode != ShakeModes::SHAKE1024)
	{
		Digest::Keccak::PermuteR24P1600U(State);
	}
	else
	{
		Digest::Keccak::PermuteR48P1600U(State);
	}
}

void KMAC::Scope()
{
	Reset();

	m_legalKeySizes.resize(3);
	// minimum security is half the digest output size
	m_legalKeySizes[0] = SymmetricKeySize((m_macSize / 2), 0, 0);
	// best perf/sec mix, the digest output size
	m_legalKeySizes[1] = SymmetricKeySize(m_macSize, 0, 0);
	// max recommended key input; is two times the digest output size
	m_legalKeySizes[2] = SymmetricKeySize(m_macSize * 2, 0, 0);
}

void KMAC::Squeeze(std::array<ulong, 25> &State, std::vector<byte> &Output, size_t OutOffset, size_t Length)
{
	size_t i;
	
	while (Length > m_blockSize)
	{
		Permute(State);

		for (i = 0; i < m_blockSize / 8; ++i)
		{
			IntegerTools::Le64ToBytes(State[i], Output, OutOffset + (i * 8));
		}

		OutOffset += m_blockSize;
		Length -= m_blockSize;
	}

	if (Length > 0)
	{
		Permute(State);

		for (i = 0; i < Length / 8; ++i)
		{
			IntegerTools::Le64ToBytes(State[i], Output, OutOffset + (i * 8));
		}

		Length -= i * 8;

		if (Length > 0)
		{
			MemoryTools::CopyFromValue(State[i], Output, OutOffset + (i * 8), Length);
		}
	}
}

NAMESPACE_MACEND
