#ifndef _CEXTEST_XSPPRSGTEST_H
#define _CEXTEST_XSPPRSGTEST_H

#include "ITest.h"

namespace Test
{
	/// <summary>
	/// XSPRsg known answer tests.
	/// <para>Test vectors generated by the C implementations:
	/// <see href="http://xorshift.di.unimi.it/xorshift128plus.c"/> and 
	/// <see href="http://xorshift.di.unimi.it/xorshift1024star.c"/></para>
	/// </summary>
	class XSPRsgTest : public ITest
	{
	private:
		const std::string DESCRIPTION = "XorShift+ Known Answer Test Vectors.";
		const std::string FAILURE = "FAILURE! ";
		const std::string SUCCESS = "SUCCESS! All XSPRsg tests have executed succesfully.";

		TestEventHandler m_progressEvent;
		std::vector<std::vector<byte>> m_expected;
		std::vector<std::vector<uint64_t>> m_input;

	public:
		/// <summary>
		/// Get: The test description
		/// </summary>
		virtual const std::string Description() { return DESCRIPTION; }

		/// <summary>
		/// Progress return event callback
		/// </summary>
		virtual TestEventHandler &Progress() { return m_progressEvent; }

		/// <summary>
		/// Compares known answer XSPRsg vectors for equality
		/// </summary>
		XSPRsgTest()
			:
			m_input(4, std::vector<uint64_t>(4))
		{

		}

		/// <summary>
		/// Destructor
		/// </summary>
		~XSPRsgTest()
		{
		}

		/// <summary>
		/// Start the tests
		/// </summary>
		virtual std::string Run();

	private:
		void CompareVector(std::vector<uint64_t> &Input, std::vector<byte> &Expected);
		void Initialize();
		void OnProgress(char* Data);
	};
}

#endif