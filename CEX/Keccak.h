// The GPL version 3 License (GPLv3)
// 
// Copyright (c) 2019 vtdev.com
// This file is part of the CEX Cryptographic library.
// 
// This program is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITStateOUT ANY WARRANTY; without even the implied warranty of
// MERCStateANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef CEX_KECCAK_H
#define CEX_KECCAK_H

#include "CexDomain.h"
#include "IntegerTools.h"
#include "MemoryTools.h"

#if defined(__AVX2__)
#	include "ULong256.h"
#endif
#if defined(__AVX512__)
#	include "ULong512.h"
#endif

NAMESPACE_DIGEST

using Utility::IntegerTools;
using Utility::MemoryTools;

#if defined(__AVX2__)
	using Numeric::ULong256;
#endif
#if defined(__AVX512__)
	using Numeric::ULong512;
#endif

/// <summary>
/// Internal static class containing the 24 and 48 round Keccak permutation functions.
/// <para>The function names are in the format; Permute-rounds-bits-suffix, ex. PermuteR24P1600C, 24 rounds, permutes 1600 bits, using the compact form of the function. \n
/// Note: The PermuteR48P1600U is an extended permutation function that uses 48 rounds, rather than the 24 rounds used by the standard implementation of Keccak. \n
/// The additional 24 rounds constants were generated using the LFSR from the Keccak code package, with the additional 24 constants being \n
/// the next in sequence generated by that LFSR.</para>
/// <para>The compact forms of the permutations have the suffix C, and are optimized for low memory consumption 
/// (enabled in the hash function by adding the CEX_DIGEST_COMPACT to the CexConfig file). \n
/// The Unrolled forms are optimized for speed and timing neutrality have the U suffix. \n
/// The H suffix denotes functions that take an SIMD wrapper class as the state values, and process message blocks in SIMD parallel blocks.</para>
/// <para>This class contains wide forms of the functions; PermuteR24P4x1600H and PermuteR48P4x1600H use AVX2. \n
/// Experimental functions using AVX512 instructions are also implemented; PermuteR24P8x1600H and PermuteR48P8x1600H. \n
/// These extended functions are only visible at run-time on some development platforms (VS..), if the __AVX2__ or __AVX512__ compiler flags are declared explicitly.</para>
/// </summary>
class Keccak
{
// Keccak 1024 round constants enum:
// Generated using the InitializeRoundConstants/LFSR86540 function from the keccak code package:
// https://github.com/gvanas/KeccakCodePackage/blob/aa3cded0ae844dbff0dbecfb6d42d50c7bdb9d9b/SnP/KeccakP-1600/Reference/KeccakP-1600-reference.c
// The first 24 are the standard constants, the second set was generated by extending the LFSR to generate 48 round constants
//const ulong RC[48] =
//{
//	0x0000000000000001, 0x0000000000008082, 0x800000000000808A, 0x8000000080008000,
//	0x000000000000808B, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
//	0x000000000000008A, 0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
//	0x000000008000808B, 0x800000000000008B, 0x8000000000008089, 0x8000000000008003,
//	0x8000000000008002, 0x8000000000000080, 0x000000000000800A, 0x800000008000000A,
//	0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
//  // next sequence generated using LFSR86540
//	0x8000000080008082, 0x800000008000800A, 0x8000000000000003, 0x8000000080000009,
//	0x8000000000008082, 0x0000000000008009, 0x8000000000000080, 0x0000000000008083,
//	0x8000000000000081, 0x0000000000000001, 0x000000000000800B, 0x8000000080008001,
//	0x0000000000000080, 0x8000000000008000, 0x8000000080008001, 0x0000000000000009,
//	0x800000008000808B, 0x0000000000000081, 0x8000000000000082, 0x000000008000008B,
//	0x8000000080008009, 0x8000000080000000, 0x0000000080000080, 0x0000000080008003
//};

private:

	static const std::array<ulong, 24> RC24;
	static const std::array<ulong, 48> RC48;

public:

	static const byte KECCAK_CSHAKE_DOMAIN = 0x04;
	static const byte KECCAK_SHA3_DOMAIN = 0x06;
	static const byte KECCAK_SHAKE_DOMAIN = 0x1F;
	static const byte KECCAK_CSHAKEW4_DOMAIN = 0x21;
	static const byte KECCAK_CSHAKEW8_DOMAIN = 0x22;

