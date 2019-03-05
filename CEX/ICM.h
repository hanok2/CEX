﻿// The GPL version 3 License (GPLv3)
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
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//
//
// Implementation Details:
// An implementation of an integer Counter Mode (ICM).
// Written by John Underhill, September 24, 2014
// Updated September 16, 2016
// Updated April 18, 2017
// Updated October 14, 2017
// Updated March 1, 2019
// Contact: develop@vtdev.com

#ifndef CEX_ICM_H
#define CEX_ICM_H

#include "ICipherMode.h"

NAMESPACE_MODE

/// <summary>
/// ICM: An implementation of a Little-Endian Integer Counter Mode
/// </summary> 
/// 
/// <example>
/// <description>Encrypting a single block of bytes:</description>
/// <code>
/// ICM cipher(BlockCiphers::AES);
/// // initialize for encryption
/// cipher.Initialize(true, SymmetricKey(Key, Nonce));
/// // encrypt one block
/// cipher.EncryptBlock(Input, 0, Output, 0);
/// </code>
/// </example>
///
/// <example>
/// <description>Encrypting using parallel processing:</description>
/// <code>
/// ICM cipher(new AES());
/// // enable parallel and define parallel input block size
/// cipher.IsParallel() = true;
/// // calculated automatically based on cache size, but overridable
/// cipher.ParallelBlockSize() = cipher.ProcessorCount() * 32000;
/// // initialize for encryption
/// cipher.Initialize(true, SymmetricKey(Key, Nonce));
/// // encrypt one parallel sized block
/// cipher.Transform(Input, 0, Output, 0);
/// </code>
/// </example>
/// 
/// <remarks>
/// <description><B>Overview:</B></description>
/// <para>The ICM Counter mode generates a key-stream by encrypting successive values of an incrementing Little Endian ordered, 64bit integer counter array. \n
/// The key-stream is then XOR'd with the input message block creating a type of stream cipher. \n
/// The ICM counter mode differs from the standard CTR mode by using a little endian byte ordered counter, allowing the use of 64 bit integers in the counter array on an LE based architecture (e.g AMD, Intel). \n
/// The trend in processors is moving towards little endian format, and with devices that use this bit ordering, ICM can be significantly faster then the standard big endian CTR implementation. \n
/// In parallel mode, the ICM modes counter is increased by a number factored from the number of message blocks (ParallelBlockSize), allowing for counter pre-calculation and multi-threaded processing. \n
/// The implementation is further parallelized by constructing a 'staggered' counter array, and processing large sub-blocks using 128 or 256 SIMD instructions through the ciphers Transform-64/128 SIMD methods.</para>
/// 
/// <description><B>Description:</B></description>
/// <para><EM>Legend:</EM> \n 
/// <B>C</B>=ciphertext, <B>P</B>=plaintext, <B>K</B>=key, <B>E</B>=encrypt, <B>^</B>=XOR \n
/// <EM>Encryption</EM> \n
/// C0 ← IV. For 1 ≤ j ≤ t, Cj ← EK(Cj) ^ Pj, C+1.</para> \n
///
/// <description><B>Multi-Threading:</B></description>
/// <para>The transformation function of the ICM mode is not limited by a dependency chain; this mode can be both SIMD pipelined and multi-threaded.
/// Output from the parallelized functions aligns with the output from a standard sequential ICM implementation. \n
/// This is achieved by pre-calculating the counters positional offset over multiple 'chunks' of key-stream, which are then generated independently across threads. \n 
/// The key stream generated by encrypting the counter array(s), is used as a source of random, and XOR'd with the message input to produce the cipher text.</para>
///
/// <description>Implementation Notes:</description>
/// <list type="bullet">
/// <item><description>In ICM mode, Encryption/Decryption can both be pipelined (SSE3-128 or AVX-256), and multi-threaded.</description></item>
/// <item><description>A cipher mode constructor can either be initialized with a block-cipher instance, or using the block ciphers enumeration name.</description></item>
/// <item><description>A block-cipher instance created using the enumeration constructor, is automatically deleted when the class is destroyed.</description></item>
/// <item><description>The class functions are virtual, and can be accessed from an ICipherMode instance.</description></item>
/// <item><description>The EncryptBlock function can only be accessed through the class instance.</description></item>
/// <item><description>The transformation methods can not be called until the Initialize(bool, ISymmetricKey) function has been called.</description></item>
/// <item><description>If the system supports Parallel processing, and IsParallel() is set to true; passing an input block of ParallelBlockSize() to the transform will be auto parallelized.</description></item>
/// <item><description>The ParallelThreadsMax() property is used as the thread count in the parallel loop; this must be an even number no greater than the number of processer cores on the system.</description></item>
/// <item><description>ParallelBlockSize() is calculated automatically based on the processor(s) L1 data cache size, this property can be user defined, and must be evenly divisible by ParallelMinimumSize().</description></item>
/// <item><description>The ParallelBlockSize() can be changed through the ParallelProfile() property; parallel processing can be disabled by setting IsParallel() to false in the ParallelProfile() accessor.</description></item>
/// </list>
/// 
/// <description>Guiding Publications:</description>
/// <list type="number">
/// <item><description>NIST <a href="http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf">SP800-38A</a>.</description></item>
/// <item><description>Comments to NIST concerning AES Modes of Operations: <a href="http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/ctr/ctr-spec.pdf">CTR-Mode Encryption</a>.</description></item>
/// <item><description>Handbook of Applied Cryptography <a href="http://cacr.uwaterloo.ca/hac/about/chap7.pdf">Chapter 7: Block Ciphers</a>.</description></item>
/// </list>
/// </remarks>
class ICM final : public ICipherMode
{
private:

