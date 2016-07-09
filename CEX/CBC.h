// The MIT License (MIT)
// 
// Copyright (c) 2016 vtdev.com
// This file is part of the CEX Cryptographic library.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// 
// Implementation Details:
// An implementation of a Cipher Block Chaining mode (CBC).
// Written by John Underhill, September 24, 2014
// contact: develop@vtdev.com

#ifndef _CEXENGINE_CBC_H
#define _CEXENGINE_CBC_H

#include "ICipherMode.h"

NAMESPACE_MODE

/// <summary>
/// Implements a Cipher Block Chaining Mode: CBC
/// </summary> 
/// 
/// <example>
/// <description>Example using an <c>ICipherMode</c> interface:</description>
/// <code>
/// CBC cipher(new RDX());
/// // initialize for encryption
/// cipher.Initialize(true, KeyParams(Key, IV));
/// // encrypt a block
/// cipher.Transform(Input, Output);
/// </code>
/// </example>
/// 
/// <seealso cref="CEX::Cipher::Symmetric::Block"/>
/// <seealso cref="CEX::Cipher::Symmetric::Block::Mode::ICipherMode"/>
/// 
/// <remarks>
/// <description>Implementation Notes:</description>
/// <list type="bullet">
/// <item><description>In CFB mode, only decryption can be processed in parallel.</description></item>
/// <item><description>Parallel processing is enabled on decryption by passing a block size of ParallelBlockSize to the transform.</description></item>
/// <item><description>ParallelBlockSize must be divisible by ParallelMinimumSize.</description></item>
/// <item><description>Parallel block calculation ex. <c>int blocklen = (data.size() / cipher.ParallelMinimumSize()) * 100</c></description></item>
/// </list>
/// 
/// <description>Guiding Publications:</description>
/// <list type="number">
/// <item><description>NIST <a href="http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf">SP800-38A</a>.</description></item>
/// </list>
/// </remarks>
class CBC : public ICipherMode
{
private:
	static constexpr size_t PARALLEL_DEFBLOCK = 64000;
	static constexpr size_t MAXALLOC_MB100 = 100000000;

	IBlockCipher* m_blockCipher;
	size_t m_blockSize;
	std::vector<byte> m_cbcIv;
	std::vector<byte> m_cbcNextIv;
	bool m_isDestroyed;
	bool m_isEncryption;
	bool m_isInitialized;
	bool m_isParallel;
	size_t m_parallelBlockSize;
	size_t m_processorCount;
	std::vector<std::vector<byte>> m_threadVectors;

public:

	// *** Properties *** //

	/// <summary>
	/// Get: Unit block size of internal cipher
	/// </summary>
	virtual const size_t BlockSize() { return m_blockSize; }

	/// <summary>
	/// Get: Underlying Cipher
	/// </summary>
	virtual IBlockCipher* Engine() { return m_blockCipher; }

	/// <summary>
	/// Get: The cipher modes type name
	/// </summary>
	virtual const CEX::Enumeration::CipherModes Enumeral() { return CEX::Enumeration::CipherModes::CBC; }

	/// <summary>
	/// Get: Initialized for encryption, false for decryption
	/// </summary>
	virtual const bool IsEncryption() { return m_isEncryption; }

	/// <summary>
	/// Get: Cipher is ready to transform data
	/// </summary>
	virtual const bool IsInitialized() { return m_isInitialized; }

	/// <summary>
	/// Get/Set: Automatic processor parallelization
	/// </summary>
	virtual bool &IsParallel() { return m_isParallel; }

	/// <summary>
	/// Get: The current state of the initialization Vector
	/// </summary>
	virtual const std::vector<byte> &IV() { return m_cbcIv; }

	/// <summary>
	/// Get: Available Encryption Key Sizes in bytes
	/// </summary>
	virtual const std::vector<size_t> &LegalKeySizes() { return m_blockCipher->LegalKeySizes(); }

	/// <summary>
	/// Get: Cipher name
	/// </summary>
	virtual const char* Name() { return "CBC"; }

	/// <summary>
	/// Get: Parallel block size.
	/// </summary>
	virtual const size_t ParallelBlockSize() { return m_parallelBlockSize; }

