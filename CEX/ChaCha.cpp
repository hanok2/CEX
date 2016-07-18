#include "ChaCha.h"
#include "CpuDetect.h"
#include "IntUtils.h"
#include "ParallelUtils.h"

#if defined(HAS_MINSSE)
#	include "UInt128.h"
#	if defined(HAS_AVX)
#		include "UInt256.h"
#	endif
#endif

NAMESPACE_STREAM

void ChaCha::Destroy()
{
	if (!m_isDestroyed)
	{
		m_isDestroyed = true;
		m_hasAVX = false;
		m_hasIntrinsics = false;
		m_isInitialized = false;
		m_processorCount = 0;
		m_isParallel = false;
		m_parallelBlockSize = 0;
		m_rndCount = 0;

		CEX::Utility::IntUtils::ClearVector(m_ctrVector);
		CEX::Utility::IntUtils::ClearVector(m_wrkState);
		CEX::Utility::IntUtils::ClearVector(m_dstCode);
		CEX::Utility::IntUtils::ClearVector(m_threadVectors);
	}
}

void ChaCha::DetectCpu()
{
	CEX::Common::CpuDetect detect;
	m_hasIntrinsics = detect.HasMinIntrinsics();
	m_hasAVX = detect.HasAVX();
}

void ChaCha::Initialize(const CEX::Common::KeyParams &KeyParam)
{
#if defined(ENABLE_CPPEXCEPTIONS)
	if (KeyParam.IV().size() != 8)
		throw CryptoSymmetricCipherException("ChaCha:Initialize", "Requires exactly 8 bytes of IV!");
	if (KeyParam.Key().size() != 16 && KeyParam.Key().size() != 32)
		throw CryptoSymmetricCipherException("ChaCha:Initialize", "Key must be 16 or 32 bytes!");
	if (ParallelBlockSize() < ParallelMinimumSize() || ParallelBlockSize() > ParallelMaximumSize())
		throw CryptoSymmetricCipherException("ChaCha:Initialize", "The parallel block size is out of bounds!");
	if (ParallelBlockSize() % ParallelMinimumSize() != 0)
		throw CryptoSymmetricCipherException("ChaCha:Initialize", "The parallel block size must be evenly aligned to the ParallelMinimumSize!");
#endif

	if (m_dstCode.size() == 0)
	{
		std::string info;
		if (KeyParam.Key().size() == 16)
			info = "expand 16-byte k";
		else
			info = "expand 32-byte k";

		m_dstCode.reserve(info.size());
		for (size_t i = 0; i < info.size(); ++i)
			m_dstCode.push_back(info[i]);
	}

	Reset();
	SetKey(KeyParam.Key(), KeyParam.IV());
	m_isInitialized = true;
}

void ChaCha::Reset()
{
	m_ctrVector[0] = 0;
	m_ctrVector[1] = 0;
}

void ChaCha::Transform(const std::vector<byte> &Input, std::vector<byte> &Output)
{
	ProcessBlock(Input, 0, Output, 0, Input.size());
}

void ChaCha::Transform(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset)
{
	ProcessBlock(Input, InOffset, Output, OutOffset, m_isParallel ? m_parallelBlockSize : BLOCK_SIZE);
}

void ChaCha::Transform(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset, const size_t Length)
{
	ProcessBlock(Input, InOffset, Output, OutOffset, Length);
}

// ** Key Schedule ** //

