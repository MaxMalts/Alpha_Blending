#include <iostream>
#include <ctime>
#include <fstream>
#include <immintrin.h>
#include <assert.h>




int GetFileSize(std::ifstream& file) {
	assert(file.is_open());

	std::streampos backupPos = file.tellg();

	file.seekg(0, std::ios_base::end);
	int res = file.tellg();

	file.seekg(backupPos);

	return res;
}



class BMP_Structure {
public:

	BMP_Structure(std::ifstream& fin) {
		assert(fin.is_open());

		FromFile(fin);
	}

	BMP_Structure(char* bmpBuf, int bmpSize) {
		assert(bmpBuf > 0);
		assert(bmpSize > 0);

		buf = bmpBuf;
		bufSize = bmpSize;

		Init();
	}

	~BMP_Structure() {
		delete[] buf;
	}

	void OverlayImg(BMP_Structure& frontBmp, int posX, int posY);

	void OverlayImg_optimized(BMP_Structure& frontBmp, int posX, int posY);

	void ToFile(std::ofstream& fout);

	void FromFile(std::ifstream& fin) {
		assert(fin.is_open());

		int fSize = GetFileSize(fin);

		if (buf != nullptr) {
			delete[] buf;
		}
		buf = new char[fSize + 1];

		fin.read(buf, fSize);

		bufSize = fSize;

		Init();
	}

private:

	void Init() {
		const int startOffsetPos = 10;
		const int widthOffsetPos = 18;
		const int heightOffsetPos = 22;

		int startOffset = *reinterpret_cast<int*>(buf + startOffsetPos);
		assert(bufSize > startOffset);
		imgStart = buf + startOffset;

		width = *reinterpret_cast<int*>(buf + widthOffsetPos);
		height = *reinterpret_cast<int*>(buf + heightOffsetPos);
	}


public:
	char* buf = nullptr;

	char* imgStart = nullptr;

	int width = 0;
	int height = 0;

	int bufSize = 0;
};

void BMP_Structure::OverlayImg(BMP_Structure& frontBmp, int posX, int posY) {
	assert(buf != nullptr);
	assert(frontBmp.buf != nullptr);


	for (int frontX = 0; frontX < frontBmp.width; ++frontX) {
		for (int frontY = 0; frontY < frontBmp.height; ++frontY) {

			int backX = posX + frontX;
			int backY = posY + frontY;

			unsigned char* backPixel = reinterpret_cast<unsigned char*>(imgStart +
									                                    backY * width * 4 + backX * 4);

			unsigned char* frontPixel = reinterpret_cast<unsigned char*>(frontBmp.imgStart +
				                                                         frontY * frontBmp.width * 4 + frontX * 4);

			unsigned char frontAlpha = *(frontPixel + 3);
			for (int i = 0; i < 3; ++i) {
				*(backPixel + i) = *(frontPixel + i) * frontAlpha / 255 + *(backPixel + i) * (255 - frontAlpha) / 255;
			}
			*(backPixel + 3) = 255;
		}
	}
}


