#include "HKDF.h"
#include "DigestFromName.h"
#include "IntegerTools.h"
#include "SymmetricKey.h"

NAMESPACE_KDF

const std::string HKDF::CLASS_NAME("HKDF");

//~~~Constructor~~~//

HKDF::HKDF(SHA2Digests DigestType)
	:
	m_macGenerator(DigestType != SHA2Digests::None ? new HMAC(DigestType) :
		throw CryptoKdfException(CLASS_NAME, std::string("Constructor"), std::string("The digest type is not supported!"), ErrorCodes::InvalidParam)),
	m_blockSize(m_macGenerator->BlockSize()),
	m_destroyEngine(true),
	m_isDestroyed(false),
	m_isInitialized(false),
	m_kdfCounter(0),
	m_kdfDigestType(static_cast<Digests>(DigestType)),
	m_kdfInfo(0),
	m_kdfState(m_macGenerator->TagSize()),
	m_legalKeySizes(0),
	m_macSize(m_macGenerator->TagSize())
{
	LoadState();
}

HKDF::HKDF(IDigest* Digest)
	:
	m_macGenerator(Digest->Enumeral() == Digests::SHA256 || Digest->Enumeral() == Digests::SHA512 ? new HMAC(Digest) :
		throw CryptoKdfException(CLASS_NAME, std::string("Constructor"), std::string("The digest can not be null!"), ErrorCodes::IllegalOperation)),
	m_blockSize(m_macGenerator->BlockSize()),
	m_destroyEngine(false),
	m_isDestroyed(false),
	m_isInitialized(false),
	m_kdfCounter(0),
	m_kdfDigestType(m_macGenerator->DigestType()),
	m_kdfInfo(0),
	m_kdfState(m_macGenerator->TagSize()),
	m_legalKeySizes(0),
	m_macSize(m_macGenerator->TagSize())
{
	LoadState();
}

HKDF::HKDF(HMAC* Mac)
	:
	m_macGenerator(Mac != nullptr ? Mac :
		throw CryptoKdfException(CLASS_NAME, std::string("Constructor"), std::string("The mac generator can not be null!"), ErrorCodes::IllegalOperation)),
	m_blockSize(m_macGenerator->BlockSize()),
	m_destroyEngine(false),
	m_isDestroyed(false),
	m_isInitialized(false),
	m_kdfCounter(0),
	m_kdfDigestType(m_macGenerator->DigestType()),
	m_kdfState(m_macGenerator->TagSize()),
	m_legalKeySizes(0),
	m_macSize(m_macGenerator->TagSize())
{
	LoadState();
}

HKDF::~HKDF()
{
	if (!m_isDestroyed)
	{
		m_isDestroyed = true;
		m_blockSize = 0;
		m_macSize = 0;
		m_isInitialized = false;
		m_kdfCounter = 0;
		m_kdfDigestType = Digests::None;

		Utility::IntegerTools::Clear(m_kdfInfo);
		Utility::IntegerTools::Clear(m_kdfState);
		Utility::IntegerTools::Clear(m_legalKeySizes);

		if (m_destroyEngine)
		{
			m_destroyEngine = false;

			if (m_macGenerator != 0)
			{
				m_macGenerator.reset(nullptr);
			}
		}
		else
		{
			if (m_macGenerator != nullptr)
			{
				m_macGenerator.release();
			}
		}
	}
}

//~~~Accessors~~~//

const Kdfs HKDF::Enumeral() 
{ 
	return Kdfs::HKDF256; 
}

std::vector<byte> &HKDF::Info() 
{ 
	return m_kdfInfo; 
}

const bool HKDF::IsInitialized() 
{ 
	return m_isInitialized; 
}

std::vector<SymmetricKeySize> HKDF::LegalKeySizes() const 
{ 
	return m_legalKeySizes; 
};

const size_t HKDF::MinKeySize() 
{
	return m_macSize; 
}

const std::string HKDF::Name()
{
	return CLASS_NAME + "-" + m_macGenerator->Name();
}

//~~~Public Functions~~~//

size_t HKDF::Generate(std::vector<byte> &Output)
{
	if (!m_isInitialized)
	{
		throw CryptoKdfException(Name(), std::string("Generate"), std::string("The generator has not been initialized!"), ErrorCodes::NotInitialized);
	}
	if (m_kdfCounter + (Output.size() / m_macSize) > 255)
	{
		throw CryptoKdfException(Name(), std::string("Generate"), std::string("Request exceeds maximum allowed output!"), ErrorCodes::MaxExceeded);
	}

	return Expand(Output, 0, Output.size());
}

size_t HKDF::Generate(std::vector<byte> &Output, size_t OutOffset, size_t Length)
{
	if (!m_isInitialized)
	{
		throw CryptoKdfException(Name(), std::string("Generate"), std::string("The generator has not been initialized!"), ErrorCodes::NotInitialized);
	}
	if (m_kdfCounter + (Output.size() / m_macSize) > 255)
	{
		throw CryptoKdfException(Name(), std::string("Generate"), std::string("Request exceeds maximum allowed output!"), ErrorCodes::MaxExceeded);
	}
	if (Output.size() - OutOffset < Length)
	{
		throw CryptoKdfException(Name(), std::string("Generate"), std::string("The output buffer is too short!"), ErrorCodes::InvalidSize);
	}

	return Expand(Output, OutOffset, Length);
}

void HKDF::Initialize(ISymmetricKey &GenParam)
{
	if (GenParam.Nonce().size() != 0)
	{
		if (GenParam.Info().size() != 0)
		{
			Initialize(GenParam.Key(), GenParam.Nonce(), GenParam.Info());
		}
		else
		{
			Initialize(GenParam.Key(), GenParam.Nonce());
		}
	}
	else
	{
		Initialize(GenParam.Key());
	}
}

