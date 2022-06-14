#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#include <stdint.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <set>

struct COLOR4{
    uint8_t r; uint8_t g; uint8_t b; uint8_t a;
};
struct ImageInfo
{
    COLOR4* img;
    int x,y,comp;
};


void CopyRegion(int srcX, int srcY, int srcW, int dstX, int dstY, int dstW, int cpyW, int cpyH, COLOR4* dst, const COLOR4* src)
{
    for(int y = 0; y < cpyH; y++)
    {
        for(int x = 0; x < cpyW; x++)
        {
            int srcIdx = x + srcX + srcW * (srcY + y);
            int dstIdx = x + dstX + dstW * (dstY + y);
            dst[dstIdx] = src[srcIdx];
        }
    }
}

void AttachAllFiles(std::vector<std::string>& result, std::filesystem::path path, const char* toFind)
{
    for(const auto& iter : std::filesystem::directory_iterator(path))
    {
        if(iter.is_regular_file())
        {
            auto& rPath = iter.path();
            if(rPath.extension().compare(".png") == 0 && rPath.string().find(toFind) != (size_t)-1)
            {
                result.push_back(rPath.string());
            }
        }
        else if(iter.is_directory())
        {
            AttachAllFiles(result, iter.path(), toFind);
        }
    }
}
bool SortString(std::string& v1, std::string& v2)
{
    try
    {
        size_t extension1 = v1.find(".png");
        size_t extension2 = v2.find(".png");
        size_t last1 = v1.find_last_of('_');
        size_t last2 = v2.find_last_of('_');


        int count1 = std::stoi(v1.substr(last1+1, extension1-last1-1));
        int count2 = std::stoi(v2.substr(last2+1, extension2-last2-1));
        return count2 > count1;
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    return false;
}

int main(int argc, char** argv)
{
    if(argc < 3) {
        std::cout << "need 2 targs: START_PATH, MATCH_STRING" << std::endl;
        return 1;
    }
    std::cout << "path: " << argv[1] << ", matching: " << argv[2] << std::endl;

    std::filesystem::path dirPath(argv[1]);

    std::vector<std::string> strVec;
    AttachAllFiles(strVec, dirPath, argv[2]);
    std::sort(strVec.begin(), strVec.end(), SortString);
    if(strVec.empty())
    {
        std::cout << "FOUND NON MATCHING: " << argv[2] << std::endl;
        return 1;
    }
    int x,y,comp;
    std::cout << "0: " << strVec.begin()->c_str() << std::endl;
    COLOR4* tex = (COLOR4*)stbi_load(strVec.begin()->c_str(), &x, &y, &comp, 4);

    int singleSizeX = x;
    int width = x * strVec.size();
    int height = y;

    COLOR4* outputColor = new COLOR4[width * height];
    memset(outputColor, 0, sizeof(COLOR4) * width * height);
    CopyRegion(0, 0, singleSizeX, 0, 0, width, singleSizeX, height, outputColor, tex);
    int counter = 1;
    for(auto i = ++strVec.begin(); i != strVec.end(); i++)
    {
        std::cout << counter << ": " << i->c_str() << std::endl;
        tex = (COLOR4*)stbi_load(i->c_str(), &x, &y, &comp, 4);
        CopyRegion(0, 0, singleSizeX, singleSizeX * counter, 0, width, singleSizeX, height, outputColor, tex);
        counter++;
    }

    stbi_write_png((std::string(argv[2]) + ".png").c_str(), width, height, 4, outputColor, width * 4);
   
    return true;
}