	/// <summary>
	/// Set: Parallel block size. Must be a multiple of <see cref="ParallelMinimumSize"/>.
	/// </summary>
	virtual void ParallelBlockSize(size_t BlockSize)
	{
		SetScope();
		m_parallelBlockSize = BlockSize;
	}

	/// <summary>
	/// Get: Maximum input size with parallel processing
	/// </summary>
	virtual const size_t ParallelMaximumSize() { return MAXALLOC_MB100; }

	/// <summary>
	/// Get: The smallest parallel block size. Parallel blocks must be a multiple of this size.
	/// </summary>
	virtual const size_t ParallelMinimumSize() { return m_processorCount * m_blockSize; }

	/// <remarks>
	/// Get: Processor count
	/// </remarks>
	virtual const size_t ProcessorCount() { return m_processorCount; }


	// ** Constructor ** //

	/// <summary>
	/// Initialize the Cipher
	/// </summary>
	///
	/// <param name="Cipher">Underlying encryption cipher</param>
	///
	/// <exception cref="CEX::Exception::CryptoCipherModeException">Thrown if a null Cipher is used</exception>
	explicit CBC(IBlockCipher* Cipher)
		:
		m_blockCipher(Cipher),
		m_blockSize(Cipher->BlockSize()),
		m_isDestroyed(false),
		m_isEncryption(false),
		m_isInitialized(false),
		m_isParallel(false),
		m_processorCount(1),
		m_parallelBlockSize(PARALLEL_DEFBLOCK)
	{
		if (Cipher == 0)
			throw CryptoCipherModeException("CBC:CTor", "The Cipher can not be null!");

		SetScope();
	}

	/// <summary>
	/// Finalize objects
	/// </summary>
	virtual ~CBC()
	{
		Destroy();
	}

	// *** Public Methods *** //

	/// <summary>
	/// Decrypt a block of bytes.
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Encrypted bytes</param>
	/// <param name="Output">Decrypted bytes</param>
	void DecryptBlock(const std::vector<byte> &Input, std::vector<byte> &Output);

	/// <summary>
	/// Decrypt a block of bytes with offset parameters. 
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Encrypted bytes</param>
	/// <param name="InOffset">Offset in the Input array</param>
	/// <param name="Output">Decrypted bytes</param>
	/// <param name="OutOffset">Offset in the Output array</param>
	void DecryptBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset);

	/// <summary>
	/// Release all resources associated with the object
	/// </summary>
	virtual void Destroy();

	/// <summary>
	/// Encrypt a block of bytes. 
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Input bytes to Transform</param>
	/// <param name="Output">Output product of Transform</param>
	void EncryptBlock(const std::vector<byte> &Input, std::vector<byte> &Output);

	/// <summary>
	/// Encrypt a block of bytes with offset parameters. 
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Input bytes to Transform</param>
	/// <param name="InOffset">Offset in the Input array</param>
	/// <param name="Output">Output product of Transform</param>
	/// <param name="OutOffset">Offset in the Output array</param>
	void EncryptBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset);

	/// <summary>
	/// Initialize the Cipher
	/// </summary>
	/// 
	/// <param name="Encryption">True if cipher is used for encryption, false to decrypt</param>
	/// <param name="KeyParam">KeyParam containing key and std::vector</param>
	virtual void Initialize(bool Encryption, const CEX::Common::KeyParams &KeyParam);

	/// <summary>
	/// Transform a block of bytes. Parallel capable in Decryption mode.
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	///
	/// <param name="Input">Input bytes to Transform</param>
	/// <param name="Output">Output product of Transform</param>
	virtual void Transform(const std::vector<byte> &Input, std::vector<byte> &Output);

	/// <summary>
	/// Transform a block of bytes with offset parameters.
	/// <para> Parallel capable in Decryption mode.
	/// Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	///
	/// <param name="Input">Input bytes to Transform</param>
	/// <param name="InOffset">Offset in the Input array</param>
	/// <param name="Output">Output product of Transform</param>
	/// <param name="OutOffset">Offset in the Output array</param>
	virtual void Transform(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset);

private:
	void ParallelDecrypt(const std::vector<byte> &Input, std::vector<byte> &Output);
	void ParallelDecrypt(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset);
	void ProcessDecrypt(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset, std::vector<byte> &Iv, const size_t BlockCount);
	void SetScope();
};

NAMESPACE_MODEEND
#endif
