#include "SymmetricSecureKey.h"
#include "ArrayUtils.h"
#include "CTR.h"
#include "SHA512.h"
#include "StreamWriter.h"
#include "StreamReader.h"
#include "SymmetricKey.h"
#include "SysUtils.h"

NAMESPACE_KEYSYMMETRIC

//~~~Public Functions~~~//

SymmetricSecureKey* SymmetricSecureKey::Clone()
{
	return new SymmetricSecureKey(Key(), Nonce(), Info());
}

void SymmetricSecureKey::Destroy()
{
	if (!m_isDestroyed)
	{
		if (m_keyState.size() > 0)
			Utility::ArrayUtils::ClearVector(m_keyState);
		if (m_keySalt.size() > 0)
			Utility::ArrayUtils::ClearVector(m_keySalt);

		m_isDestroyed = true;
	}
}

SymmetricSecureKey* SymmetricSecureKey::DeSerialize(MemoryStream &KeyStream)
{
	IO::StreamReader reader(KeyStream);
	short keyLen = reader.ReadInt16();
	short nonceLen = reader.ReadInt16();
	short infoLen = reader.ReadInt16();
	std::vector<byte> key;
	std::vector<byte> nonce;
	std::vector<byte> info;

	if (keyLen > 0)
		key = reader.ReadBytes(keyLen);
	if (nonceLen > 0)
		nonce = reader.ReadBytes(nonceLen);
	if (infoLen > 0)
		info = reader.ReadBytes(infoLen);

	return new SymmetricSecureKey(key, nonce, info);
}

bool SymmetricSecureKey::Equals(ISymmetricKey &Obj)
{
	return (Obj.Key() == Key() && Obj.Nonce() == Nonce() && Obj.Info() == Info());
}

MemoryStream* SymmetricSecureKey::Serialize(SymmetricSecureKey &KeyObj)
{
	short klen = (short)KeyObj.Key().size();
	short vlen = (short)KeyObj.Nonce().size();
	short mlen = (short)KeyObj.Info().size();
	int len = 6 + klen + vlen + mlen;

	IO::StreamWriter writer(len);
	writer.Write(klen);
	writer.Write(vlen);
	writer.Write(mlen);

	if (KeyObj.Key().size() != 0)
		writer.Write(KeyObj.Key());
	if (KeyObj.Nonce().size() != 0)
		writer.Write(KeyObj.Nonce());
	if (KeyObj.Info().size() != 0)
		writer.Write(KeyObj.Info());

	IO::MemoryStream* strm = writer.GetStream();
	strm->Seek(0, IO::SeekOrigin::Begin);

	return strm;
}

//~~~Private Functions~~~//

std::vector<byte> SymmetricSecureKey::Extract(size_t Offset, size_t Length)
{
	Transform();
	std::vector<byte> state(Length);
	memcpy(&state[0], &m_keyState[Offset], Length);
	Transform();

	return state;
}

std::vector<byte> SymmetricSecureKey::GetSystemKey()
{
	std::vector<byte> state(0);
	Utility::ArrayUtils::Append(Utility::SysUtils::ComputerName(), state);
	Utility::ArrayUtils::Append(Utility::SysUtils::OsName(), state);
	Utility::ArrayUtils::Append(Utility::SysUtils::ProcessId(), state);
	Utility::ArrayUtils::Append(Utility::SysUtils::UserId(), state);
	Utility::ArrayUtils::Append(Utility::SysUtils::UserName(), state);

	if (m_keySalt.size() != 0)
		Utility::ArrayUtils::Append(m_keySalt, state);

	Digest::SHA512 dgt;
	std::vector<byte> hash(dgt.DigestSize());
	dgt.Compute(state, hash);

	return hash;
}

void SymmetricSecureKey::Transform()
{
	std::vector<byte> seed = GetSystemKey();
	std::vector<byte> key(32);
	std::vector<byte> iv(16);

	memcpy(&key[0], &seed[0], key.size());
	memcpy(&iv[0], &seed[key.size()], iv.size());
	SymmetricKey kp(key, iv);

	// AES256-CTR
	Cipher::Symmetric::Block::Mode::CTR cpr(Enumeration::BlockCiphers::Rijndael);
	cpr.Initialize(true, kp);
	std::vector<byte> state(m_keyState.size());
	cpr.Transform(m_keyState, state);
	m_keyState = state;
}

NAMESPACE_KEYSYMMETRICEND