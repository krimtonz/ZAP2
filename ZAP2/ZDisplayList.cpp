#include "ZDisplayList.h"
#include "BitConverter.h"
#include "StringHelper.h"
#include "Globals.h"
#include <chrono>
#include <algorithm>

using namespace std;
using namespace tinyxml2;

ZDisplayList::ZDisplayList() : ZResource()
{
	defines = "";
	sceneSegName = "";
	lastTexWidth = 0;
	lastTexHeight = 0;
	lastTexAddr = 0;
	lastTexFmt = F3DZEXTexFormats::G_IM_FMT_RGBA;
	lastTexSiz = F3DZEXTexSizes::G_IM_SIZ_16b;
	lastTexSizTest = F3DZEXTexSizes::G_IM_SIZ_16b;
	lastTexLoaded = false;
	name = "";
	scene = nullptr;

	vertices = map<uint32_t, vector<Vertex>>();
	vtxDeclarations = map<uint32_t, string>();
	otherDLists = vector<ZDisplayList*>();
	textures = map<uint32_t, ZTexture*>();
	texDeclarations = map<uint32_t, std::string>();
}

// EXTRACT MODE
ZDisplayList* ZDisplayList::ExtractFromXML(XMLElement* reader, vector<uint8_t> nRawData, int nRawDataIndex, int rawDataSize, string nRelPath)
{
	ZDisplayList* dList = new ZDisplayList();

	dList->name = reader->Attribute("Name");

	dList->rawData = nRawData;
	dList->rawDataIndex = nRawDataIndex;
	dList->fileData = dList->rawData;
	dList->relativePath = nRelPath;
	dList->rawData = vector<uint8_t>(dList->rawData.data() + dList->rawDataIndex, dList->rawData.data() + dList->rawDataIndex + rawDataSize);
	dList->ParseRawData();

	return dList;
}

ZDisplayList::ZDisplayList(vector<uint8_t> nRawData, int nRawDataIndex, int rawDataSize) : ZDisplayList()
{
	fileData = nRawData;
	//dListAddress = nRawDataIndex;
	rawDataIndex = nRawDataIndex;
	name = StringHelper::Sprintf("dlist_%08X", rawDataIndex);
	rawData = vector<uint8_t>(nRawData.data() + rawDataIndex, nRawData.data() + rawDataIndex + rawDataSize);
	ParseRawData();
}

void ZDisplayList::ParseRawData()
{
	int numInstructions = rawData.size() / 8;
	uint8_t* rawDataArr = rawData.data();

	instructions.reserve(numInstructions);

	for (int i = 0; i < numInstructions; i++)
		instructions.push_back(BitConverter::ToInt64BE(rawDataArr, (i * 8)));
}

int ZDisplayList::GetDListLength(vector<uint8_t> rawData, int rawDataIndex)
{
	int i = 0;

	while (true)
	{
		F3DZEXOpcode opcode = (F3DZEXOpcode)rawData[rawDataIndex + (i * 8)];

		i++;

		if (opcode == F3DZEXOpcode::G_ENDDL)
			return i * 8;
	}
}

string ZDisplayList::GetSourceOutputHeader(string prefix)
{
	char line[4096];
	string sourceOutput = "";

	//sprintf(line, "extern Gfx _%s_dlist_%08X[];\n", prefix.c_str(), dListAddress);
	sprintf(line, "extern Gfx _%s_%s[];\n", prefix.c_str(), name.c_str());
	sourceOutput += line;

	return "";
}

