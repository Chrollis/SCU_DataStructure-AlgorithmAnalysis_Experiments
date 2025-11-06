#ifndef COMPRESSOR_HPP
#define COMPRESSOR_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <string>
#include <vector>
#include <queue>

namespace chr {

	template <typename T>
	struct huffman_node {
		T data;
		int frequency;
		std::shared_ptr<huffman_node<T>> left;
		std::shared_ptr<huffman_node<T>> right;

		huffman_node(const T& d, int f) :
			data(d), frequency(f), left(nullptr), right(nullptr) {}
		huffman_node(int f, std::shared_ptr<huffman_node<T>> l, std::shared_ptr<huffman_node<T>>r)
			:data(T()), frequency(f), left(l), right(r) {}
		bool is_leaf() const { return left == nullptr && right == nullptr; }
	};
	template <typename T>
	struct node_comparator {
		bool operator()(const std::shared_ptr<huffman_node<T>>& a, const std::shared_ptr<huffman_node<T>>& b) {
			return a->frequency > b->frequency;
		}
	};
	using byte = unsigned char;
	class byte_array {
		std::vector<byte> m_data;
		size_t m_bit_count;
	public:
		byte_array() :m_bit_count(0) {}
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
	struct byte_array_hash {
		size_t operator()(const byte_array& ba) const;
	};
	template <typename T>
	class huffman_tree {
		std::shared_ptr<huffman_node<T>> m_root;
		std::unordered_map<T, byte_array> m_codes;
		std::unordered_map<byte_array, T, byte_array_hash> m_reverse_codes;
	private:
		template <typename InputIt>
		std::unordered_map<T, int> build_frequency_table(InputIt first, InputIt last) {
			std::unordered_map<T, int> frequency_table;
			for (auto it = first; it != last; ++it) {
				frequency_table[*it]++;
			}
			return frequency_table;
		}
		void build_tree(const std::unordered_map<T, int>& frequency_table) {
			std::priority_queue<
				std::shared_ptr<huffman_node<T>>,
				std::vector<std::shared_ptr<huffman_node<T>>>,
				node_comparator<T>
			> min_heap;
			for (const std::pair<T, int>& pair : frequency_table) {
				min_heap.push(std::make_shared<huffman_node<T>>(pair.first, pair.second));
			}
			if (min_heap.empty()) {
				m_root = nullptr;
				return;
			}
			if (min_heap.size() == 1) {
				auto left = min_heap.top();
				min_heap.pop();
				m_root = std::make_shared<huffman_node<T>>(left->frequency, left, nullptr);
				return;
			}
			while (min_heap.size() > 1) {
				auto left = min_heap.top();
				min_heap.pop();
				auto right = min_heap.top();
				min_heap.pop();
				int sum_freq = left->frequency + right->frequency;
				auto parent = std::make_shared<huffman_node<T>>(sum_freq, left, right);
				min_heap.push(parent);
			}
			m_root = min_heap.top();
		}
		void generate_codes(std::shared_ptr<huffman_node<T>> node, const byte_array& current_code) {
			if (node == nullptr) {
				return;
			}
			if (node->is_leaf()) {
				byte_array code = current_code;
				if (code.empty()) {
					code.push_back(0);
				}
				m_codes[node->data] = code;
				m_reverse_codes[code] = node->data;
			}
			else {
				byte_array left_code = current_code;
				left_code.push_back(0);
				generate_codes(node->left, left_code);
				byte_array right_code = current_code;
				right_code.push_back(1);
				generate_codes(node->right, right_code);
			}
		}
		T decode_single(std::shared_ptr<huffman_node<T>> node, const byte_array& encoded, size_t& bit_index) const {
			while (!node->is_leaf()) {
				if (bit_index >= encoded.size()) {
					throw std::invalid_argument("无效编码");
				}
				bool bit = encoded.bit(bit_index++);
				if (bit) {
					node = node->right;
				}
				else {
					node = node->left;
				}
			}
			return node->data;
		}
	public:
		huffman_tree() :m_root(nullptr) {}
		template <typename InputIt>
		huffman_tree(InputIt first, InputIt last) {
			build_from_data(first, last);
		}
		huffman_tree(const std::unordered_map<T, int>& frequency_table) {
			build_tree(frequency_table);
			generate_codes(m_root, byte_array());
		}
		template <typename InputIt>
		void build_from_data(InputIt first, InputIt last) {
			auto frequency_table = build_frequency_table(first, last);
			build_tree(frequency_table);
			generate_codes(m_root, byte_array());
		}
		void build_from_frequency_table(const std::unordered_map<T, int>& frequency_table) {
			build_tree(frequency_table);
			generate_codes(m_root, byte_array());
		}
		const byte_array& get_code(const T& data) const {
			auto it = m_codes.find(data);
			if (it != m_codes.end()) {
				return it->second;
			}
			throw std::invalid_argument("未找到相应编码");
		}
		byte_array encode(const T& data) const {
			return get_code(data);
		}
		template<typename InputIt>
		byte_array encode(InputIt first, InputIt last) const {
			byte_array result;
			for (auto it = first; it != last; ++it) {
				result += get_code(*it);
			}
			return result;
		}
		byte_array encode(const std::vector<T>& data) const {
			return encode(data.begin(), data.end());
		}
		template<typename OutputIt>
		void decode(const byte_array& encoded, OutputIt result) const {
			if (m_root == nullptr || encoded.empty()) return;
			if (m_root->is_leaf()) {
				for (std::size_t i = 0; i < encoded.size(); ++i) {
					*result++ = m_root->data;
				}
				return;
			}
			std::size_t bit_index = 0;
			while (bit_index < encoded.size()) {
				*result++ = decode_single(m_root, encoded, bit_index);
			}
		}
		std::vector<T> decode(const byte_array& encoded) const {
			std::vector<T> result;
			decode(encoded, std::back_inserter(result));
			return result;
		}
		template<typename OutputIt>
		void fast_decode(const byte_array& encoded, OutputIt result) const {
			byte_array current_code;
			for (std::size_t i = 0; i < encoded.size(); ++i) {
				current_code.push_back(encoded.bit(i));
				auto it = m_reverse_codes.find(current_code);
				if (it != m_reverse_codes.end()) {
					*result++ = it->second;
					current_code.clear();
				}
			}

			if (!current_code.empty()) {
				throw std::invalid_argument("不完整的编码");
			}
		}
		const std::unordered_map<T, byte_array>& codes() const { return m_codes; }
		bool is_built() const { return m_root != nullptr; }
		const std::shared_ptr<huffman_node<T>>& root() const { return m_root; }
		std::string to_string() const {
			std::ostringstream oss;
			for (const auto& pair : m_codes) {
				oss << "[" << pair.first << "]: ";
				oss << pair.second.to_string() << "\n";
			}
			return oss.str();
		}
		template<typename InputIt>
		void print_compression_info(InputIt first, InputIt last) const {
			byte_array encoded = encode(first, last);
			std::size_t data_count = std::distance(first, last);
			double original_size = data_count * sizeof(T) * 8.0;
			size_t encoded_size = encoded.size();
			double compression_ratio = (1 - encoded_size / original_size) * 100;
			std::cout << "\n压缩信息：\n";
			std::cout << "数据数量：" << data_count << "\n";
			std::cout << "原始大小：" << original_size << " 位\n";
			std::cout << "编码大小：" << encoded_size << " 位\n";
			std::cout << "压缩率：" << std::fixed << std::setprecision(2)
				<< compression_ratio << "%\n";
			std::cout << "节省空间：" << (original_size - encoded_size) / 8.0
				<< " 字节\n";
		}
	};
}

#endif // !COMPRESSOR_HPP