void ChaCha::SetKey(const std::vector<byte> &Key, const std::vector<byte> &Iv)
{
	if (Key.size() == 32)
	{
		m_wrkState[0] = CEX::Utility::IntUtils::BytesToLe32(m_dstCode, 0);
		m_wrkState[1] = CEX::Utility::IntUtils::BytesToLe32(m_dstCode, 4);
		m_wrkState[2] = CEX::Utility::IntUtils::BytesToLe32(m_dstCode, 8);
		m_wrkState[3] = CEX::Utility::IntUtils::BytesToLe32(m_dstCode, 12);
		m_wrkState[4] = CEX::Utility::IntUtils::BytesToLe32(Key, 0);
		m_wrkState[5] = CEX::Utility::IntUtils::BytesToLe32(Key, 4);
		m_wrkState[6] = CEX::Utility::IntUtils::BytesToLe32(Key, 8);
		m_wrkState[7] = CEX::Utility::IntUtils::BytesToLe32(Key, 12);
		m_wrkState[8] = CEX::Utility::IntUtils::BytesToLe32(Key, 16);
		m_wrkState[9] = CEX::Utility::IntUtils::BytesToLe32(Key, 20);
		m_wrkState[10] = CEX::Utility::IntUtils::BytesToLe32(Key, 24);
		m_wrkState[11] = CEX::Utility::IntUtils::BytesToLe32(Key, 28);
		m_wrkState[12] = CEX::Utility::IntUtils::BytesToLe32(Iv, 0);
		m_wrkState[13] = CEX::Utility::IntUtils::BytesToLe32(Iv, 4);

	}
	else
	{
		m_wrkState[0] = CEX::Utility::IntUtils::BytesToLe32(m_dstCode, 0);
		m_wrkState[1] = CEX::Utility::IntUtils::BytesToLe32(m_dstCode, 4);
		m_wrkState[2] = CEX::Utility::IntUtils::BytesToLe32(m_dstCode, 8);
		m_wrkState[3] = CEX::Utility::IntUtils::BytesToLe32(m_dstCode, 12);
		m_wrkState[4] = CEX::Utility::IntUtils::BytesToLe32(Key, 0);
		m_wrkState[5] = CEX::Utility::IntUtils::BytesToLe32(Key, 4);
		m_wrkState[6] = CEX::Utility::IntUtils::BytesToLe32(Key, 8);
		m_wrkState[7] = CEX::Utility::IntUtils::BytesToLe32(Key, 12);
		m_wrkState[8] = CEX::Utility::IntUtils::BytesToLe32(Key, 0);
		m_wrkState[9] = CEX::Utility::IntUtils::BytesToLe32(Key, 4);
		m_wrkState[10] = CEX::Utility::IntUtils::BytesToLe32(Key, 8);
		m_wrkState[11] = CEX::Utility::IntUtils::BytesToLe32(Key, 12);
		m_wrkState[12] = CEX::Utility::IntUtils::BytesToLe32(Iv, 0);
		m_wrkState[13] = CEX::Utility::IntUtils::BytesToLe32(Iv, 4);
	}
}

// ** Processing ** //

void ChaCha::Increase(const std::vector<uint> &Input, std::vector<uint> &Output, const size_t Length)
{
	Output = Input;

	for (size_t i = 0; i < Length; i++)
		Increment(Output);
}

void ChaCha::Increment(std::vector<uint> &Counter)
{
	if (++Counter[0] == 0)
		++Counter[1];
}

void ChaCha::Generate(std::vector<byte> &Output, const size_t OutOffset, std::vector<uint> &Counter, const size_t Length)
{
	size_t aln = Length - (Length % BLOCK_SIZE);
	size_t ctr = 0;

	const size_t BALN = Length - (Length % BLOCK_SIZE);
	const size_t BLK4 = 4 * BLOCK_SIZE;

	if (HasAVX() && Length >= 2 * BLK4)
	{
		const size_t BLK8 = 8 * BLOCK_SIZE;
		size_t paln = Length - (Length % BLK8);
		std::vector<uint> ctrBlk(BLK8 / sizeof(uint));

		// process 8 blocks (uses avx if available)
		while (ctr != paln)
		{
			memcpy(&ctrBlk[0], &Counter[0], 4);
			memcpy(&ctrBlk[8], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[1], &Counter[0], 4);
			memcpy(&ctrBlk[9], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[2], &Counter[0], 4);
			memcpy(&ctrBlk[10], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[3], &Counter[0], 4);
			memcpy(&ctrBlk[11], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[4], &Counter[0], 4);
			memcpy(&ctrBlk[12], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[5], &Counter[0], 4);
			memcpy(&ctrBlk[13], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[6], &Counter[0], 4);
			memcpy(&ctrBlk[14], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[7], &Counter[0], 4);
			memcpy(&ctrBlk[15], &Counter[0], 4);
			Increment(Counter);
			Transform512(Output, OutOffset + ctr, ctrBlk);
			ctr += BLK8;
		}
	}
	else if (HasIntrinsics() && Length >= BLK4)
	{
		size_t paln = Length - (Length % BLK4);
		std::vector<uint> ctrBlk(BLK4 / sizeof(uint));

		// process 4 blocks (uses sse intrinsics if available)
		while (ctr != paln)
		{
			memcpy(&ctrBlk[0], &Counter[0], 4);
			memcpy(&ctrBlk[4], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[1], &Counter[0], 4);
			memcpy(&ctrBlk[5], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[2], &Counter[0], 4);
			memcpy(&ctrBlk[6], &Counter[0], 4);
			Increment(Counter);
			memcpy(&ctrBlk[3], &Counter[0], 4);
			memcpy(&ctrBlk[7], &Counter[0], 4);
			Increment(Counter);
			Transform256(Output, OutOffset + ctr, ctrBlk);
			ctr += BLK4;
		}
	}

	while (ctr != aln)
	{
		Transform64(Output, OutOffset + ctr, Counter);
		Increment(Counter);
		ctr += BLOCK_SIZE;
	}

	if (ctr != Length)
	{
		std::vector<byte> outputBlock(BLOCK_SIZE, 0);
		Transform64(outputBlock, 0, Counter);
		int fnlSize = Length % BLOCK_SIZE;
		memcpy(&Output[OutOffset + (Length - fnlSize)], &outputBlock[0], fnlSize);
		Increment(Counter);
	}
}

