#pragma once
#include<stdint.h>
#include<string>
#include<vector>

struct VcbBlock
{
	uint32_t blockSize{};
	uint32_t layerId{};
	uint32_t uncompressedSize{};
	std::vector<uint32_t> buffer;
};

struct VcbCircuit
{
	uint32_t version;
	uint64_t checksum;
	uint32_t width;
	uint32_t height;
	std::vector<VcbBlock> blocks;
	int logic{-1};
};

class VcbParser
{
public:
	static VcbCircuit parseBP(const std::string &input);
	static void test();
private:
	VcbParser();
};