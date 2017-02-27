#include "KeyFactoryTest.h"
#include "../CEX/CipherDescription.h"
#include "../CEX/CipherKey.h"
#include "../CEX/SecureRandom.h"
#include "../CEX/KeyFactory.h"
#include "../CEX/KeyGenerator.h"
#include "../CEX/MemoryStream.h"
#include "../CEX/MessageHeader.h"

namespace Test
{
	std::string KeyFactoryTest::Run()
	{
		try
		{
			CompareKeySerialization();
			OnProgress("Passed SymmetricKey serialization tests..");

			CompareKeyExtraction();
			OnProgress("Passed KeyFactory creation and extraction tests..");

			CompareCipherKey();
			OnProgress("Passed CipherKey serialization and access tests..");

			CompareMessageHeader();
			OnProgress("Passed MessageHeader serialization and access tests..");

			return SUCCESS;
		}
		catch (std::exception const &ex)
		{
			throw TestException(std::string(FAILURE + " : " + ex.what()));
		}
		catch (...)
		{
			throw TestException(std::string(FAILURE + " : Internal Error"));
		}
	}

	void KeyFactoryTest::CompareKeySerialization()
	{
		Prng::SecureRandom rnd;
		Key::Symmetric::KeyGenerator kg;

		for (unsigned int i = 0; i < 10; ++i)
		{
			// out-bound funcs return pointer to obj
			Key::Symmetric::SymmetricKey kp = kg.GetKeyParams(rnd.NextInt32(1, 1024), rnd.NextInt32(1, 128), rnd.NextInt32(1, 128));
			IO::MemoryStream* m = Key::Symmetric::SymmetricKey::Serialize(kp);
			Key::Symmetric::SymmetricKey* kp2 = Key::Symmetric::SymmetricKey::DeSerialize(*m);

			if (!kp.Equals(*kp2))
				throw std::exception("KeyFactoryTest: Key serialization test has failed!");

			delete kp2;
			delete m;
		}
	}

	void KeyFactoryTest::CompareKeyExtraction()
	{
		using namespace Enumeration;

		Key::Symmetric::KeyGenerator kg;
		Key::Symmetric::SymmetricKey kp = kg.GetKeyParams(192, 16, 64);

		Processing::Structure::CipherDescription ds(
			SymmetricEngines::RHX,
			192,
			IVSizes::V128,
			CipherModes::CTR,
			PaddingModes::PKCS7,
			BlockSizes::B128,
			RoundCounts::R22,
			Digests::Skein512,
			64,
			Digests::SHA512);

		// in/out use a pointer
		IO::MemoryStream* m = new IO::MemoryStream;
		Processing::Factory::KeyFactory kf(m);
		kf.Create(ds, kp);
		// init new instance w/ populated stream
		m->Seek(0, IO::SeekOrigin::Begin);
		Processing::Factory::KeyFactory kf2(m);
		// extract key and desc from stream
		Processing::Structure::CipherKey ck;
		Key::Symmetric::SymmetricKey kp2;
		kf2.Extract(ck, kp2);
		Processing::Structure::CipherDescription ds2 = ck.Description();

		if (!ds.Equals(ds2))
			throw std::exception("KeyFactoryTest: Description extraction has failed!");
		if (!kp.Equals(kp2))
			throw std::exception("KeyFactoryTest: Key extraction has failed!");

		IO::MemoryStream* m2 = new IO::MemoryStream;
		Processing::Factory::KeyFactory kf3(m2);
		// test other create func
		kf3.Create(kp, SymmetricEngines::RHX, 192, IVSizes::V128, CipherModes::CTR, PaddingModes::PKCS7,
			BlockSizes::B128, RoundCounts::R22, Digests::Skein512, 64, Digests::SHA512);

		m2->Seek(0, IO::SeekOrigin::Begin);
		Processing::Factory::KeyFactory kf4(m2);
		kf4.Extract(ck, kp2);
		Processing::Structure::CipherDescription ds3 = ck.Description();

		if (!ds.Equals(ds2))
			throw std::exception("KeyFactoryTest: Description extraction has failed!");
		if (!kp.Equals(kp2))
			throw std::exception("KeyFactoryTest: Key extraction has failed!");

		delete m;
		delete m2;
	}

