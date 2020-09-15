#pragma once


#pragma pack(1)
struct BMP_Header {
	short bfType = 0;
	int bfSize = 0;
	int bfReserved = 0;
	int bfOffBits = 0;

	int biSize = 0;


	int biWidth = 0;
	int biHeight = 0;
	short biPlanes = 0;
	short biBitCount = 0;
	int biCompression = 0;

	int biSizeImage = 0;

	int biXPelsPerMeter = 0;
	int biYPelsPerMeter = 0;
	int biClrUsed = 0;

	int biClrImportant = 0;
};
#pragma pack()

class BMP_Img {
public:

	BMP_Img(std::ifstream& fin);

	BMP_Img(char* bmpBuf, int bmpSize);

	~BMP_Img();

	void OverlayImg(BMP_Img& frontBmp, int posX, int posY);

	void OverlayImg_fast(BMP_Img& frontBmp, int posX, int posY);

	void ToFile(std::ofstream& fout);

	void FromFile(std::ifstream& fin);

private:

	void Init();

	char* buf = nullptr;

	char* imgStart = nullptr;

	int bufSize = 0;

	BMP_Header* bmpHeader = {};
};