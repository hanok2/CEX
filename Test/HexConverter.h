#ifndef _CEXTEST_HEXCONVERTER_H
#define _CEXTEST_HEXCONVERTER_H

#include <string.h>
#include <vector>
#include "../CEX/Common.h"

namespace Test
{
	/// <summary>
	/// A Hexadecimal conversion helper class
	/// </summary>
	class HexConverter
	{
	private:
		static constexpr byte m_encBytes[16] =
		{
			(byte)'0', (byte)'1', (byte)'2', (byte)'3', (byte)'4', (byte)'5', (byte)'6', (byte)'7',
			(byte)'8', (byte)'9', (byte)'a', (byte)'b', (byte)'c', (byte)'d', (byte)'e', (byte)'f'
		};

	public:
		static void Decode(const std::string &Input, std::vector<byte> &Output)
		{
			size_t end = Input.size();
			Output.resize(end / 2, 0);

			while (end > 0)
			{
				if (!Ignore(Input[end - 1]))
					break;

				end--;
			}

			size_t i = 0;
			size_t j = 0;
			size_t length = 0;
			std::vector<byte> decTable = GetDecodingTable();

			while (i < end)
			{
				while (i < end && Ignore(Input[i]))
					i++;

				byte b1 = decTable[Input[i++]];

				while (i < end && Ignore(Input[i]))
					i++;

				byte b2 = decTable[Input[i++]];
				Output[j++] = (byte)((b1 << 4) | b2);
				length++;
			}
		}

		static void Decode(const std::vector<std::string> &Input, std::vector<std::vector<byte>> &Output)
		{
			Output.clear();

			for (size_t i = 0; i < Input.size(); ++i)
			{
				const std::string str = Input[i];
				std::vector<byte> temp;
				Decode(str, temp);
				Output.push_back(temp);
			}
		}

		static void Decode(const char *InputArray[], size_t Length, std::vector<std::vector<byte>> &OutputArray)
		{
			OutputArray.reserve(Length);

			for (size_t i = 0; i < Length; ++i)
			{
				std::string encoded = InputArray[i];
				std::vector<byte> decoded;
				Decode(encoded, decoded);
				OutputArray.push_back(decoded);
			}
		}

		static void Encode(const std::vector<byte> &Input, size_t Offset, size_t Length, std::vector<byte> &Output)
		{
			Output.resize(Length * 2, 0);
			size_t counter = 0;
			std::vector<byte> encTable = GetEncodingTable();

			for (size_t i = Offset; i < (Offset + Length); i++)
			{
				int vct = Input[i];
				Output[counter++] = encTable[vct >> 4];
				Output[counter++] = encTable[vct & 0xf];
			}
		}

		static bool Ignore(char Value)
		{
			return (Value == '\n' || Value == '\r' || Value == '\t' || Value == ' ');
		}

		static void ToString(const std::vector<byte> &Input, std::string &Output)
		{
			std::vector<byte> encoded;
			Encode(Input, 0, Input.size(), encoded);
			Output.assign((char*)&encoded[0], encoded.size());
		}

	private:
		static std::vector<byte> GetEncodingTable()
		{
			std::vector<byte> encTable;
			encTable.reserve(sizeof(m_encBytes));
			for (size_t i = 0; i < sizeof(m_encBytes); ++i)
				encTable.push_back(m_encBytes[i]);

			return encTable;
		}

		static std::vector<byte> GetDecodingTable()
		{
			std::vector<byte> encTable = GetEncodingTable();
			std::vector<byte> decTable(128, 0);

			for (size_t i = 0; i < encTable.size(); i++)
				decTable[encTable[i]] = (byte)i;

			decTable['A'] = decTable['a'];
			decTable['B'] = decTable['b'];
			decTable['C'] = decTable['c'];
			decTable['D'] = decTable['d'];
			decTable['E'] = decTable['e'];
			decTable['F'] = decTable['f'];

			return decTable;
		}
	};
}

#endif