	static const size_t BLOCK_SIZE = 16;

	class IcmState;
	std::unique_ptr<IcmState> m_icmState;
	std::unique_ptr<IBlockCipher> m_blockCipher;
	ParallelOptions m_parallelProfile;

public:

	//~~~Constructor~~~//

	/// <summary>
	/// Copy constructor: copy is restricted, this function has been deleted
	/// </summary>
	ICM(const ICM&) = delete;

	/// <summary>
	/// Copy operator: copy is restricted, this function has been deleted
	/// </summary>
	ICM& operator=(const ICM&) = delete;

	/// <summary>
	/// Default constructor: default is restricted, this function has been deleted
	/// </summary>
	ICM() = delete;

	/// <summary>
	/// Initialize the Cipher Mode using a block-cipher type name
	/// </summary>
	///
	/// <param name="CipherType">The enumeration type name of the block-cipher</param>
	///
	/// <exception cref="CryptoCipherModeException">Thrown if an undefined block-cipher type name is used</exception>
	explicit ICM(BlockCiphers CipherType);

	/// <summary>
	/// Initialize the Cipher Mode using a block-cipher instance
	/// </summary>
	///
	/// <param name="Cipher">The uninitialized block-cipher instance; can not be null</param>
	///
	/// <exception cref="CryptoCipherModeException">Thrown if a null block-cipher is used</exception>
	explicit ICM(IBlockCipher* Cipher);

	/// <summary>
	/// Destructor: finalize this class
	/// </summary>
	~ICM() override;

	//~~~Accessors~~~//

	/// <summary>
	/// Read Only: The ciphers internal block-size in bytes
	/// </summary>
	const size_t BlockSize() override;

	/// <summary>
	/// Read Only: The block ciphers enumeration type name
	/// </summary>
	const BlockCiphers CipherType() override;

	/// <summary>
	/// Read Only: A pointer to the underlying block-cipher instance
	/// </summary>
	IBlockCipher* Engine() override;

	/// <summary>
	/// Read Only: The cipher modes enumeration type name
	/// </summary>
	const CipherModes Enumeral() override;

	/// <summary>
	/// Read Only: The operation mode, returns true if initialized for encryption, false for decryption
	/// </summary>
	const bool IsEncryption() override;

	/// <summary>
	/// Read Only: The block-cipher mode has been keyed and is ready to transform data
	/// </summary>
	const bool IsInitialized() override;

	/// <summary>
	/// Read Only: Processor parallelization availability.
	/// <para>Indicates whether parallel processing is available with this mode.
	/// If parallel capable, input/output data arrays passed to the transform must be ParallelBlockSize in bytes to trigger parallelization.</para>
	/// </summary>
	const bool IsParallel() override;

	/// <summary>
	/// Read Only: A vector of allowed cipher-mode input key byte-sizes
	/// </summary>
	const std::vector<SymmetricKeySize> &LegalKeySizes() override;

	/// <summary>
	/// Read Only: The cipher-modes formal class name
	/// </summary>
	const std::string Name() override;

