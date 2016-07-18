#include "Skein256.h"
#include "IntUtils.h"

NAMESPACE_DIGEST

void Skein256::BlockUpdate(const std::vector<byte> &Input, size_t InOffset, size_t Length)
{
#if defined(ENABLE_CPPEXCEPTIONS)
	if ((InOffset + Length) > Input.size())
		throw CryptoDigestException("Skein256:BlockUpdate", "The Input buffer is too short!");
#endif

	size_t bytesDone = 0;

	// fill input buffer
	while (bytesDone < Length && InOffset < Input.size())
	{
		// do a transform if the input buffer is filled
		if (m_bytesFilled == STATE_BYTES)
		{
			// moves the byte input buffer to the UInt64 cipher input
			for (int i = 0; i < STATE_WORDS; i++)
				m_cipherInput[i] = CEX::Utility::IntUtils::BytesToLe64(m_inputBuffer, i * 8);

			// process the block
			ProcessBlock(STATE_BYTES);
			// clear first flag, which will be set by Initialize() if this is the first transform
			m_ubiParameters.SetIsFirstBlock(false);
			// reset buffer fill count
			m_bytesFilled = 0;
		}

		m_inputBuffer[m_bytesFilled++] = Input[InOffset++];
		bytesDone++;
	}
}

void Skein256::ComputeHash(const std::vector<byte> &Input, std::vector<byte> &Output)
{
	Output.resize(DIGEST_SIZE);
	BlockUpdate(Input, 0, Input.size());
	DoFinal(Output, 0);
	Reset();
}

void Skein256::Destroy()
{
	if (!m_isDestroyed)
	{
		m_isDestroyed = true;
		m_bytesFilled = 0;
		m_blockCipher.Clear();
		m_ubiParameters.Clear();

		CEX::Utility::IntUtils::ClearVector(m_cipherInput);
		CEX::Utility::IntUtils::ClearVector(m_configString);
		CEX::Utility::IntUtils::ClearVector(m_configValue);
		CEX::Utility::IntUtils::ClearVector(m_digestState);
		CEX::Utility::IntUtils::ClearVector(m_inputBuffer);
	}
}

size_t Skein256::DoFinal(std::vector<byte> &Output, const size_t OutOffset)
{
#if defined(ENABLE_CPPEXCEPTIONS)
	if (Output.size() - OutOffset < DIGEST_SIZE)
		throw CryptoDigestException("Skein256:DoFinal", "The Output buffer is too short!");
#endif

	// pad left over space in input buffer with zeros
	for (size_t i = m_bytesFilled; i < m_inputBuffer.size(); i++)
		m_inputBuffer[i] = 0;
	// copy to cipher input buffer
	for (size_t i = 0; i < STATE_WORDS; i++)
		m_cipherInput[i] = CEX::Utility::IntUtils::BytesToLe64(m_inputBuffer, i * 8);

	// process final message block
	m_ubiParameters.SetIsFinalBlock(true);
	ProcessBlock((uint)m_bytesFilled);
	// clear cipher input
	std::fill(m_cipherInput.begin(), m_cipherInput.end(), 0);
	// do output block counter mode output 
	std::vector<byte> hash(STATE_OUTPUT, 0);
	std::vector<ulong> oldState(STATE_WORDS);

	// save old state
	for (size_t j = 0; j < m_digestState.size(); j++)
		oldState[j] = m_digestState[j];

	for (size_t i = 0; i < STATE_OUTPUT; i += STATE_BYTES)
	{
		m_ubiParameters.StartNewBlockType((SkeinUbiType)Out);
		m_ubiParameters.SetIsFinalBlock(true);
		ProcessBlock(8);

		// output a chunk of the hash
		size_t outputSize = STATE_OUTPUT - i;
		if (outputSize > STATE_BYTES)
			outputSize = STATE_BYTES;

		PutBytes(m_digestState, hash, i, outputSize);
		// restore old state
		for (size_t j = 0; j < m_digestState.size(); j++)
			m_digestState[j] = oldState[j];

		// Increment counter
		m_cipherInput[0]++;
	}

	memcpy(&Output[OutOffset], &hash[0], hash.size());

	return hash.size();
}

void Skein256::GenerateConfiguration(std::vector<ulong> InitialState)
{
	Threefish256 cipher;
	SkeinUbiTweak tweak;

	// initialize the tweak value
	tweak.StartNewBlockType((SkeinUbiType)Config);
	tweak.SetIsFinalBlock(true);
	tweak.SetBitsProcessed(32);

	cipher.SetKey(InitialState);
	cipher.SetTweak(tweak.GetTweak());
	cipher.Encrypt(m_configString, m_configValue);

	m_configValue[0] ^= m_configString[0];
	m_configValue[1] ^= m_configString[1];
	m_configValue[2] ^= m_configString[2];
}

