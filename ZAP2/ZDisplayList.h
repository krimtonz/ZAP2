#pragma once

#include "ZResource.h"
#include "tinyxml2.h"

#include <vector>
#include <string>

enum F3DZEXOpcode
{
	G_NOOP = 0x00,
	G_VTX = 0x01,
	G_MODIFYVTX = 0x02,
	G_CULLDL = 0x03,
	G_BRANCH_Z = 0x04,
	G_TRI1 = 0x05,
	G_TRI5 = 0x06,
	G_QUAD = 0x07,
	G_SPECIAL_3 = 0xD3,
	G_SPECIAL_2 = 0xD4,
	G_SPECIAL_1 = 0xD5,
	G_DMA_IO = 0xD6,
	G_TEXTURE = 0xD7,
	G_POPMTX = 0xD8,
	G_GEOMETRYMODE = 0xD9,
	G_MTX = 0xDA,
	G_MOVEWORD = 0xDB,
	G_MOVEMEM = 0xDC,
	G_LOAD_UCODE = 0xDD,
	G_DL = 0xDE,
	G_ENDDL = 0xDF,
	G_SPNOOP = 0xE0,
	G_RDPHALF_1 = 0xE1,
	G_SETOTHERMODE_L = 0xE2,
	G_SETOTHERMODE_H = 0xE3,
	G_TEXRECT = 0xE4,
	G_TEXRECTFLIP = 0xE5,
	G_RDPLOADSYNC = 0xE6,
	G_RDPPIPESYNC = 0xE7,
	G_RDPTILESYNC = 0xE8,
	G_RDPFULLSYNC = 0xE9,
	G_SETKEYGB = 0xEA,
	G_SETKEYR = 0xEB,
	G_SETCONVERT = 0xEC,
	G_SETSCISSOR = 0xED,
	G_SETPRIMDEPTH = 0xEE,
	G_RDPSETOTHERMODE = 0xEF,
	G_LOADTLUT = 0xF0,
	G_RDPHALF_2 = 0xF1,
	G_SETTILESIZE = 0xF2,
	G_LOADBLOCK = 0xF3,
	G_LOADTILE = 0xF4,
	G_SETTILE = 0xF5,
	G_FILLRECT = 0xF6,
	G_SETFILLCOLOR = 0xF7,
	G_SETFOGCOLOR = 0xF8,
	G_SETBLENDCOLOR = 0xF9,
	G_SETPRIMCOLOR = 0xFA,
	G_SETENVCOLOR = 0xFB,
	G_SETCOMBINE = 0xFC,
	G_SETTIMG = 0xFD,
	G_SETZIMG = 0xFE,
	G_SETCIMG = 0xFF
};

class ZDisplayList : public ZResource
{
protected:
	std::vector<uint64_t> instructions;

	//void ParseXML(tinyxml2::XMLElement* reader);
	void ParseRawData();
	void PrepareSourceOutput();
public:
	ZDisplayList(tinyxml2::XMLElement* reader, std::vector<uint8_t> nRawData, int rawDataIndex, std::string nRelPath);

	std::vector<uint8_t> GetRawData();
	int GetRawDataSize();
	void Save(std::string outFolder);
};