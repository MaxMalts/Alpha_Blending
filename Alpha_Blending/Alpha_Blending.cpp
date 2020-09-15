#include <iostream>
#include <ctime>
#include <fstream>
#include <immintrin.h>
#include <assert.h>
#include "BMP_Img.h"



void MergeImages(std::ifstream& backF, std::ifstream& frontF, std::ofstream& fout, std::ofstream& fout_opt) {
	assert(backF.is_open());
	assert(frontF.is_open());
	assert(fout.is_open());
	assert(fout_opt.is_open());

	const int posX = 220;
	const int posY = 260;

	BMP_Img backBmp(backF);
	BMP_Img frontBmp(frontF);

#ifdef COMPARE_TIMES
	const int timesLoop = 100000;

	clock_t tempClock = clock();
	for (int i = 0; i < timesLoop; ++i) {
		backBmp.OverlayImg(frontBmp, posX, posY);
	}
	float naiveTime = static_cast<float>(clock() - tempClock) / CLOCKS_PER_SEC;
	backBmp.ToFile(fout);

	backF.seekg(0, std::ios_base::beg);
	backBmp.FromFile(backF);
	tempClock = clock();
	for (int i = 0; i < timesLoop; ++i) {
		backBmp.OverlayImg_optimized(frontBmp, posX, posY);
	}
	float optTime = static_cast<float>(clock() - tempClock) / CLOCKS_PER_SEC;
	backBmp.ToFile(fout_opt);

	printf("Naive time: %f\nAVX time: %f\n", naiveTime, optTime);
#else
	backBmp.OverlayImg(frontBmp, posX, posY);
	backBmp.ToFile(fout);


	backF.seekg(0, std::ios_base::beg);
	backBmp.FromFile(backF);

	backBmp.OverlayImg_fast(frontBmp, posX, posY);
	backBmp.ToFile(fout_opt);
#endif
}

int main() {
	const char* backFName("Table.bmp");
	const char* frontFName("Cat.bmp");
	const char* outFName("CatOnTable.bmp");
	const char* outOptFName("CatOnTable_opt.bmp");

	std::ifstream backF(backFName, std::ios::binary);
	std::ifstream frontF(frontFName, std::ios::binary);
	std::ofstream fout(outFName, std::ios::binary);
	std::ofstream fout_opt(outOptFName, std::ios::binary);
	if (!backF.is_open() || !frontF.is_open() || !fout.is_open() || !fout_opt.is_open()) {
		return -1;
	}

	MergeImages(backF, frontF, fout, fout_opt);

	backF.close();
	frontF.close();
	fout.close();
	fout_opt.close();
	return 0;
}