string ZDisplayList::GetSourceOutputCode(std::string prefix)
{
	char line[4096];
	string sourceOutput = "";

	sourceOutput += StringHelper::Sprintf("Gfx _%s_%s[] =\n{\n", prefix.c_str(), name.c_str());

	for (int i = 0; i < instructions.size(); i++) 
	{
		F3DZEXOpcode opcode = (F3DZEXOpcode)rawData[(i * 8) + 0];
		uint64_t data = instructions[i];
		sourceOutput += "\t";

		auto start = chrono::steady_clock::now();

		switch (opcode)
		{
		case F3DZEXOpcode::G_NOOP:
			sprintf(line, "gsDPNoOpTag(0x%08X),", data & 0xFFFFFFFF);
			break;
		case F3DZEXOpcode::G_DL:
		{
			int pp = (data & 0x00FF000000000000) >> 56;

			if (pp != 0)
				sprintf(line, "gsSPBranchList(_%s_dlist_%08X),", prefix.c_str(), data & 0x00FFFFFF);
			else
				sprintf(line, "gsSPDisplayList(_%s_dlist_%08X),", prefix.c_str(), data & 0x00FFFFFF);

			int segmentNumber = (data & 0xFF000000) >> 24;

			if (segmentNumber == 8 || segmentNumber == 9 || segmentNumber == 10 || segmentNumber == 11 || segmentNumber == 12 || segmentNumber == 13) // Used for runtime-generated display lists
			{
				if (pp != 0)
					sprintf(line, "gsSPBranchList(0x%08X),", data & 0xFFFFFFFF);
				else
					sprintf(line, "gsSPDisplayList(0x%08X),", data & 0xFFFFFFFF);
			}
			else
			{
				ZDisplayList* nList = new ZDisplayList(fileData, data & 0x00FFFFFF, GetDListLength(fileData, data & 0x00FFFFFF));
				nList->scene = scene;
				otherDLists.push_back(nList);
			}
		}
			break;
		case F3DZEXOpcode::G_CULLDL:
		{
			int vvvv = (data & 0xFFFF00000000) >> 32;
			int wwww = (data & 0x0000FFFF);

			sprintf(line, "gsSPCullDisplayList(%i, %i),", vvvv / 2, wwww / 2);
		}
		break;
		case F3DZEXOpcode::G_TRI1:
		{
			int aa = ((data & 0x00FF000000000000ULL) >> 48) / 2;
			int bb = ((data & 0x0000FF0000000000ULL) >> 40) / 2;
			int cc = ((data & 0x000000FF00000000ULL) >> 32) / 2;
			sprintf(line, "gsSP1Triangle(%i, %i, %i, 0),", aa, bb, cc);
		}
			break;
		case F3DZEXOpcode::G_TRI2:
		{
			int aa = ((data & 0x00FF000000000000ULL) >> 48) / 2;
			int bb = ((data & 0x0000FF0000000000ULL) >> 40) / 2;
			int cc = ((data & 0x000000FF00000000ULL) >> 32) / 2;
			int dd = ((data & 0x00000000FF0000ULL) >> 16) / 2;
			int ee = ((data & 0x0000000000FF00ULL) >> 8) / 2;
			int ff = ((data & 0x000000000000FFULL) >> 0) / 2;
			sprintf(line, "gsSP2Triangles(%i, %i, %i, 0, %i, %i, %i, 0),", aa, bb, cc, dd, ee, ff);
		}
		break;
		case F3DZEXOpcode::G_VTX:
		{
			int nn = (data & 0x000FF00000000000ULL) >> 44;
			int aa = (data & 0x000000FF00000000ULL) >> 32;
			uint32_t vtxAddr = data & 0x00FFFFFF;
			sprintf(line, "gsSPVertex(_%s_vertices_%08X, %i, %i),", prefix.c_str(), vtxAddr, nn, ((aa >> 1) - nn));
			
			{
				uint32_t currentPtr = data & 0x00FFFFFF;

				vector<Vertex> vtxList = vector<Vertex>();

				vtxList.reserve(nn);

				for (int i = 0; i < nn; i++)
				{
					Vertex vtx = Vertex(fileData, currentPtr);
					vtxList.push_back(vtx);
					
					currentPtr += 16;
				}

				vertices[data & 0x00FFFFFF] = vtxList;
			}

		}
			break;
		case F3DZEXOpcode::G_SETTIMG: // HOTSPOT
		{
			int __ = (data & 0x00FF000000000000) >> 48;
			int www = (data & 0x00000FFF00000000) >> 32;
			string fmtTbl[] = { "G_IM_FMT_RGBA", "G_IM_FMT_YUV", "G_IM_FMT_CI", "G_IM_FMT_IA", "G_IM_FMT_I" };
			string sizTbl[] = { "G_IM_SIZ_4b", "G_IM_SIZ_8b", "G_IM_SIZ_16b", "G_IM_SIZ_32b" };

			uint32_t fmt = (__ & 0xE0) >> 5;
			uint32_t siz = (__ & 0x18) >> 3;

			TextureGenCheck(prefix); // HOTSPOT

			lastTexFmt = (F3DZEXTexFormats)fmt;
			lastTexSiz = (F3DZEXTexSizes)siz;
			lastTexSeg = data;
			lastTexAddr = data & 0x00FFFFFF;

			int segmentNumber = (data >> 24) & 0xFF;

			if (segmentNumber != 2)
			{
				char texStr[2048];

				int32_t texAddress = data & 0x00FFFFFF;

				if (texAddress != 0)
					sprintf(texStr, "_%s_tex_%08X", prefix.c_str(), texAddress);
				else if (segmentNumber != 3)
					sprintf(texStr, "0x%08X", data);
				else
					sprintf(texStr, "0");

				sprintf(line, "gsDPSetTextureImage(%s, %s, %i, %s),", fmtTbl[fmt].c_str(), sizTbl[siz].c_str(), www + 1, texStr);
			}
			else
			{
				//sprintf(line, "gsDPSetTextureImage(%s, %s, %i, 0x%08X),", fmtTbl[fmt].c_str(), sizTbl[siz].c_str(), www + 1, data & 0xFFFFFFFF);
				sprintf(line, "gsDPSetTextureImage(%s, %s, %i, _%s_tex_%08X),", fmtTbl[fmt].c_str(), sizTbl[siz].c_str(), www + 1, scene->GetName().c_str(), data & 0x00FFFFFF);
			}
		}
			break;
		case F3DZEXOpcode::G_GEOMETRYMODE:
		{
			int cccccc = (data & 0x00FFFFFF00000000) >> 32;
			int ssssssss = (data & 0xFFFFFFFF);
			sprintf(line, "gsSPGeometryMode(0x%08X, 0x%08X),", ~cccccc, ssssssss);
		}
		break;
		case F3DZEXOpcode::G_SETPRIMCOLOR:
		{
			int mm = (data & 0x0000FF0000000000) >> 40;
			int ff = (data & 0x000000FF00000000) >> 32;
			int rr = (data & 0x00000000FF000000) >> 24;
			int gg = (data & 0x0000000000FF0000) >> 16;
			int bb = (data & 0x000000000000FF00) >> 8;
			int aa = (data & 0x00000000000000FF) >> 0;
			sprintf(line, "gsDPSetPrimColor(%i, %i, %i, %i, %i, %i),", mm, ff, rr, gg, bb, aa);
		}
		break;
		case F3DZEXOpcode::G_SETOTHERMODE_L:
		{
			int ss = (data & 0x0000FF0000000000) >> 40;
			int nn = (data & 0x000000FF00000000) >> 32;
			int dd = (data & 0xFFFFFFFF);

			sprintf(line, "gsSPSetOtherMode(0xE2, %i, %i, 0x%08X),", 32 - (nn + 1) - ss, nn + 1, dd);
		}
			break;
		case F3DZEXOpcode::G_SETOTHERMODE_H:
		{
			int ss = (data & 0x0000FF0000000000) >> 40;
			int nn = (data & 0x000000FF00000000) >> 32;
			int dd = (data & 0xFFFFFFFF);

			sprintf(line, "gsSPSetOtherMode(0xE3, %i, %i, 0x%08X),", 32 - (nn + 1) - ss, nn + 1, dd);
		}
		break;
		case F3DZEXOpcode::G_SETTILE:
		{
			int fff =          (data & 0b0000000011100000000000000000000000000000000000000000000000000000) >> 53;
			int ii =           (data & 0b0000000000011000000000000000000000000000000000000000000000000000) >> 51;
			int nnnnnnnnn =    (data & 0b0000000000000011111111100000000000000000000000000000000000000000) >> 41;
			int mmmmmmmmm =    (data & 0b0000000000000000000000011111111100000000000000000000000000000000) >> 32;
			int ttt =          (data & 0b0000000000000000000000000000000000000111000000000000000000000000) >> 24;
			int pppp =         (data & 0b0000000000000000000000000000000000000000111100000000000000000000) >> 20;
			int cc =           (data & 0b0000000000000000000000000000000000000000000011000000000000000000) >> 18;
			int aaaa =         (data & 0b0000000000000000000000000000000000000000000000111100000000000000) >> 14;
			int ssss =         (data & 0b0000000000000000000000000000000000000000000000000011110000000000) >> 10;
			int dd =           (data & 0b0000000000000000000000000000000000000000000000000000001100000000) >> 8;
			int bbbb =         (data & 0b0000000000000000000000000000000000000000000000000000000011110000) >> 4;
			int uuuu =         (data & 0b0000000000000000000000000000000000000000000000000000000000001111);

			string fmtTbl[] = { "G_IM_FMT_RGBA", "G_IM_FMT_YUV", "G_IM_FMT_CI", "G_IM_FMT_IA", "G_IM_FMT_I" };
			string sizTbl[] = { "G_IM_SIZ_4b", "G_IM_SIZ_8b", "G_IM_SIZ_16b", "G_IM_SIZ_32b" };

			if (fff == (int)F3DZEXTexFormats::G_IM_FMT_CI)
				lastCISiz = (F3DZEXTexSizes)ii;

			lastTexSizTest = (F3DZEXTexSizes)ii;

			sprintf(line, "gsDPSetTile(%s, %s, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i),", fmtTbl[fff].c_str(), sizTbl[ii].c_str(), nnnnnnnnn, mmmmmmmmm, ttt, pppp, cc, aaaa, ssss, dd, bbbb, uuuu);
		}
			break;
		case F3DZEXOpcode::G_SETTILESIZE:
		{
			int sss = (data & 0x00FFF00000000000) >> 48;
			int ttt = (data & 0x00000FFF00000000) >> 36;
			int uuu = (data & 0x0000000000FFF000) >> 12;
			int vvv = (data & 0x0000000000000FFF);
			int i = (data & 0x000000000F000000) >> 24;

			int shiftAmtW = 2;
			int shiftAmtH = 2;

			if (lastTexSizTest == F3DZEXTexSizes::G_IM_SIZ_8b && lastTexFmt == F3DZEXTexFormats::G_IM_FMT_IA)
				shiftAmtW = 3;

			//if (lastTexFmt == F3DZEXTexFormats::G_IM_FMT_I || lastTexFmt == F3DZEXTexFormats::G_IM_FMT_CI)
			if (lastTexSizTest == F3DZEXTexSizes::G_IM_SIZ_4b)
				shiftAmtW = 3;

			if (lastTexSizTest == F3DZEXTexSizes::G_IM_SIZ_4b && lastTexFmt == F3DZEXTexFormats::G_IM_FMT_IA)
				shiftAmtH = 3;

			lastTexWidth = (uuu >> shiftAmtW) + 1;
			lastTexHeight = (vvv >> shiftAmtH) + 1;
			//printf("lastTexWidth: %i lastTexHeight: %i\n", lastTexWidth, lastTexHeight);

			//TextureGenCheck(prefix);

			sprintf(line, "gsDPSetTileSize(%i, %i, %i, %i, %i),", i, sss, ttt, uuu, vvv);
		}
			break;
		case F3DZEXOpcode::G_LOADBLOCK:
		{
			int sss = (data & 0x00FFF00000000000) >> 48;
			int ttt = (data & 0x00000FFF00000000) >> 36;
			int i = (data & 0x000000000F000000) >> 24;
			int xxx = (data & 0x0000000000FFF000) >> 12;
			int ddd = (data & 0x0000000000000FFF);

			//lastTexHeight = (ddd + 1) / 16;

			lastTexLoaded = true;

			//TextureGenCheck(prefix);

			sprintf(line, "gsDPLoadBlock(%i, %i, %i, %i, %i),", i, sss, ttt, xxx, ddd);
		}
		break;
		case F3DZEXOpcode::G_TEXTURE:
		{
			int ____ = (data & 0x0000FFFF00000000) >> 32;
			int ssss = (data & 0x00000000FFFF0000) >> 16;
			int tttt = (data & 0x000000000000FFFF);
			int lll = (____ & 0x3800) >> 7;
			int ddd = (____ & 0x700) >> 4;
			int nnnnnnn = (____ & 0xFE) >> 1;

			sprintf(line, "gsSPTexture(%i, %i, %i, %i, %i),", ssss, tttt, lll, ddd, nnnnnnn);
		}
		break;
		case F3DZEXOpcode::G_LOADTLUT:
		{
			int t =   (data & 0x0000000007000000) >> 24;
			int ccc = (data & 0x00000000003FF000) >> 14;

			if (lastCISiz == F3DZEXTexSizes::G_IM_SIZ_8b)
			{
				lastTexWidth = 16;
				lastTexHeight = 16;
			}
			else
			{
				lastTexWidth = 4;
				lastTexHeight = 4;
			}

			lastTexLoaded = true;

			TextureGenCheck(prefix);

			sprintf(line, "gsDPLoadTLUTCmd(%i, %i),", t, ccc);
		}
			break;
		case F3DZEXOpcode::G_SETENVCOLOR:
		{
			uint8_t r = (data & 0xFF000000) >> 24;
			uint8_t g = (data & 0x00FF0000) >> 16;
			uint8_t b = (data & 0xFF00FF00) >> 8;
			uint8_t a = (data & 0x000000FF) >> 0;

			sprintf(line, "gsDPSetEnvColor(%i, %i, %i, %i),", r, g, b, a);
		}
			break;
		case F3DZEXOpcode::G_SETCOMBINE:
		{
			int a0 = (data & 0b000000011110000000000000000000000000000000000000000000000000000) >> 52;
			int c0 = (data & 0b000000000001111100000000000000000000000000000000000000000000000) >> 47;
			int aa0 = (data & 0b00000000000000011100000000000000000000000000000000000000000000) >> 44;
			int ac0 = (data & 0b00000000000000000011100000000000000000000000000000000000000000) >> 41;
			int a1 = (data & 0b000000000000000000000011110000000000000000000000000000000000000) >> 37;
			int c1 = (data & 0b000000000000000000000000001111100000000000000000000000000000000) >> 32;
			int b0 = (data & 0b000000000000000000000000000000011110000000000000000000000000000) >> 28;
			int b1 = (data & 0b000000000000000000000000000000000001111000000000000000000000000) >> 24;
			int aa1 = (data & 0b00000000000000000000000000000000000000111000000000000000000000) >> 21;
			int ac1 = (data & 0b00000000000000000000000000000000000000000111000000000000000000) >> 18;
			int d0 = (data & 0b000000000000000000000000000000000000000000000111000000000000000) >> 15;
			int ab0 = (data & 0b00000000000000000000000000000000000000000000000111000000000000) >> 12;
			int ad0 = (data & 0b00000000000000000000000000000000000000000000000000111000000000) >> 9;
			int d1 = (data & 0b000000000000000000000000000000000000000000000000000000111000000) >> 6;
			int ab1 = (data & 0b00000000000000000000000000000000000000000000000000000000111000) >> 3;
			int ad1 = (data & 0b00000000000000000000000000000000000000000000000000000000000111) >> 0;

			string modes[] = { "COMBINED", "TEXEL0", "TEXEL1", "PRIMITIVE", "SHADE", "ENVIRONMENT", "1", "COMBINED_ALPHA", 
				"TEXEL0_ALPHA", "TEXEL1_ALPHA", "PRIMITIVE_ALPHA", "SHADE_ALPHA", "ENV_ALPHA", "LOD_FRACTION", "PRIM_LOD_FRAC", "K5",
				"17", "18", "19", "20", "21", "22", "23", "24", 
				"25", "26", "27", "28", "29", "30", "31", "0" };

			string modes2[] = { "COMBINED", "TEXEL0", "TEXEL1", "PRIMITIVE", "SHADE", "ENVIRONMENT", "1", "0" };

			sprintf(line, "gsDPSetCombineLERP(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s),", 
				modes[a0].c_str(), modes[b0].c_str(), modes[c0].c_str(), modes[d0].c_str(), 
				modes2[aa0].c_str(),  modes2[ab0].c_str(), modes2[ac0].c_str(), modes2[ad0].c_str(), 
				modes[a1].c_str(), modes[b1].c_str(), modes[c1].c_str(), modes[d1].c_str(), 
				modes2[aa1].c_str(), modes2[ab1].c_str(), modes2[ac1].c_str(), modes2[ad1].c_str());
		}
		break;
		case F3DZEXOpcode::G_RDPLOADSYNC:
			sprintf(line, "gsDPLoadSync(),");
			break;
		case F3DZEXOpcode::G_RDPPIPESYNC:
			sprintf(line, "gsDPPipeSync(),");
			break;
		case F3DZEXOpcode::G_RDPTILESYNC:
			sprintf(line, "gsDPTileSync(),");
			break;
		case F3DZEXOpcode::G_RDPFULLSYNC:
			sprintf(line, "gsDPFullSync(),");
			break;
		case F3DZEXOpcode::G_ENDDL:
			sprintf(line, "gsSPEndDisplayList(),");

			TextureGenCheck(prefix);
			break;
		case F3DZEXOpcode::G_RDPHALF_1:
		{
			uint64_t data2 = instructions[i+1];
			uint32_t h = (data & 0xFFFFFFFF);
			F3DZEXOpcode opcode2 = (F3DZEXOpcode)rawData[((i + 1) * 8) + 0];

			if (opcode2 == F3DZEXOpcode::G_BRANCH_Z)
			{
				uint32_t a = (data2 & 0x00FFF00000000000) >> 44;
				uint32_t b = (data2 & 0x00000FFF00000000) >> 32;
				uint32_t z = (data2 & 0x00000000FFFFFFFF) >> 0;

				//sprintf(line, "gsDPWord(%i, 0),", h);
				sprintf(line, "gsSPBranchLessZraw(_%s_dlist_%08X, 0x%02X, 0x%02X),", prefix.c_str(), h & 0x00FFFFFF, (a / 5) | (b / 2), z);

				ZDisplayList* nList = new ZDisplayList(fileData, h & 0x00FFFFFF, GetDListLength(fileData, h & 0x00FFFFFF));
				nList->scene = scene;
				otherDLists.push_back(nList);

				i++;
			}
		}
			break;
		/*case F3DZEXOpcode::G_BRANCH_Z:
		{
			uint8_t h = (data & 0xFFFFFFFF);

			sprintf(line, "gsSPBranchLessZraw(%i, %i, %i),", h);
		}
			break;*/
		case F3DZEXOpcode::G_MTX:
		{
			// TODO: FINISH THIS
			uint32_t pp = (data & 0x000000FF00000000) >> 32;
			uint32_t mm = (data & 0x00000000FFFFFFFF);
			std::string matrixRef = "";

			if (Globals::Instance->symbolMap.find(mm) != Globals::Instance->symbolMap.end())
				matrixRef = StringHelper::Sprintf("&%s", Globals::Instance->symbolMap[mm].c_str());
			else
				matrixRef = StringHelper::Sprintf("0x%08X", mm);

			sprintf(line, "gsSPMatrix(%s, 0x%02X),", matrixRef.c_str(), pp ^ 0x01);
		}
			break;
		default:
			sprintf(line, "// Opcode 0x%02X unimplemented!", opcode);
			break;
		}

		auto end = chrono::steady_clock::now();
		auto diff = chrono::duration_cast<chrono::milliseconds>(end - start).count();

#if _MSC_VER
		//if (diff > 5)
			//printf("F3DOP: 0x%02X, TIME: %ims\n", opcode, diff);
#endif

		sourceOutput += line;

		sourceOutput += StringHelper::Sprintf(" // 0x%08X", rawDataIndex + (i * 8));
		sourceOutput += "\n";
	}

	sourceOutput += "};\n";

	// Iterate through our vertex lists, connect intersecting lists.
	if (vertices.size() > 0)
	{
		vector<pair<int32_t, vector<Vertex>>> verticesSorted(vertices.begin(), vertices.end());

		sort(verticesSorted.begin(), verticesSorted.end(), [](const auto& lhs, const auto& rhs)
		{
			return lhs.first < rhs.first;
		});

		for (int i = 0; i < verticesSorted.size() - 1; i++)
		{
			//int vtxSize = verticesSorted[i].second.size() * 16;
			int vtxSize = vertices[verticesSorted[i].first].size() * 16;

			if ((verticesSorted[i].first + vtxSize) > verticesSorted[i + 1].first)
			{
				int intersectAmt = (verticesSorted[i].first + vtxSize) - verticesSorted[i + 1].first;
				int intersectIndex = intersectAmt / 16;

				for (int j = intersectIndex; j < verticesSorted[i + 1].second.size(); j++)
				{
					vertices[verticesSorted[i].first].push_back(verticesSorted[i + 1].second[j]);
				}

				defines += StringHelper::Sprintf("#define _%s_vertices_%08X ((u32)_%s_vertices_%08X + 0x%08X)\n", prefix.c_str(), verticesSorted[i + 1].first, prefix.c_str(), verticesSorted[i].first, verticesSorted[i + 1].first - verticesSorted[i].first);
				
				int nSize = vertices[verticesSorted[i].first].size();

				vertices.erase(verticesSorted[i + 1].first);
				verticesSorted.erase(verticesSorted.begin() + i + 1);

				i--;
			}
		}

		// Generate Vertex Declarations
		for (pair<int32_t, vector<Vertex>> item : vertices)
		{
			string declaration = "";

			declaration += StringHelper::Sprintf("Vtx_t _%s_vertices_%08X[%i] = \n{\n", prefix.c_str(), item.first, item.second.size());

			int curAddr = item.first;

			for (Vertex vtx : item.second)
			{
				declaration += StringHelper::Sprintf("\t { %i, %i, %i, %i, %i, %i, %i, %i, %i, %i }, // 0x%08X\n",
					vtx.x, vtx.y, vtx.z, vtx.flag, vtx.s, vtx.t, vtx.r, vtx.g, vtx.b, vtx.a, curAddr);

				curAddr += 16;
			}

			declaration += "};\n";
			vtxDeclarations[item.first] = declaration;

			if (parent != nullptr)
			{
				Declaration* test = new Declaration(DeclarationAlignment::None, item.second.size() * 16, declaration);
				parent->declarations[item.first] = new Declaration(DeclarationAlignment::None, item.second.size() * 16, declaration);
				parent->externs[item.first] = StringHelper::Sprintf("extern Vtx_t _%s_vertices_%08X[%i];", prefix.c_str(), item.first, item.second.size());
			}
		}

		// Check for texture intersections
		{
			if (scene != nullptr && scene->textures.size() != 0)
			{
				vector<pair<uint32_t, ZTexture*>> texturesSorted(scene->textures.begin(), scene->textures.end());

				sort(texturesSorted.begin(), texturesSorted.end(), [](const auto& lhs, const auto& rhs)
				{
					return lhs.first < rhs.first;
				});

				for (int i = 0; i < texturesSorted.size() - 1; i++)
				{
					int texSize = scene->textures[texturesSorted[i].first]->GetRawDataSize();

					if ((texturesSorted[i].first + texSize) > texturesSorted[i + 1].first)
					{
						int intersectAmt = (texturesSorted[i].first + texSize) - texturesSorted[i + 1].first;

						defines += StringHelper::Sprintf("#define _%s_tex_%08X ((u32)_%s_tex_%08X + 0x%08X)\n", scene->GetName().c_str(), texturesSorted[i + 1].first, scene->GetName().c_str(),
							texturesSorted[i].first, texturesSorted[i + 1].first - texturesSorted[i].first);

						//int nSize = textures[texturesSorted[i].first]->GetRawDataSize();

						scene->parent->declarations.erase(texturesSorted[i + 1].first);
						scene->parent->externs.erase(texturesSorted[i + 1].first);
						scene->textures.erase(texturesSorted[i + 1].first);
						texturesSorted.erase(texturesSorted.begin() + i + 1);

						//textures.erase(texturesSorted[i + 1].first);

						i--;
					}
				}

				scene->extDefines += defines;
			}

			{
				vector<pair<uint32_t, ZTexture*>> texturesSorted(textures.begin(), textures.end());

				sort(texturesSorted.begin(), texturesSorted.end(), [](const auto& lhs, const auto& rhs)
				{
					return lhs.first < rhs.first;
				});

				for (int i = 0; i < texturesSorted.size() - 1; i++)
				{
					if (texturesSorted.size() == 0) // ?????
						break;

					int texSize = textures[texturesSorted[i].first]->GetRawDataSize();

					if ((texturesSorted[i].first + texSize) > texturesSorted[i + 1].first)
					{
						int intersectAmt = (texturesSorted[i].first + texSize) - texturesSorted[i + 1].first;

						defines += StringHelper::Sprintf("#define _%s_tex_%08X ((u32)_%s_tex_%08X + 0x%08X)\n", prefix.c_str(), texturesSorted[i + 1].first, prefix.c_str(),
							texturesSorted[i].first, texturesSorted[i + 1].first - texturesSorted[i].first);

						textures.erase(texturesSorted[i + 1].first);
						texturesSorted.erase(texturesSorted.begin() + i + 1);

						i--;
					}
				}
			}
		}


		// Generate Texture Declarations
		for (pair<int32_t, ZTexture*> item : textures)
		{
			string declaration = "";

			declaration += item.second->GetSourceOutputCode(prefix);
			texDeclarations[item.first] = declaration;

			if (parent != nullptr)
			{
				Declaration* test = new Declaration(DeclarationAlignment::None, item.second->GetRawDataSize(), declaration);
				parent->declarations[item.first] = new Declaration(DeclarationAlignment::None, item.second->GetRawDataSize(), declaration);
				parent->externs[item.first] = StringHelper::Sprintf("extern u64 _%s_tex_%08X[];", prefix.c_str(), item.first);
			}
		}

	}

	if (parent != nullptr)
	{
		parent->declarations[rawDataIndex] = new Declaration(DeclarationAlignment::None, GetRawDataSize(), sourceOutput);
		return "";
	}

	return sourceOutput;
}