void HKDF::Initialize(const std::vector<byte> &Key)
{
	if (Key.size() < MIN_KEYLEN)
	{
		throw CryptoKdfException(Name(), std::string("Initialize"), std::string("Key value is too small, must be at least 16 bytes in length!"), ErrorCodes::InvalidKey);
	}

	if (m_isInitialized)
	{
		Reset();
	}

	Cipher::SymmetricKey kp(Key);
	m_macGenerator->Initialize(kp);
	m_isInitialized = true;
}

void HKDF::Initialize(const std::vector<byte> &Key, size_t Offset, size_t Length)
{
	CEXASSERT(Key.size() >= Length + Offset, "The key is too small");

	std::vector<byte> tmpK(Length);

	Utility::MemoryTools::Copy(Key, Offset, tmpK, 0, Length);
	Initialize(tmpK);
}

void HKDF::Initialize(const std::vector<byte> &Key, const std::vector<byte> &Salt)
{
	if (Key.size() < MIN_KEYLEN)
	{
		throw CryptoKdfException(Name(), std::string("Initialize"), std::string("Key value is too small, must be at least 16 bytes in length!"), ErrorCodes::InvalidKey);
	}
	if (Salt.size() != 0 && Salt.size() < MIN_SALTLEN)
	{
		throw CryptoKdfException(Name(), std::string("Initialize"), std::string("Salt value is too small, must be at least 4 bytes in length!"), ErrorCodes::InvalidSalt);
	}

	if (m_isInitialized)
	{
		Reset();
	}

	std::vector<byte> prk;
	Extract(Key, Salt, prk);
	Cipher::SymmetricKey kp(prk);
	m_macGenerator->Initialize(kp);
	m_isInitialized = true;
}

void HKDF::Initialize(const std::vector<byte> &Key, const std::vector<byte> &Salt, const std::vector<byte> &Info)
{
	if (Key.size() < MIN_KEYLEN)
	{
		throw CryptoKdfException(Name(), std::string("Initialize"), std::string("Key value is too small, must be at least 16 bytes in length!"), ErrorCodes::InvalidKey);
	}
	if (Salt.size() != 0 && Salt.size() < MIN_SALTLEN)
	{
		throw CryptoKdfException(Name(), std::string("Initialize"), std::string("Salt value is too small, must be at least 4 bytes in length!"), ErrorCodes::InvalidSalt);
	}

	if (m_isInitialized)
	{
		Reset();
	}

	if (Salt.size() != 0)
	{
		std::vector<byte> prk(m_macSize);
		Extract(Key, Salt, prk);
		Cipher::SymmetricKey kp(prk);
		m_macGenerator->Initialize(kp);
	}
	else
	{
		Cipher::SymmetricKey kp(Key);
		m_macGenerator->Initialize(kp);
	}

	m_kdfInfo = Info;
	m_isInitialized = true;
}

void HKDF::ReSeed(const std::vector<byte> &Seed)
{
	Initialize(Seed);
}

void HKDF::Reset()
{
	m_kdfCounter = 0;
	m_kdfInfo.clear();
	m_kdfState.clear();
	m_kdfState.resize(m_macSize);
	m_isInitialized = false;
}

//~~~Private Functions~~~//

size_t HKDF::Expand(std::vector<byte> &Output, size_t OutOffset, size_t Length)
{
	size_t prcLen = 0;

	while (prcLen != Length)
	{
		if (m_kdfCounter != 0)
		{
			m_macGenerator->Update(m_kdfState, 0, m_kdfState.size());
		}

		if (m_kdfInfo.size() != 0)
		{
			m_macGenerator->Update(m_kdfInfo, 0, m_kdfInfo.size());
		}

		++m_kdfCounter;
		m_macGenerator->Update(m_kdfCounter);
		m_macGenerator->Finalize(m_kdfState, 0);

		const size_t RMDLEN = Utility::IntegerTools::Min(m_macSize, Length - prcLen);
		Utility::MemoryTools::Copy(m_kdfState, 0, Output, OutOffset, RMDLEN);
		prcLen += RMDLEN;
		OutOffset += RMDLEN;
	}

	return Length;
}

void HKDF::Extract(const std::vector<byte> &Key, const std::vector<byte> &Salt, std::vector<byte> &Output)
{
	Cipher::SymmetricKey kp(Key);
	m_macGenerator->Initialize(kp);

	if (Salt.size() != 0)
	{
		Cipher::SymmetricKey kps(Salt);
		m_macGenerator->Initialize(kps);
	}
	else
	{
		Cipher::SymmetricKey kps(std::vector<byte>(m_macSize, 0));
		m_macGenerator->Initialize(kps);
	}

	m_macGenerator->Update(Key, 0, Key.size());
	m_macGenerator->Finalize(Output, 0);
}

void HKDF::LoadState()
{
	// best info size is half of block size
	size_t infoLen = m_macSize;
	m_legalKeySizes.resize(3);
	// minimum security is the digest output size
	m_legalKeySizes[0] = SymmetricKeySize(m_macSize, 0, 0);
	// best security, adjusted info size to hash full blocks in generate
	m_legalKeySizes[1] = SymmetricKeySize(m_blockSize, 0, infoLen);
	// max key input; add a block of key to salt (triggers extract)
	m_legalKeySizes[2] = SymmetricKeySize(m_blockSize, m_blockSize, infoLen);
}

NAMESPACE_KDFEND
