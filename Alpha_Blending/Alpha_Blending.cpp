#include <iostream>
#include <ctime>
#include <fstream>
#include <assert.h>


int GetFileSize(std::ifstream& file) {
	assert(file.is_open());

	std::streampos backupPos = file.tellg();

	file.seekg(0, std::ios_base::end);
	int res = file.tellg();

	file.seekg(backupPos);

	return res;
}


void MergeImgBufs(char* backBuf, char* frontBuf) {
	assert(backBuf != nullptr);
	assert(frontBuf != nullptr);

	const int startPosX = 220;
	const int startPosY = 260;
	const float scale = 0.6;

	int backImgStart = *(reinterpret_cast<int*>(backBuf + 10));
	int frontImgStart = *(reinterpret_cast<int*>(frontBuf + 10));

	int backWidth = *(reinterpret_cast<int*>(backBuf + 18));

	int frontWidth = *(reinterpret_cast<int*>(frontBuf + 18));
	int frontHeight = *(reinterpret_cast<int*>(frontBuf + 22));

	for (int frontX = 0; frontX < frontWidth; ++frontX) {
		for (int frontY = 0; frontY < frontHeight; ++frontY) {

			int backX = startPosX + static_cast<int>(frontX * scale);
			int backY = startPosY + static_cast<int>(frontY * scale);

			unsigned char* backPixel = reinterpret_cast<unsigned char*>(backBuf + backImgStart +
									                                    backY * backWidth * 4 + backX * 4);

			unsigned char* frontPixel = reinterpret_cast<unsigned char*>(frontBuf + frontImgStart +
				                                                         frontY * frontWidth * 4 + frontX * 4);

			unsigned char frontAlpha = *(frontPixel + 3);
			for (int i = 0; i < 3; ++i) {
				*(backPixel + i) = *(frontPixel + i) * frontAlpha / 256 + *(backPixel + i) * (256 - frontAlpha) / 256;
			}
			*(backPixel + 3) = 256;
		}
	}
}


void MergeImages(std::ifstream& backF, std::ifstream& frontF, std::ofstream& fout) {
	assert(backF.is_open());
	assert(frontF.is_open());
	assert(fout.is_open());

	int backSize = GetFileSize(backF);
	char* backBuf = new char[backSize + 1];
	backF.read(backBuf, backSize);

	int frontSize = GetFileSize(frontF);
	char* frontBuf = new char[frontSize + 1];
	frontF.read(frontBuf, frontSize);

	MergeImgBufs(backBuf, frontBuf);

	fout.write(backBuf, backSize);

	delete[] backBuf;
	delete[] frontBuf;
}

int main() {
	const char* backFName("Table.bmp");
	const char* frontFName("Cat.bmp");
	const char* outFName("CatOnTable.bmp");

	std::ifstream backF(backFName, std::ios::binary);
	std::ifstream frontF(frontFName, std::ios::binary);
	std::ofstream fout(outFName, std::ios::binary);
	if (!backF.is_open() || !frontF.is_open() || !fout.is_open()) {
		return -1;
	}

	MergeImages(backF, frontF, fout);

	return 0;
}