// HOTSPOT
void ZDisplayList::TextureGenCheck(string prefix)
{
	if (TextureGenCheck(fileData, textures, scene, prefix, lastTexWidth, lastTexHeight, lastTexAddr, lastTexSeg, lastTexFmt, lastTexSiz, lastTexLoaded))
	{
		//lastTexWidth = 0;
		//lastTexHeight = 0;
		lastTexAddr = 0;
		lastTexLoaded = false;
	}
}

// HOTSPOT
bool ZDisplayList::TextureGenCheck(vector<uint8_t> fileData, map<uint32_t, ZTexture*>& textures, ZRoom* scene, string prefix, uint32_t texWidth, uint32_t texHeight, uint32_t texAddr, uint32_t texSeg, F3DZEXTexFormats texFmt, F3DZEXTexSizes texSiz, bool texLoaded)
{
	int segmentNumber = (texSeg & 0xFF000000) >> 24;

	//printf("TextureGenCheck seg=%i width=%i height=%i addr=0x%08X\n", segmentNumber, texWidth, texHeight, texAddr);

	if (texAddr != 0 && texWidth != 0 && texHeight != 0 && texLoaded)
	{
		if (segmentNumber != 2) // Not from a scene file
		{
			ZTexture* tex = ZTexture::FromBinary(TexFormatToTexType(texFmt, texSiz), fileData, texAddr, StringHelper::Sprintf("_%s_tex_%08X", prefix.c_str(), texAddr), texWidth, texHeight);

#ifdef _WIN32 // TEST
			//tex->Save("dump");
#endif	
			textures[texAddr] = tex;

			return true;
		}
		else
		{
			ZTexture* tex = ZTexture::FromBinary(TexFormatToTexType(texFmt, texSiz), scene->GetRawData(), texAddr,
				StringHelper::Sprintf("_%s_tex_%08X", Globals::Instance->lastScene->GetName().c_str(), texAddr), texWidth, texHeight);

#ifdef _WIN32 // TEST
			//tex->Save("dump"); // TEST
#endif

			if (scene != nullptr)
			{
				scene->textures[texAddr] = tex;
				scene->parent->declarations[texAddr] = new Declaration(DeclarationAlignment::None, tex->GetRawDataSize(), tex->GetSourceOutputCode(scene->GetName()));
				scene->parent->externs[texAddr] = tex->GetSourceOutputHeader(scene->GetName());
			}

			return true;
		}
	}

	return false;
}