uint ChaCha::GetProcessorCount()
{
	return CEX::Utility::ParallelUtils::ProcessorCount();
}

void ChaCha::ProcessBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset, const size_t Length)
{
	size_t blkSize = (Length > Input.size() - InOffset) ? Input.size() - InOffset : Length;
	if (blkSize > Output.size() - OutOffset)
		blkSize = Output.size() - OutOffset;

	if (!m_isParallel || blkSize < ParallelMinimumSize())
	{
		// generate random
		Generate(Output, OutOffset, m_ctrVector, blkSize);
		// output is input xor random
		size_t sze = blkSize - (blkSize % BLOCK_SIZE);

		if (sze != 0)
			CEX::Utility::IntUtils::XORBLK(Input, InOffset, Output, OutOffset, sze);

		// get the remaining bytes
		if (sze != blkSize)
		{
			for (size_t i = sze; i < blkSize; ++i)
				Output[i + OutOffset] ^= Input[i + InOffset];
		}
	}
	else
	{
		// parallel CTR processing //
		const size_t cnkSize = (blkSize / BLOCK_SIZE / m_processorCount) * BLOCK_SIZE;
		const size_t rndSize = cnkSize * m_processorCount;
		const size_t subSize = (cnkSize / BLOCK_SIZE);

		CEX::Utility::ParallelUtils::ParallelFor(0, m_processorCount, [this, &Input, InOffset, &Output, OutOffset, cnkSize, subSize](size_t i)
		{
			// offset counter by chunk size / block size
			this->Increase(m_ctrVector, m_threadVectors[i], subSize * i);
			// create random at offset position
			this->Generate(Output, (i * cnkSize), m_threadVectors[i], cnkSize);
			// xor with input at offset
			CEX::Utility::IntUtils::XORBLK(Input, InOffset + (i * cnkSize), Output, OutOffset + (i * cnkSize), cnkSize, HasIntrinsics());
		});

		// last block processing
		if (rndSize < blkSize)
		{
			size_t fnlSize = blkSize % rndSize;
			Generate(Output, rndSize, m_threadVectors[m_processorCount - 1], fnlSize);

			for (size_t i = 0; i < fnlSize; ++i)
				Output[i + OutOffset + rndSize] ^= (byte)(Input[i + InOffset + rndSize]);
		}

		// copy the last counter position to class variable
		memcpy(&m_ctrVector[0], &m_threadVectors[m_processorCount - 1][0], m_ctrVector.size() * sizeof(uint));
	}
}

