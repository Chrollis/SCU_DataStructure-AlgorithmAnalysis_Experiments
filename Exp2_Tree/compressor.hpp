#ifndef COMPRESSOR_HPP
#define COMPRESSOR_HPP

#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <vector>

namespace chr {

	using byte = unsigned char;
	class huffman_node {
	public:
		byte data;
		size_t frequency;
		std::shared_ptr<huffman_node> left;
		std::shared_ptr<huffman_node> right;
		huffman_node(byte d, size_t f) :
			data(d), frequency(f), left(nullptr), right(nullptr) {}
		huffman_node(size_t f, std::shared_ptr<huffman_node> l, std::shared_ptr<huffman_node>r)
			:data(0), frequency(f), left(l), right(r) {}
		bool is_leaf() const { return left == nullptr && right == nullptr; }
		unsigned depth() const;
	};
	class huffman_node_comparator {
	public:
		bool operator()(const std::shared_ptr<huffman_node>& a, const std::shared_ptr<huffman_node>& b);
	};
	class byte_array {
		std::vector<byte> m_data;
		size_t m_bit_count;
	public:
		byte_array() :m_bit_count(0) {}
		byte_array(const std::vector<byte>& vec) :m_data(vec), m_bit_count(vec.size() * 8) {}
		byte_array(const std::vector<byte>& vec, size_t bit_count) :m_data(vec), m_bit_count(bit_count) {}
		void push_back(bool bit);
		void pop_back();
		byte_array& operator+=(const byte_array& other);
		bool bit(size_t pos) const;
		void set_bit(size_t pos, bool bit);
		size_t size() const { return m_bit_count; }
		size_t byte_size() const { return m_data.size(); }
		bool empty() const { return m_bit_count == 0; }
		void clear() { m_data.clear(); m_bit_count = 0; }
		const std::vector<byte>& data() const { return m_data; }
		std::vector<byte> to_bytes() const { return m_data; }
		std::string to_string(bool in_hexadecimal = 0) const;
		friend bool operator==(const byte_array& a, const byte_array& b);
		friend bool operator!=(const byte_array& a, const byte_array& b) { return !(a == b); }
	};
	std::ostream& operator<<(std::ostream& os, const byte_array& binary);
	struct byte_array_hash {
		size_t operator()(const byte_array& binary) const;
	};
	class huffman_tree {
		std::shared_ptr<huffman_node> m_root;
		std::unordered_map<byte, byte_array> m_codes;
		std::unordered_map<byte_array, byte, byte_array_hash> m_reverse_codes;
	private:
		void build_tree(const std::unordered_map<byte, unsigned>& frequency_table);
		std::unordered_map<byte, unsigned> build_frequency_table(const std::vector<byte>& vec_data);
		void from_frequency_table(const std::unordered_map<byte, unsigned>& frequency_table);
		void from_vector(const std::vector<byte>& vec_data);
		void from_binary_data(const byte_array& serialized_tree);
		void generate_codes(std::shared_ptr<huffman_node> node, const byte_array& current_code);
		byte decode_single(std::shared_ptr<huffman_node> node, const byte_array& encoded, size_t& bit_index) const;
		void serialize_tree(std::shared_ptr<huffman_node> node, byte_array& buffer) const;
		void serialize_data(byte data, byte_array& buffer) const;
		std::shared_ptr<huffman_node> deserialize_tree(byte_array& buffer, size_t& bit_index) const;
		byte deserialize_data(byte_array& buffer, size_t& bit_index) const;
	public:
		huffman_tree(const std::vector<byte>& vec_data) { from_vector(vec_data); }
		huffman_tree(const std::unordered_map<byte, unsigned>& frequency_table) { from_frequency_table(frequency_table); }
		huffman_tree(const byte_array& serialized_tree) { from_binary_data(serialized_tree); }
		const byte_array& encode(byte data) const;
		byte_array encode(const std::vector<byte>& vec_data) const;
		std::pair<byte_array, std::string> encode_with_info(const std::vector<byte>& vec_data) const;
		std::vector<byte> decode(const byte_array& encoded) const;
		std::vector<byte> fast_decode(const byte_array& encoded) const;
		byte_array to_byte_array() const;
		std::string to_string() const { return to_byte_array().to_string(); }
		const std::unordered_map<byte, byte_array>& codes() const { return m_codes; }
		std::string code_table() const;
		bool is_built() const { return m_root != nullptr; }
		const std::shared_ptr<huffman_node>& root() const { return m_root; }
	};
	std::filesystem::path compress(const std::filesystem::path& path);
	std::filesystem::path decompress(const std::filesystem::path& path);
}

#endif // !COMPRESSOR_HPP