TextureType ZDisplayList::TexFormatToTexType(F3DZEXTexFormats fmt, F3DZEXTexSizes siz)
{
	if (fmt == F3DZEXTexFormats::G_IM_FMT_RGBA)
	{
		if (siz == F3DZEXTexSizes::G_IM_SIZ_16b)
			return TextureType::RGBA16bpp;
		else if (siz == F3DZEXTexSizes::G_IM_SIZ_32b)
			return TextureType::RGBA32bpp;
	}
	else if (fmt == F3DZEXTexFormats::G_IM_FMT_CI)
	{
		//if (siz == F3DZEXTexSizes::G_IM_SIZ_8b)
			return TextureType::Palette8bpp;
	}
	else if (fmt == F3DZEXTexFormats::G_IM_FMT_IA)
	{
		if (siz == F3DZEXTexSizes::G_IM_SIZ_16b)
			return TextureType::GrayscaleAlpha16bpp;
		else if (siz == F3DZEXTexSizes::G_IM_SIZ_8b)
			return TextureType::GrayscaleAlpha8bpp;
	}
	else if (fmt == F3DZEXTexFormats::G_IM_FMT_I)
	{
		if (siz == F3DZEXTexSizes::G_IM_SIZ_8b)
			return TextureType::Grayscale8bpp;
		else if (siz == F3DZEXTexSizes::G_IM_SIZ_16b)
			return TextureType::Grayscale8bpp;
	}

	return TextureType::RGBA16bpp;
}

