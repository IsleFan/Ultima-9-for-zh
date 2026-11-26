#include "file_redirect.h"
#include <iostream>

using namespace u9cht::file_redirect;
namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { std::cerr << "FAIL: " << what << "\n"; ++failures; } }
}

int main() {
    check(is_bitmap16("static\\bitmap16.flx"), "relative path");
    check(is_bitmap16("C:\\GOG Games\\Ultima 9\\static\\BITMAP16.FLX"), "absolute path, upper case");
    check(is_bitmap16(".\\Static/Bitmap16.flx"), "mixed separators and case");
    check(!is_bitmap16("bitmap16.flx"), "no folder");
    check(!is_bitmap16("static\\bitmapC.flx"), "another texture file");
    check(!is_bitmap16("static\\bitmap16.flx.bak"), "another name");
    check(!is_bitmap16("data\\bitmap16.flx"), "another folder");
    check(!is_bitmap16("mystatic\\bitmap16.flx"), "folder name must match whole");
    check(!is_bitmap16(""), "empty");
    check(read_only(0x80000000, OpenExisting), "generic read");
    check(read_only(0x80000000 | 0x1, OpenExisting), "read data");
    check(!read_only(0x80000000 | GenericWrite, OpenExisting), "write access");
    check(!read_only(FileWriteData, OpenExisting), "write data");
    check(read_only(0x80000000, OpenAlways), "open always, as u9.exe opens its resources");
    check(!read_only(0x80000000, 2), "create always");
    check(!read_only(0x80000000, 1), "create new");
    check(!read_only(0x80000000, 5), "truncate existing");
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "file redirect tests passed\n";
    return 0;
}