void Skein256::Initialize(SkeinStateType InitializationType)
{
	m_initializationType = InitializationType;

	switch (InitializationType)
	{
	case SkeinStateType::Normal:
		{
			// normal initialization
			Initialize();
			return;
		}
		case SkeinStateType::ZeroedState:
		{
			// copy the configuration value to the state
			for (size_t i = 0; i < m_digestState.size(); i++)
				m_digestState[i] = 0;
			break;
		}
		case SkeinStateType::ChainedConfig:
		{
			// generate a chained configuration
			GenerateConfiguration(m_digestState);
			// continue initialization
			Initialize();
			return;
		}
		case SkeinStateType::ChainedState:// keep the state as it is and do nothing
			break;
	}

	// reset bytes filled
	m_bytesFilled = 0;
}

void Skein256::Reset()
{
	Initialize();
}

void Skein256::SetMaxTreeHeight(const byte Height)
{
#if defined(ENABLE_CPPEXCEPTIONS)
	if (Height == 1)
		throw CryptoDigestException("Skein256:SetMaxTreeHeight", "Tree height must be zero or greater than 1.");
#endif

	m_configString[2] &= ~((ulong)0xff << 16);
	m_configString[2] |= (ulong)Height << 16;
}

void Skein256::SetSchema(const std::vector<byte> &Schema)
{
#if defined(ENABLE_CPPEXCEPTIONS)
	if (Schema.size() != 4)
		throw CryptoDigestException("Skein256:SetSchema", "Schema must be 4 bytes.");
#endif

	ulong n = m_configString[0];

	// clear the schema bytes
	n &= ~(ulong)0xfffffffful;
	// set schema bytes
	n |= (ulong)Schema[3] << 24;
	n |= (ulong)Schema[2] << 16;
	n |= (ulong)Schema[1] << 8;
	n |= (ulong)Schema[0];

	m_configString[0] = n;
}

void Skein256::SetTreeFanOutSize(const byte Size)
{
	m_configString[2] &= ~((ulong)0xff << 8);
	m_configString[2] |= (ulong)Size << 8;
}

void Skein256::SetTreeLeafSize(const byte Size)
{
	m_configString[2] &= ~(ulong)0xff;
	m_configString[2] |= (ulong)Size;
}

void Skein256::SetVersion(const uint Version)
{
#if defined(ENABLE_CPPEXCEPTIONS)
	if (Version > 3)
		throw CryptoDigestException("Skein256:SetVersion", "Version must be between 0 and 3, inclusive.");
#endif

	m_configString[0] &= ~((ulong)0x03 << 32);
	m_configString[0] |= (ulong)Version << 32;
}

void Skein256::Update(byte Input)
{
	std::vector<byte> one(1, Input);
	BlockUpdate(one, 0, 1);
}

// *** Protected Methods *** //

void Skein256::GenerateConfiguration()
{
	// default generation function>
	Threefish256 cipher;
	SkeinUbiTweak tweak;

	// initialize the tweak value
	tweak.StartNewBlockType((SkeinUbiType)Config);
	tweak.SetIsFinalBlock(true);
	tweak.SetBitsProcessed(32);

	cipher.SetTweak(tweak.GetTweak());
	cipher.Encrypt(m_configString, m_configValue);

	m_configValue[0] ^= m_configString[0];
	m_configValue[1] ^= m_configString[1];
	m_configValue[2] ^= m_configString[2];
}

void Skein256::Initialize()
{
	// copy the configuration value to the state
	for (size_t i = 0; i < m_digestState.size(); i++)
		m_digestState[i] = m_configValue[i];

	// set up tweak for message block
	m_ubiParameters.StartNewBlockType((SkeinUbiType)Message);
	// reset bytes filled
	m_bytesFilled = 0;
}

void Skein256::ProcessBlock(uint Value)
{
	// set the key to the current state
	m_blockCipher.SetKey(m_digestState);
	// update tweak
	ulong bits = m_ubiParameters.GetBitsProcessed() + Value;
	m_ubiParameters.SetBitsProcessed(bits);
	m_blockCipher.SetTweak(m_ubiParameters.GetTweak());
	// encrypt block
	m_blockCipher.Encrypt(m_cipherInput, m_digestState);

	// feed-forward input with state
	for (size_t i = 0; i < m_cipherInput.size(); i++)
		m_digestState[i] ^= m_cipherInput[i];
}

void Skein256::PutBytes(std::vector<ulong> Input, std::vector<byte> &Output, size_t Offset, size_t ByteCount)
{
	ulong j = 0;
	for (size_t i = 0; i < ByteCount; i++)
	{
		Output[Offset + i] = (byte)((Input[i / 8] >> j) & 0xff);
		j = (j + 8) % 64;
	}
}

NAMESPACE_DIGESTEND
