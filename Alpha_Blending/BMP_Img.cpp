#include <iostream>
#include <fstream>
#include <immintrin.h>
#include <assert.h>
#include "BMP_Img.h"


int GetFileSize(std::ifstream& file) {
	assert(file.is_open());

	std::streampos backupPos = file.tellg();

	file.seekg(0, std::ios_base::end);
	int res = file.tellg();

	file.seekg(backupPos);

	return res;
}



BMP_Img::BMP_Img(std::ifstream& fin) {
	assert(fin.is_open());

	FromFile(fin);
}


BMP_Img::BMP_Img(char* bmpBuf, int bmpSize) {
	assert(bmpBuf > 0);
	assert(bmpSize > 0);

	buf = bmpBuf;
	bufSize = bmpSize;

	Init();
}


BMP_Img::~BMP_Img() {
	delete[] buf;
}


void BMP_Img::OverlayImg(BMP_Img& frontBmp, int posX, int posY) {
	assert(buf != nullptr);
	assert(frontBmp.buf != nullptr);


	for (int frontX = 0; frontX < frontBmp.bmpHeader->biWidth; ++frontX) {
		for (int frontY = 0; frontY < frontBmp.bmpHeader->biHeight; ++frontY) {

			int backX = posX + frontX;
			int backY = posY + frontY;

			unsigned char* backPixel = reinterpret_cast<unsigned char*>(imgStart +
				backY * bmpHeader->biWidth * 4 + backX * 4);

			unsigned char* frontPixel = reinterpret_cast<unsigned char*>(frontBmp.imgStart +
				frontY * frontBmp.bmpHeader->biWidth * 4 + frontX * 4);

			unsigned char frontAlpha = *(frontPixel + 3);
			for (int i = 0; i < 3; ++i) {
				*(backPixel + i) = *(frontPixel + i) * frontAlpha / 255 + *(backPixel + i) * (255 - frontAlpha) / 255;
			}
			*(backPixel + 3) = 255;
		}
	}
}


void BMP_Img::OverlayImg_fast(BMP_Img& frontBmp, int posX, int posY) {
	assert(buf != nullptr);
	assert(frontBmp.buf != nullptr);

	const unsigned char mulAlpha_mask_mem[] = { 6, 255, 6, 255, 6, 255, 255, 255, 14, 255, 14, 255, 14, 255, 255, 255,
											   6, 255, 6, 255, 6, 255, 255, 255, 14, 255, 14, 255, 14, 255, 255, 255 };

	const unsigned char addAlpha_mask_mem[] = { 0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 255, 0,
											   0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 255, 0 };

	const unsigned char pack1_mask_mem[] = { 0, 2, 4, 6, 8, 10, 12, 14, 255, 255, 255, 255, 255, 255, 255, 255,
											0, 2, 4, 6, 8, 10, 12, 14, 255, 255, 255, 255, 255, 255, 255, 255 };

	const unsigned char pack2_mask_mem[] = { 0, 0, 0, 0, 1, 0, 0, 0, 4, 0, 0, 0, 5, 0, 0, 0,
											0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	__m256i mulAlpha_mask = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(mulAlpha_mask_mem));
	__m256i addAlpha_mask = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(addAlpha_mask_mem));
	__m256i pack1_mask = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(pack1_mask_mem));
	__m256i pack2_mask = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(pack2_mask_mem));

	for (int frontY = 0; frontY < frontBmp.bmpHeader->biHeight; ++frontY) {
		int backY = posY + frontY;

		int frontX = 0;
		while (frontX < frontBmp.bmpHeader->biWidth - 3) {

			int backX = posX + frontX;

			__m128i* backPixel = reinterpret_cast<__m128i*>(imgStart +
				backY * bmpHeader->biWidth * 4 + backX * 4);

			// Loading original pixels {
			__m128i back4Px = _mm_loadu_si128(backPixel);
			__m128i front4Px = _mm_loadu_si128(reinterpret_cast<__m128i const*>(frontBmp.imgStart +
				frontY * frontBmp.bmpHeader->biWidth * 4 + frontX * 4));
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
		while (frontX < frontBmp.bmpHeader->biWidth) {

			int backX = posX + frontX;

			unsigned char* backPixel = reinterpret_cast<unsigned char*>(imgStart +
				backY * bmpHeader->biWidth * 4 + backX * 4);

			unsigned char* frontPixel = reinterpret_cast<unsigned char*>(frontBmp.imgStart +
				frontY * frontBmp.bmpHeader->biWidth * 4 + frontX * 4);
			unsigned char frontAlpha = *(frontPixel + 3);
			for (int i = 0; i < 3; ++i) {
				*(backPixel + i) = *(frontPixel + i) * frontAlpha / 256 + *(backPixel + i) * (256 - frontAlpha) / 256;
			}
			*(backPixel + 3) = 256;

			++frontX;
		}
	}
}


void BMP_Img::ToFile(std::ofstream& fout) {
	assert(fout.is_open());
	assert(buf != nullptr);

	fout.write(buf, bufSize);
}


void BMP_Img::FromFile(std::ifstream& fin) {
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


void BMP_Img::Init() {
	bmpHeader = reinterpret_cast<BMP_Header*>(buf);
	imgStart = buf + sizeof(BMP_Header);
}