#include "compressor.hpp"
using namespace chr;
int main() {
    // 示例1：字符类型
    std::cout << "=== Character Huffman Tree ===\n";
    std::string text = "this is an example for huffman encoding";
    std::cout << "Input: " << text << "\n";
    huffman_tree<char> char_tree(text.begin(), text.end());

    std::cout<<char_tree.to_string();

    // 编码
    auto encoded = char_tree.encode(text.begin(), text.end());
    std::cout << encoded.to_string(1) << "\n";

    // 解码
    std::string decoded;
    char_tree.decode(encoded, std::back_inserter(decoded));
    std::cout << "Decoded: " << decoded << "\n";

    char_tree.print_compression_info(text.begin(), text.end());

    // 示例2：整数类型
    std::cout << "\n=== Integer Huffman Tree ===\n";
    std::vector<int> numbers = { 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5 };
    std::cout << "Input numbers: ";
    for (int num : numbers) {
        std::cout << num << " ";
    }
    std::cout << "\n";
    huffman_tree<int> int_tree(numbers.begin(), numbers.end());

    std::cout << int_tree.to_string();

    auto encoded_nums = int_tree.encode(numbers);

    std::cout << encoded_nums.to_string(1) << "\n";

    auto decoded_nums = int_tree.decode(encoded_nums);

    std::cout << "Decoded numbers: ";
    for (int num : decoded_nums) {
        std::cout << num << " ";
    }
    std::cout << "\n";

    int_tree.print_compression_info(numbers.begin(), numbers.end());

    return 0;
}