	/// <summary>
	/// Read Only: Parallel block size; the byte-size of the input/output data arrays passed to a transform that trigger parallel processing.
	/// <para>This value can be changed through the ParallelProfile class.</para>
	/// </summary>
	const size_t ParallelBlockSize() override;

	/// <summary>
	/// Read/Write: Contains parallel and SIMD capability flags and sizes
	/// </summary>
	ParallelOptions &ParallelProfile() override;

	//~~~Public Functions~~~//

	/// <summary>
	/// Decrypt a single block of bytes.
	/// <para>Decrypts one block of bytes beginning at a zero index.
	/// Initialize(bool, ISymmetricKey) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input vector of cipher-text bytes</param>
	/// <param name="Output">The output vector of plain-text bytes</param>
	void DecryptBlock(const std::vector<byte> &Input, std::vector<byte> &Output) override;

	/// <summary>
	/// Decrypt a block of bytes with offset parameters.
	/// <para>Decrypts one block of bytes at the designated offsets.
	/// Initialize(bool, ISymmetricKey) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input vector of cipher-text bytes</param>
	/// <param name="InOffset">Starting offset within the input vector</param>
	/// <param name="Output">The output vector of plain-text bytes</param>
	/// <param name="OutOffset">Starting offset within the output vector</param>
	void DecryptBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset) override;

	/// <summary>
	/// Encrypt a single block of bytes. 
	/// <para>Encrypts one block of bytes beginning at a zero index.
	/// Initialize(bool, ISymmetricKey) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input vector of plain-text bytes</param>
	/// <param name="Output">The output vector of cipher-text bytes</param>
	void EncryptBlock(const std::vector<byte> &Input, std::vector<byte> &Output) override;

	/// <summary>
	/// Encrypt a block of bytes using offset parameters. 
	/// <para>Encrypts one block of bytes at the designated offsets.
	/// Initialize(bool, ISymmetricKey) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input vector of plain-text bytes</param>
	/// <param name="InOffset">Starting offset within the input vector</param>
	/// <param name="Output">The output vector of cipher-text bytes</param>
	/// <param name="OutOffset">Starting offset within the output vector</param>
	void EncryptBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset) override;

	/// <summary>
	/// Initialize the cipher-mode instance
	/// </summary>
	/// 
	/// <param name="Encryption">Operation mode, true if cipher is used for encryption, false to decrypt</param>
	/// <param name="Parameters">SymmetricKey containing the encryption Key and Initialization Vector</param>
	/// 
	/// <exception cref="CryptoCipherModeException">Thrown if an invalid key or nonce is used</exception>
	void Initialize(bool Encryption, ISymmetricKey &Parameters) override;

	/// <summary>
	/// Set the maximum number of threads allocated when using multi-threaded processing.
	/// <para>When set to zero, thread count is set automatically. If set to 1, sets IsParallel() to false and runs in sequential mode. 
	/// Thread count must be an even number, and not exceed the number of processor cores.</para>
	/// </summary>
	///
	/// <param name="Degree">The desired number of threads to allocate</param>
	/// 
	/// <exception cref="CryptoCipherModeException">Thrown if the degree parameter is invalid</exception>
	void ParallelMaxDegree(size_t Degree) override;

	/// <summary>
	/// Transform a length of bytes with offset parameters. 
	/// <para>This method processes a specified length of bytes, utilizing offsets incremented by the caller.
	/// If IsParallel() is set to true, and the length is at least ParallelBlockSize(), the transform is run in parallel processing mode.
	/// To disable parallel processing, set the ParallelOptions().IsParallel() property to false.
	/// Initialize(bool, ISymmetricKey) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input vector of bytes to transform</param>
	/// <param name="InOffset">Starting offset within the input vector</param>
	/// <param name="Output">The output vector of transformed bytes</param>
	/// <param name="OutOffset">Starting offset within the output vector</param>
	/// <param name="Length">The number of bytes to transform</param>
	void Transform(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset, const size_t Length) override;

private:

	void Encrypt128(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset);
	void Generate(std::vector<byte> &Output, const size_t OutOffset, const size_t Length, std::vector<ulong> &Counter);
	void ProcessParallel(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset, const size_t Length);
	void ProcessSequential(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset, const size_t Length);
};

NAMESPACE_MODEEND
#endif