void ChaCha::Transform64(std::vector<byte> &Output, size_t OutOffset, std::vector<uint> &Counter)
{
	size_t ctr = 0;
	uint X0 = m_wrkState[ctr];
	uint X1 = m_wrkState[++ctr];
	uint X2 = m_wrkState[++ctr];
	uint X3 = m_wrkState[++ctr];
	uint X4 = m_wrkState[++ctr];
	uint X5 = m_wrkState[++ctr];
	uint X6 = m_wrkState[++ctr];
	uint X7 = m_wrkState[++ctr];
	uint X8 = m_wrkState[++ctr];
	uint X9 = m_wrkState[++ctr];
	uint X10 = m_wrkState[++ctr];
	uint X11 = m_wrkState[++ctr];
	uint X12 = Counter[0];
	uint X13 = Counter[1];
	uint X14 = m_wrkState[++ctr];
	uint X15 = m_wrkState[++ctr];

	ctr = m_rndCount;
	while (ctr != 0)
	{
		X0 += X4;
		X12 = CEX::Utility::IntUtils::RotateFixLeft(X12 ^ X0, 16);
		X8 += X12;
		X4 = CEX::Utility::IntUtils::RotateFixLeft(X4 ^ X8, 12);
		X0 += X4; 
		X12 = CEX::Utility::IntUtils::RotateFixLeft(X12 ^ X0, 8);
		X8 += X12; 
		X4 = CEX::Utility::IntUtils::RotateFixLeft(X4 ^ X8, 7);

		X1 += X5; 
		X13 = CEX::Utility::IntUtils::RotateFixLeft(X13 ^ X1, 16);
		X9 += X13; 
		X5 = CEX::Utility::IntUtils::RotateFixLeft(X5 ^ X9, 12);
		X1 += X5; 
		X13 = CEX::Utility::IntUtils::RotateFixLeft(X13 ^ X1, 8);
		X9 += X13; 
		X5 = CEX::Utility::IntUtils::RotateFixLeft(X5 ^ X9, 7);//x1:338,x13:154,x9:323,x5:399

		X2 += X6; 
		X14 = CEX::Utility::IntUtils::RotateFixLeft(X14 ^ X2, 16);
		X10 += X14; 
		X6 = CEX::Utility::IntUtils::RotateFixLeft(X6 ^ X10, 12);
		X2 += X6; 
		X14 = CEX::Utility::IntUtils::RotateFixLeft(X14 ^ X2, 8);
		X10 += X14; 
		X6 = CEX::Utility::IntUtils::RotateFixLeft(X6 ^ X10, 7);

		X3 += X7; 
		X15 = CEX::Utility::IntUtils::RotateFixLeft(X15 ^ X3, 16);
		X11 += X15; 
		X7 = CEX::Utility::IntUtils::RotateFixLeft(X7 ^ X11, 12);
		X3 += X7; 
		X15 = CEX::Utility::IntUtils::RotateFixLeft(X15 ^ X3, 8);
		X11 += X15; 
		X7 = CEX::Utility::IntUtils::RotateFixLeft(X7 ^ X11, 7);

		X0 += X5; 
		X15 = CEX::Utility::IntUtils::RotateFixLeft(X15 ^ X0, 16);
		X10 += X15; 
		X5 = CEX::Utility::IntUtils::RotateFixLeft(X5 ^ X10, 12);
		X0 += X5; 
		X15 = CEX::Utility::IntUtils::RotateFixLeft(X15 ^ X0, 8);
		X10 += X15; 
		X5 = CEX::Utility::IntUtils::RotateFixLeft(X5 ^ X10, 7);

		X1 += X6; 
		X12 = CEX::Utility::IntUtils::RotateFixLeft(X12 ^ X1, 16);
		X11 += X12; 
		X6 = CEX::Utility::IntUtils::RotateFixLeft(X6 ^ X11, 12);
		X1 += X6; 
		X12 = CEX::Utility::IntUtils::RotateFixLeft(X12 ^ X1, 8);
		X11 += X12; 
		X6 = CEX::Utility::IntUtils::RotateFixLeft(X6 ^ X11, 7);

		X2 += X7; 
		X13 = CEX::Utility::IntUtils::RotateFixLeft(X13 ^ X2, 16);
		X8 += X13; 
		X7 = CEX::Utility::IntUtils::RotateFixLeft(X7 ^ X8, 12);
		X2 += X7; 
		X13 = CEX::Utility::IntUtils::RotateFixLeft(X13 ^ X2, 8);
		X8 += X13; 
		X7 = CEX::Utility::IntUtils::RotateFixLeft(X7 ^ X8, 7);

		X3 += X4; 
		X14 = CEX::Utility::IntUtils::RotateFixLeft(X14 ^ X3, 16);
		X9 += X14; 
		X4 = CEX::Utility::IntUtils::RotateFixLeft(X4 ^ X9, 12);
		X3 += X4; 
		X14 = CEX::Utility::IntUtils::RotateFixLeft(X14 ^ X3, 8);
		X9 += X14; 
		X4 = CEX::Utility::IntUtils::RotateFixLeft(X4 ^ X9, 7);
		ctr -= 2;
	}

	CEX::Utility::IntUtils::Le32ToBytes(X0 + m_wrkState[ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X1 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X2 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X3 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X4 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X5 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X6 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X7 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X8 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X9 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X10 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X11 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X12 + Counter[0], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X13 + Counter[1], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X14 + m_wrkState[++ctr], Output, OutOffset); OutOffset += 4;
	CEX::Utility::IntUtils::Le32ToBytes(X15 + m_wrkState[++ctr], Output, OutOffset);
}

void ChaCha::Transform256(std::vector<byte> &Output, size_t OutOffset, std::vector<uint> &Counter)
{
#if defined(HAS_MINSSE)

	size_t ctr = 0;
	CEX::Common::UInt128 X0(m_wrkState[ctr]);
	CEX::Common::UInt128 X1(m_wrkState[++ctr]);
	CEX::Common::UInt128 X2(m_wrkState[++ctr]);
	CEX::Common::UInt128 X3(m_wrkState[++ctr]);
	CEX::Common::UInt128 X4(m_wrkState[++ctr]);
	CEX::Common::UInt128 X5(m_wrkState[++ctr]);
	CEX::Common::UInt128 X6(m_wrkState[++ctr]);
	CEX::Common::UInt128 X7(m_wrkState[++ctr]);
	CEX::Common::UInt128 X8(m_wrkState[++ctr]);
	CEX::Common::UInt128 X9(m_wrkState[++ctr]);
	CEX::Common::UInt128 X10(m_wrkState[++ctr]);
	CEX::Common::UInt128 X11(m_wrkState[++ctr]);
	CEX::Common::UInt128 X12(Counter, 0);
	CEX::Common::UInt128 X13(Counter, 16);
	CEX::Common::UInt128 X14(m_wrkState[++ctr]);
	CEX::Common::UInt128 X15(m_wrkState[++ctr]);

	ctr = m_rndCount;
	while (ctr != 0)
	{
		X0 += X4;
		X12 = CEX::Common::UInt128::Rotl32(X12 ^ X0, 16);
		X8 += X12;
		X4 = CEX::Common::UInt128::Rotl32(X4 ^ X8, 12);
		X0 += X4;
		X12 = CEX::Common::UInt128::Rotl32(X12 ^ X0, 8);
		X8 += X12;
		X4 = CEX::Common::UInt128::Rotl32(X4 ^ X8, 7);

		X1 += X5;
		X13 = CEX::Common::UInt128::Rotl32(X13 ^ X1, 16);
		X9 += X13;
		X5 = CEX::Common::UInt128::Rotl32(X5 ^ X9, 12);
		X1 += X5;
		X13 = CEX::Common::UInt128::Rotl32(X13 ^ X1, 8);
		X9 += X13;
		X5 = CEX::Common::UInt128::Rotl32(X5 ^ X9, 7);//x1:338,x13:154,x9:323,x5:399

		X2 += X6;
		X14 = CEX::Common::UInt128::Rotl32(X14 ^ X2, 16);
		X10 += X14;
		X6 = CEX::Common::UInt128::Rotl32(X6 ^ X10, 12);
		X2 += X6;
		X14 = CEX::Common::UInt128::Rotl32(X14 ^ X2, 8);
		X10 += X14;
		X6 = CEX::Common::UInt128::Rotl32(X6 ^ X10, 7);

		X3 += X7;
		X15 = CEX::Common::UInt128::Rotl32(X15 ^ X3, 16);
		X11 += X15;
		X7 = CEX::Common::UInt128::Rotl32(X7 ^ X11, 12);
		X3 += X7;
		X15 = CEX::Common::UInt128::Rotl32(X15 ^ X3, 8);
		X11 += X15;
		X7 = CEX::Common::UInt128::Rotl32(X7 ^ X11, 7);

		X0 += X5;
		X15 = CEX::Common::UInt128::Rotl32(X15 ^ X0, 16);
		X10 += X15;
		X5 = CEX::Common::UInt128::Rotl32(X5 ^ X10, 12);
		X0 += X5;
		X15 = CEX::Common::UInt128::Rotl32(X15 ^ X0, 8);
		X10 += X15;
		X5 = CEX::Common::UInt128::Rotl32(X5 ^ X10, 7);

		X1 += X6;
		X12 = CEX::Common::UInt128::Rotl32(X12 ^ X1, 16);
		X11 += X12;
		X6 = CEX::Common::UInt128::Rotl32(X6 ^ X11, 12);
		X1 += X6;
		X12 = CEX::Common::UInt128::Rotl32(X12 ^ X1, 8);
		X11 += X12;
		X6 = CEX::Common::UInt128::Rotl32(X6 ^ X11, 7);

		X2 += X7;
		X13 = CEX::Common::UInt128::Rotl32(X13 ^ X2, 16);
		X8 += X13;
		X7 = CEX::Common::UInt128::Rotl32(X7 ^ X8, 12);
		X2 += X7;
		X13 = CEX::Common::UInt128::Rotl32(X13 ^ X2, 8);
		X8 += X13;
		X7 = CEX::Common::UInt128::Rotl32(X7 ^ X8, 7);

		X3 += X4;
		X14 = CEX::Common::UInt128::Rotl32(X14 ^ X3, 16);
		X9 += X14;
		X4 = CEX::Common::UInt128::Rotl32(X4 ^ X9, 12);
		X3 += X4;
		X14 = CEX::Common::UInt128::Rotl32(X14 ^ X3, 8);
		X9 += X14;
		X4 = CEX::Common::UInt128::Rotl32(X4 ^ X9, 7);
		ctr -= 2;
	}

	// last round
	X0 += m_wrkState[ctr];
	X1 += m_wrkState[++ctr];
	X2 += m_wrkState[++ctr];
	X3 += m_wrkState[++ctr];
	X4 += m_wrkState[++ctr];
	X5 += m_wrkState[++ctr];
	X6 += m_wrkState[++ctr];
	X7 += m_wrkState[++ctr];
	X8 += m_wrkState[++ctr];
	X9 += m_wrkState[++ctr];
	X10 += m_wrkState[++ctr];
	X11 += m_wrkState[++ctr];
	X12 += CEX::Common::UInt128(Counter, 0);
	X13 += CEX::Common::UInt128(Counter, 16);
	X14 += m_wrkState[++ctr];
	X15 += m_wrkState[++ctr];

	CEX::Common::UInt128::StoreLE256(Output, OutOffset, X0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15);

#else

	Transform64(Output, OutOffset, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 64, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 128, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 192, Counter);

#endif
}

void ChaCha::Transform512(std::vector<byte> &Output, size_t OutOffset, std::vector<uint> &Counter)
{
#if defined(HAS_AVX)

	size_t ctr = 0;
	CEX::Common::UInt256 X0(m_wrkState[ctr]);
	CEX::Common::UInt256 X1(m_wrkState[++ctr]);
	CEX::Common::UInt256 X2(m_wrkState[++ctr]);
	CEX::Common::UInt256 X3(m_wrkState[++ctr]);
	CEX::Common::UInt256 X4(m_wrkState[++ctr]);
	CEX::Common::UInt256 X5(m_wrkState[++ctr]);
	CEX::Common::UInt256 X6(m_wrkState[++ctr]);
	CEX::Common::UInt256 X7(m_wrkState[++ctr]);
	CEX::Common::UInt256 X8(m_wrkState[++ctr]);
	CEX::Common::UInt256 X9(m_wrkState[++ctr]);
	CEX::Common::UInt256 X10(m_wrkState[++ctr]);
	CEX::Common::UInt256 X11(m_wrkState[++ctr]);
	CEX::Common::UInt256 X12(Counter, 0);
	CEX::Common::UInt256 X13(Counter, 32);
	CEX::Common::UInt256 X14(m_wrkState[++ctr]);
	CEX::Common::UInt256 X15(m_wrkState[++ctr]);

	ctr = m_rndCount;
	while (ctr != 0)
	{
		X0 += X4;
		X12 = CEX::Common::UInt256::Rotl32(X12 ^ X0, 16);
		X8 += X12;
		X4 = CEX::Common::UInt256::Rotl32(X4 ^ X8, 12);
		X0 += X4;
		X12 = CEX::Common::UInt256::Rotl32(X12 ^ X0, 8);
		X8 += X12;
		X4 = CEX::Common::UInt256::Rotl32(X4 ^ X8, 7);

		X1 += X5;
		X13 = CEX::Common::UInt256::Rotl32(X13 ^ X1, 16);
		X9 += X13;
		X5 = CEX::Common::UInt256::Rotl32(X5 ^ X9, 12);
		X1 += X5;
		X13 = CEX::Common::UInt256::Rotl32(X13 ^ X1, 8);
		X9 += X13;
		X5 = CEX::Common::UInt256::Rotl32(X5 ^ X9, 7);//x1:338,x13:154,x9:323,x5:399

		X2 += X6;
		X14 = CEX::Common::UInt256::Rotl32(X14 ^ X2, 16);
		X10 += X14;
		X6 = CEX::Common::UInt256::Rotl32(X6 ^ X10, 12);
		X2 += X6;
		X14 = CEX::Common::UInt256::Rotl32(X14 ^ X2, 8);
		X10 += X14;
		X6 = CEX::Common::UInt256::Rotl32(X6 ^ X10, 7);

		X3 += X7;
		X15 = CEX::Common::UInt256::Rotl32(X15 ^ X3, 16);
		X11 += X15;
		X7 = CEX::Common::UInt256::Rotl32(X7 ^ X11, 12);
		X3 += X7;
		X15 = CEX::Common::UInt256::Rotl32(X15 ^ X3, 8);
		X11 += X15;
		X7 = CEX::Common::UInt256::Rotl32(X7 ^ X11, 7);

		X0 += X5;
		X15 = CEX::Common::UInt256::Rotl32(X15 ^ X0, 16);
		X10 += X15;
		X5 = CEX::Common::UInt256::Rotl32(X5 ^ X10, 12);
		X0 += X5;
		X15 = CEX::Common::UInt256::Rotl32(X15 ^ X0, 8);
		X10 += X15;
		X5 = CEX::Common::UInt256::Rotl32(X5 ^ X10, 7);

		X1 += X6;
		X12 = CEX::Common::UInt256::Rotl32(X12 ^ X1, 16);
		X11 += X12;
		X6 = CEX::Common::UInt256::Rotl32(X6 ^ X11, 12);
		X1 += X6;
		X12 = CEX::Common::UInt256::Rotl32(X12 ^ X1, 8);
		X11 += X12;
		X6 = CEX::Common::UInt256::Rotl32(X6 ^ X11, 7);

		X2 += X7;
		X13 = CEX::Common::UInt256::Rotl32(X13 ^ X2, 16);
		X8 += X13;
		X7 = CEX::Common::UInt256::Rotl32(X7 ^ X8, 12);
		X2 += X7;
		X13 = CEX::Common::UInt256::Rotl32(X13 ^ X2, 8);
		X8 += X13;
		X7 = CEX::Common::UInt256::Rotl32(X7 ^ X8, 7);

		X3 += X4;
		X14 = CEX::Common::UInt256::Rotl32(X14 ^ X3, 16);
		X9 += X14;
		X4 = CEX::Common::UInt256::Rotl32(X4 ^ X9, 12);
		X3 += X4;
		X14 = CEX::Common::UInt256::Rotl32(X14 ^ X3, 8);
		X9 += X14;
		X4 = CEX::Common::UInt256::Rotl32(X4 ^ X9, 7);
		ctr -= 2;
	}

	// last round
	X0 += m_wrkState[ctr];
	X1 += m_wrkState[++ctr];
	X2 += m_wrkState[++ctr];
	X3 += m_wrkState[++ctr];
	X4 += m_wrkState[++ctr];
	X5 += m_wrkState[++ctr];
	X6 += m_wrkState[++ctr];
	X7 += m_wrkState[++ctr];
	X8 += m_wrkState[++ctr];
	X9 += m_wrkState[++ctr];
	X10 += m_wrkState[++ctr];
	X11 += m_wrkState[++ctr];
	X12 += CEX::Common::UInt256(Counter, 0);
	X13 += CEX::Common::UInt256(Counter, 32);
	X14 += m_wrkState[++ctr];
	X15 += m_wrkState[++ctr];

	CEX::Common::UInt256::StoreLE512(Output, OutOffset, X0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15);

#else

	Transform64(Output, OutOffset, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 64, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 128, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 192, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 256, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 320, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 384, Counter);
	Increment(Counter);
	Transform64(Output, OutOffset + 448, Counter);

#endif
}

void ChaCha::SetScope()
{
	m_processorCount = CEX::Utility::ParallelUtils::ProcessorCount();
	if (m_processorCount % 2 != 0)
		m_processorCount--;
	if (m_processorCount > 1)
		m_isParallel = true;

	if (m_isParallel)
	{
		if (m_threadVectors.size() != m_processorCount)
			m_threadVectors.resize(m_processorCount);
		for (size_t i = 0; i < m_processorCount; ++i)
			m_threadVectors[i].resize(VECTOR_SIZE / sizeof(uint));
	}
}

NAMESPACE_STREAMEND