	void KeyFactoryTest::CompareCipherKey()
	{
		using namespace Enumeration;

		Processing::Structure::CipherDescription ds(
			SymmetricEngines::RHX,
			192,
			IVSizes::V128,
			CipherModes::CTR,
			PaddingModes::PKCS7,
			BlockSizes::B128,
			RoundCounts::R22,
			Digests::Skein512,
			64,
			Digests::SHA512);

		Prng::SecureRandom rnd;
		std::vector<byte> id(16);
		std::vector<byte> ek(16);
		rnd.GetBytes(id);
		rnd.GetBytes(ek);

		// test serialization
		Processing::Structure::CipherKey ck(ds, id, ek);
		std::vector<byte> sk = ck.ToBytes();
		Processing::Structure::CipherKey ck2(sk);
		if (!ck.Equals(ck2))
			throw std::exception("KeyFactoryTest: CipherKey serialization has failed!");

		IO::MemoryStream* mk = ck.ToStream();
		Processing::Structure::CipherKey ck3(*mk);
		if (!ck.Equals(ck3))
			throw std::exception("KeyFactoryTest: CipherKey serialization has failed!");

		// test access funcs
		ck.SetCipherDescription(*mk, ds);
		Processing::Structure::CipherDescription* ds2 = ck.GetCipherDescription(*mk);
		if (!ck.Description().Equals(*ds2))
			throw std::exception("KeyFactoryTest: CipherKey access has failed!");
		delete ds2;

		rnd.GetBytes(ek);
		ck.SetExtensionKey(*mk, ek);
		if (ck.GetExtensionKey(*mk) != ek)
			throw std::exception("KeyFactoryTest: CipherKey access has failed!");

		rnd.GetBytes(id);
		ck.SetKeyId(*mk, id);
		if (ck.GetKeyId(*mk) != id)
			throw std::exception("KeyFactoryTest: CipherKey access has failed!");

		delete mk;
	}

	void KeyFactoryTest::CompareMessageHeader()
	{
		Prng::SecureRandom rnd;
		std::vector<byte> id(16);
		std::vector<byte> ex(16);
		std::vector<byte> ha(64);
		rnd.GetBytes(id);
		rnd.GetBytes(ex);
		rnd.GetBytes(ha);

		// test serialization
		Processing::Structure::MessageHeader mh(id, ex, ha);
		std::vector<byte> sk = mh.ToBytes();
		Processing::Structure::MessageHeader mh2(sk);
		if (!mh.Equals(mh2))
			throw std::exception("KeyFactoryTest: MessageHeader serialization has failed!");

		IO::MemoryStream* mk = mh.ToStream();
		Processing::Structure::MessageHeader mh3(*mk, 64);
		if (!mh.Equals(mh3))
			throw std::exception("KeyFactoryTest: MessageHeader serialization has failed!");

		std::vector<byte> ha2 = mh.GetMessageMac(*mk, 64);
		if (ha != ha2)
			throw std::exception("KeyFactoryTest: MessageHeader access has failed!");

		std::vector<byte> id2 = mh.GetKeyId(*mk);
		if (id != id2)
			throw std::exception("KeyFactoryTest: MessageHeader access has failed!");

		std::vector<byte> ex2 = mh.GetExtensionKey(*mk);
		if (ex != ex2)
			throw std::exception("KeyFactoryTest: MessageHeader access has failed!");

		std::string ext1 = "test";
		std::vector<byte> enc = mh.EncryptExtension(ext1, mh.GetExtensionKey(*mk));
		std::string ext2 = mh.DecryptExtension(enc, mh.GetExtensionKey(*mk));
		if (ext1.compare(ext2) != 0)
			throw std::exception("KeyFactoryTest: MessageHeader access has failed!");

		delete mk;
	}

	void KeyFactoryTest::OnProgress(char* Data)
	{
		m_progressEvent(Data);
	}
}