	static const size_t KECCAK128_DIGEST_SIZE = 16;
	static const size_t KECCAK256_DIGEST_SIZE = 32;
	static const size_t KECCAK512_DIGEST_SIZE = 64;
	static const size_t KECCAK1024_DIGEST_SIZE = 128;
	static const size_t KECCAK128_RATE_SIZE = 168;
	static const size_t KECCAK256_RATE_SIZE = 136;
	static const size_t KECCAK512_RATE_SIZE = 72;
#if defined CEX_KECCAK_STRONG
	static const size_t KECCAK1024_RATE_SIZE = 36;
#else
	static const size_t KECCAK1024_RATE_SIZE = 72;
#endif

	static const size_t KECCAK_STATE_SIZE = 25;

	/// <summary>
	/// The Keccak absorb function; copy bytes from a byte array to the state array.
	/// <para>Input length must be 64-bit aligned.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input byte array, can be either an 8-bit array or vector</param>
	/// <param name="InOffset">The starting offset withing the input array</param>
	/// <param name="InLength">The number of bytes to process; must be 64-bit aligned</param>
	/// <param name="State">The permutations uint64 state array</param>
	template<typename Array>
	static void Absorb(const Array &Input, size_t InOffset, size_t InLength, std::array<ulong, KECCAK_STATE_SIZE> &State)
	{
		CEXASSERT(InLength % sizeof(ulong) == 0, "The input length is not 64-bit aligned");

#if defined(CEX_IS_LITTLE_ENDIAN)
		MemoryTools::XOR(Input, InOffset, State, 0, InLength);
#else
		for (size_t i = 0; i < InLength / sizeof(ulong); ++i)
		{
			State[i] ^= IntegerTools::LeBytesTo64(Input, InOffset + (i * sizeof(ulong)));
		}
#endif
	}

	/// <summary>
	/// The Keccak 24-round extraction function; extract blocks of state to an output 8-bit array
	/// </summary>
	/// 
	/// <param name="State">The permutations uint64 state array</param>
	/// <param name="Output">The output byte array, can be either an 8-bit array or vector</param>
	/// <param name="OutOffset">The starting offset withing the output array</param>
	/// <param name="Blocks">The number of blocks to extract</param>
	/// <param name="Rate">The Keccak extraction rate</param>
	template<typename Array>
	static void SqueezeR24(std::array<ulong, KECCAK_STATE_SIZE> &State, Array &Output, size_t OutOffset, size_t Blocks, size_t Rate)
	{
		while (Blocks > 0)
		{
#if defined(CEX_DIGEST_COMPACT)
			Digest::Keccak::PermuteR24P1600C(State);
#else
			Digest::Keccak::PermuteR24P1600U(State);
#endif

#if defined(CEX_IS_LITTLE_ENDIAN)
			MemoryTools::Copy(State, 0, Output, OutOffset, Rate);
#else

			for (size_t i = 0; i < (Rate >> 3); i++)
			{
				IntegerTools::Le64ToBytes(State[i], Output, OutOffset + (8 * i));
			}
#endif

			OutOffset += Rate;
			--Blocks;
		}
	}

	/// <summary>
	/// The Keccak 48-round extraction function; extract blocks of state to an output 8-bit array
	/// </summary>
	/// 
	/// <param name="State">The permutations uint64 state array</param>
	/// <param name="Output">The output byte array, can be either an 8-bit array or vector</param>
	/// <param name="OutOffset">The starting offset withing the output array</param>
	/// <param name="Blocks">The number of blocks to extract</param>
	/// <param name="Rate">The Keccak extraction rate</param>
	template<typename Array>
	static void SqueezeR48(std::array<ulong, KECCAK_STATE_SIZE> &State, Array &Output, size_t OutOffset, size_t Blocks, size_t Rate)
	{
		while (Blocks > 0)
		{
#if defined(CEX_DIGEST_COMPACT)
			Digest::Keccak::PermuteR48P1600C(State);
#else
			Digest::Keccak::PermuteR48P1600U(State);
#endif

#if defined(CEX_IS_LITTLE_ENDIAN)
			MemoryTools::Copy(State, 0, Output, OutOffset, Rate);
#else

			for (size_t i = 0; i < (Rate >> 3); i++)
			{
				IntegerTools::Le64ToBytes(State[i], Output, OutOffset + (8 * i));
			}
#endif

			OutOffset += Rate;
			--Blocks;
			}
	}