void BMP_Structure::OverlayImg_optimized(BMP_Structure& frontBmp, int posX, int posY) {
	assert(buf != nullptr);
	assert(frontBmp.buf != nullptr);

	const unsigned char mulAlpha_mask_mem[] = {6, 255, 6, 255, 6, 255, 255, 255, 14, 255, 14, 255, 14, 255, 255, 255,
										       6, 255, 6, 255, 6, 255, 255, 255, 14, 255, 14, 255, 14, 255, 255, 255};

	const unsigned char addAlpha_mask_mem[] = {0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 255, 0,
								               0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 255, 0};

	const unsigned char pack1_mask_mem[] = {0, 2, 4, 6, 8, 10, 12, 14, 255, 255, 255, 255, 255, 255, 255, 255,
							                0, 2, 4, 6, 8, 10, 12, 14, 255, 255, 255, 255, 255, 255, 255, 255};

	const unsigned char pack2_mask_mem[] = {0, 0, 0, 0, 1, 0, 0, 0, 4, 0, 0, 0, 5, 0, 0, 0,
									        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	__m256i mulAlpha_mask = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(mulAlpha_mask_mem));
	__m256i addAlpha_mask = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(addAlpha_mask_mem));
	__m256i pack1_mask = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(pack1_mask_mem));
	__m256i pack2_mask = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(pack2_mask_mem));

	for (int frontY = 0; frontY < frontBmp.height; ++frontY) {
		int backY = posY + frontY;

		int frontX = 0;
		while (frontX < frontBmp.width - 3) {

			int backX = posX + frontX;

			__m128i* backPixel = reinterpret_cast<__m128i*>(imgStart +
															backY * width * 4 + backX * 4);

			// Loading original pixels {
			__m128i back4Px = _mm_loadu_si128(backPixel);
			__m128i front4Px = _mm_loadu_si128(reinterpret_cast<__m128i const*>(frontBmp.imgStart +
																				frontY * frontBmp.width * 4 + frontX * 4));
			// }

			// Zero-extending them {
			__m256i back4Px_ext = _mm256_cvtepu8_epi16(back4Px);
			//__m256i test = _mm256_set_epi32(255, 255, 255, 255, 255, 255, 0x00010000, 255);
			//__m256i test1 = _mm256_add_epi16(back4Px_ext, test);
			__m256i front4Px_ext = _mm256_cvtepu8_epi16(front4Px);
			// }

			__m256i frontAlpha = _mm256_shuffle_epi8(front4Px_ext, mulAlpha_mask);

			// Calculating new colors: newColor = front * frontAlpha / 255 + back * (255 - frontAlpha) / 255 <~>
			// <~> newColor = (back * (255 - frontAlpha) + front * frontAlpha) >> 8 {
			__m256i val255 = _mm256_set_epi64x(0x00ff00ff00ff00ff, 0x00ff00ff00ff00ff, 0x00ff00ff00ff00ff, 0x00ff00ff00ff00ff);

			__m256i test = _mm256_sub_epi16(val255, frontAlpha);
			back4Px_ext = _mm256_mullo_epi16(back4Px_ext, test);
			back4Px_ext = _mm256_add_epi16(back4Px_ext, _mm256_mullo_epi16(front4Px_ext, frontAlpha));
			back4Px_ext = _mm256_srli_epi16(back4Px_ext, 8);
			// }

			// backAlpha = 255 {
			__m256i alpha255 = _mm256_set_epi64x(0x00ff000000000000, 0x00ff000000000000, 0x00ff000000000000, 0x00ff000000000000);
			back4Px_ext = _mm256_or_si256(back4Px_ext, alpha255);
			// }

			// Undoing zero extending {
			back4Px_ext = _mm256_shuffle_epi8(back4Px_ext, pack1_mask);
			back4Px_ext = _mm256_permutevar8x32_epi32(back4Px_ext, pack2_mask);
			// }

			// Storing results {
			_mm_storeu_si128(backPixel, _mm256_castsi256_si128(back4Px_ext));
			// }

			frontX += 4;
		}


		// Rest (less than 4px) with naive algorithm
		while (frontX < frontBmp.width) {

			int backX = posX + frontX;

			unsigned char* backPixel = reinterpret_cast<unsigned char*>(imgStart +
																		backY * width * 4 + backX * 4);

			unsigned char* frontPixel = reinterpret_cast<unsigned char*>(frontBmp.imgStart +
																		 frontY * frontBmp.width * 4 + frontX * 4);
			unsigned char frontAlpha = *(frontPixel + 3);
			for (int i = 0; i < 3; ++i) {
				*(backPixel + i) = *(frontPixel + i) * frontAlpha / 256 + *(backPixel + i) * (256 - frontAlpha) / 256;
			}
			*(backPixel + 3) = 256;

			++frontX;
		}
	}
}


void BMP_Structure::ToFile(std::ofstream& fout) {
	assert(fout.is_open());
	assert(buf != nullptr);

	fout.write(buf, bufSize);
}


void MergeImages(std::ifstream& backF, std::ifstream& frontF, std::ofstream& fout, std::ofstream& fout_opt) {
	assert(backF.is_open());
	assert(frontF.is_open());
	assert(fout.is_open());
	assert(fout_opt.is_open());

	const int posX = 220;
	const int posY = 260;

	BMP_Structure backBmp(backF);
	BMP_Structure frontBmp(frontF);

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

	backBmp.OverlayImg_optimized(frontBmp, posX, posY);
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
