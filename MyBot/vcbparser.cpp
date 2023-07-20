#include "vcbparser.h"

#include<sstream>
#include<streambuf>
#include<fstream>
#include<iostream>
#include<assert.h>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zstd.hpp>
#include <openssl/sha.h>
#include <chrono>
#include "base64.h"
#include "lodepng.h"

std::string decompress(const std::string& data)
{
	namespace bio = boost::iostreams;

	std::stringstream decompressed;
	std::stringstream origin(data);

	bio::filtering_streambuf<bio::input> out;
	out.push(bio::zstd_decompressor(bio::zstd_params(bio::zstd::default_compression)));
	out.push(origin);
	bio::copy(out, decompressed);

	return decompressed.str();
}

uint64_t nbyteToUint(const std::string &s, const uint8_t len, const int offset) {
	assert(len > 0 || len<9);
	uint64_t out = 0;
	for (int i = 0; i < len; i++) {
		out |= ((uint64_t)(unsigned char)s[len - i - 1 + offset] << (i * 8)); // must cast signed char to unsigned char first
	}
	return out;
}

VcbCircuit VcbParser::parseBP(const std::string &input)
{
	VcbCircuit out{};
	std::string blueprint = input;
	// Check identifier
	if (blueprint.rfind("VCB+",0) != 0) {
		throw std::invalid_argument("Blueprint format not recognised!");
	}
	// Check version
	std::string decoded;
	macaron::Base64::Decode(blueprint.substr(4, 12), decoded);
	out.version = nbyteToUint(decoded, 3, 0);
	if (out.version != 0) {
		throw std::invalid_argument("Version not recognised!");
	}
	// Check SHA1 checksum
	out.checksum = nbyteToUint(decoded, 6, 3);
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1((const unsigned char*)blueprint.c_str()+16, blueprint.size()-16, hash);
	uint64_t sha1 =
		(uint64_t)hash[0] << 40 |
		(uint64_t)hash[1] << 32 |
		(uint64_t)hash[2] << 24 |
		(uint64_t)hash[3] << 16 |
		(uint64_t)hash[4] << 8  |
		(uint64_t)hash[5];
	if (out.checksum != sha1) {
		throw std::invalid_argument("Checksum is not valid!");
	}
	std::string content;
	macaron::Base64::Decode(blueprint.substr(16, blueprint.size()-16), content);
	// getting width, height
	out.width = nbyteToUint(content, 4, 0);
	out.height = nbyteToUint(content, 4, 4);
	if (std::min(out.width, out.height) < 1) {
		throw std::invalid_argument("Blueprint is smaller than a pixel!");
	}
	int j = 8;
	while (j < content.size()) {
		VcbBlock block;
		block.blockSize = nbyteToUint(content, 4, j);
		if (j + block.blockSize > content.size()) {
			throw std::invalid_argument("BlockSize is out of bounds");
		}
		if (block.blockSize < 12) {
			throw std::invalid_argument("BlockSize is too small");
		}
		block.layerId = nbyteToUint(content, 4, j+4);
		block.uncompressedSize = nbyteToUint(content, 4, j+8);
		if (block.uncompressedSize > 2048*2048*4*4) { // check if it is larger than the default 2k*2k board (no trolling lol)
			throw std::invalid_argument("Blueprint is larger than the board!");
		}
		if (block.uncompressedSize != out.width * out.height * 4) {
			throw std::invalid_argument("UncompressedSize is different than the bp size!");
		}
		if (block.layerId == 0) {
			if (out.logic != -1) {
				throw std::invalid_argument("Duplicate Logic layer!");
			}
			std::string tempBuf = "";

			try
			{
				tempBuf = decompress(content.substr(j + 12, block.blockSize-12));
			}
			catch (const std::exception&)
			{
				throw std::invalid_argument("Compressed data is likely corrupt!");
			}

			if (tempBuf.size() != block.uncompressedSize) {
				throw std::invalid_argument("UncompressedSize is not correct!");
			}
			block.buffer = std::vector<unsigned char>(tempBuf.begin(), tempBuf.end());
			out.logic = out.blocks.size();
		}
		out.blocks.push_back(block);
		if (j + block.blockSize > content.size() - 20) {
			break;
		}
		j += block.blockSize;
	}
	if (out.logic == -1) {
		throw std::invalid_argument("Missing Logic layer!");
	}
	return out;
}

#ifdef TEST_PARSER

void VcbParser::test()
{
	std::vector<std::string> cases = {
		"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"
	};
	for (int i = 0; i < cases.size(); i++) {
		try {
			(void)parseBP(cases[i]);
		}
		catch(std::invalid_argument &e){
			std::cout << "[TEST " << i+1 << "]: " << e.what() << '\n';
			continue;
		}
		std::cout << "[TEST " << i + 1 << "]: Accepted BP." << '\n';
	}
	std::cout << "All tests passed!\nStarting speedtest!\n";
	std::ifstream file;
	std::stringstream buffer;
	file.open("long_bp.txt", std::ios::in | std::ios::binary);
	if (file.is_open()) {
		buffer << file.rdbuf();
	}
	else throw std::exception("failed to open long_bp.txt file for testing");
	file.close();
	std::string long_bp = buffer.str();

	const auto start = std::chrono::steady_clock::now();

	VcbCircuit circ = parseBP(long_bp);
	std::vector<unsigned char> png;
	lodepng::encode(png, circ.blocks[circ.logic].buffer, circ.width, circ.height);

	const auto end = std::chrono::steady_clock::now();
	const std::chrono::duration<double, std::milli> time = end - start;
	std::cout << "[INFO]: rendering BP took " << time.count() << "ms!\n";
}

#endif // TEST_PARSER