	/// <summary>
	/// The Keccak XOF function using 24 rounds; process an input seed array and return a pseudo-random output array.
	/// <para>A compact form of the SHAKE XOF function.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input byte seed array, can be either an 8-bit array or vector</param>
	/// <param name="InOffset">The starting offset withing the input array</param>
	/// <param name="InLength">The number of seed bytes to process</param>
	/// <param name="Output">The input byte seed array, can be either an 8-bit array or vector</param>
	/// <param name="OutOffset">The starting offset withing the output array</param>
	/// <param name="OutLength">The number of output bytes to produce</param>
	/// <param name="Rate">The block rate of permutation calls; SHAKE128=168, SHAKE256=136, SHAKE512=72</param>
	template<typename ArrayA, typename ArrayB>
	static void XOFR24P1600(const ArrayA &Input, size_t InOffset, size_t InLength, ArrayB &Output, size_t OutOffset, size_t OutLength, size_t Rate)
	{
		std::array<byte, 200> msg = { 0 };
		std::array<ulong, 25> state = { 0 };
		size_t blkcnt;
		size_t i;

		while (InLength >= Rate)
		{
			Keccak::Absorb(Input, InOffset, Rate, state);
#if defined(CEX_DIGEST_COMPACT)
			Keccak::PermuteR24P1600C(state);
#else
			Keccak::PermuteR24P1600U(state);
#endif
			InLength -= Rate;
			InOffset += Rate;
		}

		MemoryTools::Copy(Input, InOffset, msg, 0, InLength);
		msg[InLength] = 0x1F;
		MemoryTools::Clear(msg, InLength + 1, Rate - InLength + 1);
		msg[Rate - 1] |= 128;

#if defined(CEX_IS_LITTLE_ENDIAN)
		MemoryTools::XOR(msg, 0, state, 0, Rate);
#else
		for (i = 0; i < (Rate >> 3); ++i)
		{
			state[i] ^= IntegerTools::LeBytesTo64(msg, (8 * i));
		}
#endif

		blkcnt = OutLength / Rate;
		Keccak::SqueezeR24(state, Output, OutOffset, blkcnt, Rate);
		OutOffset += blkcnt * Rate;
		OutLength -= blkcnt * Rate;

		if (OutLength != 0)
		{
#if defined(CEX_DIGEST_COMPACT)
			Digest::Keccak::PermuteR24P1600C(state);
#else
			Keccak::PermuteR24P1600U(state);
#endif

			const size_t FNLBLK = OutLength % sizeof(ulong) == 0 ? OutLength / sizeof(ulong) : OutLength / sizeof(ulong) + 1;

			for (i = 0; i < FNLBLK; i++)
			{
				IntegerTools::Le64ToBytes(state[i], msg, (8 * i));
			}

			MemoryTools::Copy(msg, 0, Output, OutOffset, OutLength);
		}
	}

	/// <summary>
	/// The Keccak XOF function using 48 rounds; process an input seed array and return a pseudo-random output array.
	/// <para>A compact form of the SHAKE XOF function.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input byte seed array, can be either an 8-bit array or vector</param>
	/// <param name="InOffset">The starting offset withing the input array</param>
	/// <param name="InLength">The number of seed bytes to process</param>
	/// <param name="Output">The input byte seed array, can be either an 8-bit array or vector</param>
	/// <param name="OutOffset">The starting offset withing the output array</param>
	/// <param name="OutLength">The number of output bytes to produce</param>
	/// <param name="Rate">The block rate of permutation calls; SHAKE128=168, SHAKE256=136, SHAKE512=72</param>
	template<typename ArrayA, typename ArrayB>
	static void XOFR48P1600(const ArrayA &Input, size_t InOffset, size_t InLength, ArrayB &Output, size_t OutOffset, size_t OutLength, size_t Rate)
	{
		std::array<byte, 200> msg = { 0 };
		std::array<ulong, 25> state = { 0 };
		size_t blkcnt;
		size_t i;

		while (InLength >= Rate)
		{
			Keccak::Absorb(Input, InOffset, Rate, state);
#if defined(CEX_DIGEST_COMPACT)
			Keccak::PermuteR48P1600C(state);
#else
			Keccak::PermuteR48P1600U(state);
#endif
			InLength -= Rate;
			InOffset += Rate;
		}

		MemoryTools::Copy(Input, InOffset, msg, 0, InLength);
		msg[InLength] = 0x1F;
		MemoryTools::Clear(msg, InLength + 1, Rate - InLength + 1);
		msg[Rate - 1] |= 128;

#if defined(CEX_IS_LITTLE_ENDIAN)
		MemoryTools::XOR(msg, 0, state, 0, Rate);
#else
		for (i = 0; i < (Rate >> 3); ++i)
		{
			state[i] ^= IntegerTools::LeBytesTo64(msg, (8 * i));
		}
#endif

		blkcnt = OutLength / Rate;
		Keccak::SqueezeR48(state, Output, OutOffset, blkcnt, Rate);
		OutOffset += blkcnt * Rate;
		OutLength -= blkcnt * Rate;

		if (OutLength != 0)
		{
#if defined(CEX_DIGEST_COMPACT)
			Digest::Keccak::PermuteR48P1600C(state);
#else
			Keccak::PermuteR48P1600U(state);
#endif

			const size_t FNLBLK = OutLength % sizeof(ulong) == 0 ? OutLength / sizeof(ulong) : OutLength / sizeof(ulong) + 1;

			for (i = 0; i < FNLBLK; i++)
			{
				IntegerTools::Le64ToBytes(state[i], msg, (8 * i));
			}

			MemoryTools::Copy(msg, 0, Output, OutOffset, OutLength);
		}
	}