void ZDisplayList::AddInstruction_PipeSync()
{
	instructions.push_back(G_RDPPIPESYNC << 56);
}

void ZDisplayList::AddInstruction_GeometryMode(uint32_t param)
{
	instructions.push_back((G_GEOMETRYMODE << 56) | (param << 48));
}

void ZDisplayList::AddInstruction_ClearGeometryMode(uint32_t param)
{
	AddInstruction_GeometryMode(param);
}

void ZDisplayList::AddInstruction_Vertex(uint32_t vtxAddr, uint32_t indexStart, uint32_t indexEnd)
{
	instructions.push_back((G_VTX << 56) | (vtxAddr << 48) | (indexEnd << 16) | (indexStart << 8));

}

void ZDisplayList::AddInstruction_EndDisplayList()
{
	instructions.push_back(G_ENDDL << 56);
}


void ZDisplayList::Save(string outFolder)
{

}

vector<uint8_t> ZDisplayList::GetRawData()
{
	return rawData;
}

int ZDisplayList::GetRawDataSize()
{
	return instructions.size() * 8;
}

Vertex::Vertex()
{
	x = 0;
	y = 0;
	z = 0;
	flag = 0;
	s = 0;
	t = 0;
	r = 0;
	g = 0;
	b = 0;
	a = 0;
}

Vertex::Vertex(std::vector<uint8_t> rawData, int rawDataIndex)
{
	uint8_t* data = rawData.data();

	x = BitConverter::ToInt16BE(data, rawDataIndex + 0);
	y = BitConverter::ToInt16BE(data, rawDataIndex + 2);
	z = BitConverter::ToInt16BE(data, rawDataIndex + 4);
	flag = BitConverter::ToInt16BE(data, rawDataIndex + 6);
	s = BitConverter::ToInt16BE(data, rawDataIndex + 8);
	t = BitConverter::ToInt16BE(data, rawDataIndex + 10);
	r = data[rawDataIndex + 12];
	g = data[rawDataIndex + 13];
	b = data[rawDataIndex + 14];
	a = data[rawDataIndex + 15];
}