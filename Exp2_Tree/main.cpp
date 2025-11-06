#include "compressor.hpp"

int main() {
    std::string str;
    bool is_compress = 0;
    while (1) {
        try {
            std::cout << "压缩【C】或解压【D】：";
            std::getline(std::cin, str);
            if (str == "C" || str == "c") {
                is_compress = 1;
            }
            else if (str == "D" || str == "d") {
                is_compress = 0;
            }
            else    if (str == "exit") {
                return 0;
            }
            else if (str == "clear") {
                system("cls");
            }
            else {
                throw std::runtime_error("错误代码，请重试");
            }
            std::cout << "输入地址：";
            std::getline(std::cin, str);
            if (str.starts_with("\"") && str.ends_with("\"")) {
                str = str.substr(1, str.length() - 2);
            }
            if (is_compress) {
                str = chr::compress(str).string();
                std::cout << "压缩文件创建于：" << str << "\n";
            }
            else {
                str = chr::decompress(str).string();
                std::cout << "解压缩文件创建于：" << str << "\n";
            }
        }
        catch (std::runtime_error& e) {
            std::cout << e.what() << std::endl;
        }
    }
    return 0;
}