	/// <summary>
	/// The compact form of the 24 round (standard) SHA3 permutation function.
	/// <para>This function has been optimized for a small memory consumption.
	/// To enable this function, add the CEX_DIGEST_COMPACT directive to the CexConfig file.</para>
	/// </summary>
	/// 
	/// <param name="State">The permutations uint64 state array</param>
	static void PermuteR24P1600C(std::array<ulong, KECCAK_STATE_SIZE> &State);

	/// <summary>
	/// The unrolled form of the 24 round (standard) SHA3 permutation function.
	/// <para>This function (the default) has been optimized for speed, and timing neutrality.
	/// To enable this function, remove the CEX_DIGEST_COMPACT directive from the CexConfig file.</para>
	/// </summary>
	/// 
	/// <param name="State">The permutations uint64 state array</param>
	static void PermuteR24P1600U(std::array<ulong, KECCAK_STATE_SIZE> &State);

	/// <summary>
	/// The compact form of the 48 round (extended) SHA3 permutation function.
	/// <para>This function has been optimized for a small memory consumption.
	/// To enable this function, add the CEX_DIGEST_COMPACT directive to the CexConfig file.</para>
	/// </summary>
	/// 
	/// <param name="State">The permutations uint64 state array</param>
	static void PermuteR48P1600C(std::array<ulong, KECCAK_STATE_SIZE> &State);

	/// <summary>
	/// The unrolled form of the 48 round (extended) SHA3 permutation function.
	/// <para>This function (the default) has been optimized for speed, and timing neutrality.
	/// To enable this function, remove the CEX_DIGEST_COMPACT directive from the CexConfig file.</para>
	/// </summary>
	/// 
	/// <param name="State">The permutations uint64 state array</param>
	static void PermuteR48P1600U(std::array<ulong, KECCAK_STATE_SIZE> &State);

#if defined(__AVX2__)

	/// <summary>
	/// The horizontally vectorized 24 round (standard) form of the SHA3 permutation function.
	/// <para>This function processes 4*25 blocks of state in parallel using AVX2 instructions.</para>
	/// </summary>
	/// 
	/// <param name="State">The permutations ULong256 state array</param>
	static void PermuteR24P4x1600H(std::vector<ULong256> &State);

	/// <summary>
	/// The horizontally vectorized 48 round form (extended) of the SHA3 permutation function.
	/// <para>This function processes 4*25 blocks of state in parallel using AVX2 instructions.</para>
	/// </summary>
	/// 
	/// <param name="State">The permutations ULong256 state array</param>
	static void PermuteR48P4x1600H(std::vector<ULong256> &State);

#endif

#if defined(__AVX512__)

	/// <summary>
	/// The horizontally vectorized 24 round (standard) form of the SHA3 permutation function.
	/// <para>This function processes 8*25 blocks of state in parallel using AVX512 instructions.</para>
	/// </summary>
	/// 
	/// <param name="State">The permutations ULong512 state array</param>
	static void PermuteR24P8x1600H(std::vector<ULong512> &State);

	/// <summary>
	/// The horizontally vectorized 48 round (extended) form of the SHA3 permutation function.
	/// <para>This function processes 8*25 blocks of state in parallel using AVX512 instructions.</para>
	/// </summary>
	/// 
	/// <param name="State">The permutations ULong512 state array</param>
	static void PermuteR48P8x1600H(std::vector<ULong512> &State);

#endif

};

NAMESPACE_DIGESTEND
#endif
