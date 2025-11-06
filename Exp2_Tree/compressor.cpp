#include "compressor.hpp"

namespace chr {
	unsigned huffman_node::depth() const {
		if (this->is_leaf()) {
			return 0;
		}
		return std::max(left->depth(), right->depth()) + 1;
	}
	bool huffman_node_comparator::operator()(const std::shared_ptr<huffman_node>& a, const std::shared_ptr<huffman_node>& b) {
		if (a->frequency != b->frequency) {
			return a->frequency > b->frequency;
		}
		if (a->depth() != b->depth()) {
			return a->depth() > b->depth();
		}
		return a->data > b->data;
	}
	void byte_array::push_back(bool bit) {
		size_t byte_index = m_bit_count / 8;
		size_t bit_offset = m_bit_count % 8;
		while (byte_index >= m_data.size()) {
			m_data.push_back(0);
		}
		if (bit) {
			m_data[byte_index] |= (1 << (7 - bit_offset));
		}
		++m_bit_count;
	}
	void byte_array::pop_back() {
		if (m_bit_count == 0) {
			throw std::out_of_range("弹出元素时数组为空");
		}
		--m_bit_count;
		size_t byte_index = m_bit_count / 8;
		size_t bit_offset = m_bit_count % 8;
		m_data[byte_index] &= ~(1 << (7 - bit_offset));
		if (bit_offset == 0 && !m_data.empty()) {
			m_data.pop_back();
		}
	}
	byte_array& byte_array::operator+=(const byte_array& other) {
		for (size_t i = 0; i < other.m_bit_count; ++i) {
			this->push_back(other.bit(i));
		}
		return *this;
	}
	bool byte_array::bit(size_t pos) const {
		if (pos >= m_bit_count) {
			throw std::out_of_range("下标出界");
		}
		size_t byte_index = pos / 8;
		size_t bit_offset = pos % 8;
		return (m_data[byte_index] >> (7 - bit_offset)) & 1;
	}
	void byte_array::set_bit(size_t pos, bool bit) {
		if (pos >= m_bit_count) {
			throw std::out_of_range("下标出界");
		}
		size_t byte_index = pos / 8;
		size_t bit_offset = pos % 8;
		if (bit) {
			m_data[byte_index] |= (1 << (7 - bit_offset));
		}
		else {
			m_data[byte_index] &= ~(1 << (7 - bit_offset));
		}
	}
	std::string byte_array::to_string(bool in_hexadecimal) const {
		std::ostringstream oss;
		if (in_hexadecimal) {
			for (byte b : m_data) {
				oss << std::hex << std::setw(2) << std::setfill('0')
					<< static_cast<int>(b) << " ";
			}
		}
		else {
			for (size_t i = 0; i < m_bit_count; ++i) {
				oss << this->bit(i) ? '1' : '0';
				if ((i + 1) % 8 == 0 && i + 1 < m_bit_count) {
					oss << " ";
				}
			}
		}
		return oss.str();
	}
	bool operator==(const byte_array& a, const byte_array& b) {
		if (a.size() != b.size()) {
			return 0;
		}
		size_t full_bytes = a.size() / 8;
		for (size_t i = 0; i < full_bytes; ++i) {
			if (a.data()[i] != b.data()[i]) {
				return 0;
			}
		}
		size_t remaining_bits = a.size() / 8;
		if (remaining_bits > 0) {
			byte mask = (0xFF << (8 - remaining_bits));
			if ((a.data()[full_bytes] & mask) != (b.data()[full_bytes] & mask)) {
				return 0;
			}
		}
		return 1;
	}
	std::ostream& operator<<(std::ostream& os, const byte_array& binary) {
		os << binary.to_string(1);
		return os;
	}
	std::filesystem::path compress(const std::filesystem::path& path) {
		static std::vector<std::string> postfixs = {
			".zip",".rar",".7z",".gz",".tar",
			".jpg",".jpeg",".png",".gif",".bmp",
			".mp3",".mp4",".avi","mkv",
			".pdf",".docx",".xlsx",".pptx"
		};
		for (const auto& str : postfixs) {
			if (path.string().ends_with(str)) {
				throw std::runtime_error("文件类型已经是压缩格式，不建议再次压缩：" + path.string());
			}
		}
		std::ifstream ifs(path.c_str(), std::ios::binary);
		if (!ifs.is_open()) {
			throw std::runtime_error("文件打开失败：" + path.string());
		}
		std::vector<byte> file_data;
		file_data.assign(
			std::istreambuf_iterator<char>(ifs),
			std::istreambuf_iterator<char>()
		);
		ifs.close();
		huffman_tree tree(file_data);
		byte_array tree_structure = tree.to_byte_array();
		std::filesystem::path output_path = path.string() + ".huff";
		std::ofstream ofs(output_path.c_str(), std::ios::binary);
		if (!ofs.is_open()) {
			throw std::runtime_error("无法创建压缩文件：" + output_path.string());
		}
		std::size_t tree_bit_count = tree_structure.size();
		std::size_t tree_byte_count = tree_structure.byte_size();
		ofs.write(reinterpret_cast<const char*>(&tree_bit_count), sizeof(tree_bit_count));
		ofs.write(reinterpret_cast<const char*>(&tree_byte_count), sizeof(tree_byte_count));
		ofs.write(reinterpret_cast<const char*>(tree_structure.data().data()), tree_structure.data().size());
		auto compressed_pair = tree.encode_with_info(file_data);
		auto& compressed = compressed_pair.first;
		std::cout << compressed_pair.second;
		const auto& compressed_data = compressed.data();
		size_t bit_count = compressed.size();
		size_t byte_count = compressed_data.size();
		ofs.write(reinterpret_cast<const char*>(&bit_count), sizeof(bit_count));
		ofs.write(reinterpret_cast<const char*>(&byte_count), sizeof(byte_count));
		ofs.write(reinterpret_cast<const char*>(compressed_data.data()), byte_count);
		ofs.close();
		auto src_size = std::filesystem::file_size(path);
		auto dst_size = std::filesystem::file_size(output_path);
		double compression_ratio = (1 - (double)dst_size / src_size) * 100;
		std::ostringstream oss;
		oss << "原始文件大小：" << src_size / 1024.0 << " KB\n";
		oss << "压缩文件大小：" << dst_size / 1024.0 << " KB\n";
		oss << "实际压缩率：" << std::fixed << std::setprecision(2) << compression_ratio << "%\n";
		std::cout << oss.str();
		return output_path;
	}
	std::filesystem::path decompress(const std::filesystem::path& path) {
		if (!path.string().ends_with(".huff")) {
			throw std::runtime_error("请选择.huff文件：" + path.string());
		}
		std::ifstream ifs(path.c_str(), std::ios::binary);
		if (!ifs.is_open()) {
			throw std::runtime_error("文件打开失败：" + path.string());
		}
		std::size_t tree_bit_count = 0, tree_byte_count = 0;
		ifs.read(reinterpret_cast<char*>(&tree_bit_count), sizeof(tree_bit_count));
		ifs.read(reinterpret_cast<char*>(&tree_byte_count), sizeof(tree_byte_count));
		std::vector<byte> tree_data(tree_byte_count);
		ifs.read(reinterpret_cast<char*>(tree_data.data()), tree_byte_count);
		byte_array tree_structure(tree_data, tree_bit_count);
		huffman_tree tree(tree_structure);
		std::size_t bit_count = 0, byte_count = 0;
		ifs.read(reinterpret_cast<char*>(&bit_count), sizeof(bit_count));
		ifs.read(reinterpret_cast<char*>(&byte_count), sizeof(byte_count));
		std::vector<byte> compressed_data(byte_count);
		ifs.read(reinterpret_cast<char*>(compressed_data.data()), byte_count);
		byte_array compressed(compressed_data, bit_count);
		auto decompressed = tree.decode(compressed);
		std::string output_path = path.string().substr(0, path.string().find_last_of('\\')) +
			"\\decompressed_" + path.string().substr(path.string().find_last_of('\\') + 1,
				path.string().find_last_of('.') - path.string().find_last_of('\\') - 1);
		std::ofstream ofs(output_path, std::ios::binary);
		if (!ofs.is_open()) {
			throw std::runtime_error("无法创建解压文件: " + output_path);
		}
		ofs.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
		ofs.close();
		return output_path;
	}
	size_t byte_array_hash::operator()(const byte_array& binary) const {
		size_t seed = binary.size();
		for (byte b : binary.data()) {
			seed ^= std::hash<byte>{}(b)+0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
	void huffman_tree::build_tree(const std::unordered_map<byte, unsigned>& frequency_table) {
		std::priority_queue<
			std::shared_ptr<huffman_node>,
			std::vector<std::shared_ptr<huffman_node>>,
			huffman_node_comparator
		> min_heap;
		for (const auto& pair : frequency_table) {
			min_heap.push(std::make_shared<huffman_node>(pair.first, pair.second));
		}
		if (min_heap.empty()) {
			m_root = nullptr;
			return;
		}
		if (min_heap.size() == 1) {
			std::shared_ptr<huffman_node> left = min_heap.top();
			min_heap.pop();
			m_root = std::make_shared<huffman_node>(left->frequency, left, nullptr);
			return;
		}
		while (min_heap.size() > 1) {
			std::shared_ptr<huffman_node> left = min_heap.top();
			min_heap.pop();
			std::shared_ptr<huffman_node> right = min_heap.top();
			min_heap.pop();
			size_t sum_freq = left->frequency + right->frequency;
			auto parent = std::make_shared<huffman_node>(sum_freq, left, right);
			min_heap.push(parent);
		}
		m_root = min_heap.top();
	}
	std::unordered_map<byte, unsigned> huffman_tree::build_frequency_table(const std::vector<byte>& vec_data) {
		std::unordered_map<byte, unsigned> frequency_table;
		for (byte data : vec_data) {
			frequency_table[data]++;
		}
		return frequency_table;
	}
	void huffman_tree::from_frequency_table(const std::unordered_map<byte, unsigned>& frequency_table) {
		build_tree(frequency_table);
		generate_codes(m_root, byte_array());
	}
	void huffman_tree::from_vector(const std::vector<byte>& vec_data) {
		auto frequency_table = build_frequency_table(vec_data);
		build_tree(frequency_table);
		generate_codes(m_root, byte_array());
	}
	void huffman_tree::from_binary_data(const byte_array& serialized_tree) {
		size_t bit_index = 0;
		byte_array copy_data = serialized_tree;
		m_root = deserialize_tree(copy_data, bit_index);
		generate_codes(m_root, byte_array());
	}
	void huffman_tree::generate_codes(std::shared_ptr<huffman_node> node, const byte_array& current_code) {
		if (node == nullptr) return;
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
	byte huffman_tree::decode_single(std::shared_ptr<huffman_node> node, const byte_array& encoded, size_t& bit_index) const {
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
	void huffman_tree::serialize_tree(std::shared_ptr<huffman_node> node, byte_array& buffer) const {
		if (node == nullptr) return;
		if (node->is_leaf()) {
			buffer.push_back(1);
			serialize_data(node->data, buffer);
		}
		else {
			buffer.push_back(0);
			serialize_tree(node->left, buffer);
			serialize_tree(node->right, buffer);
		}
	}
	void huffman_tree::serialize_data(byte data, byte_array& buffer) const {
		for (int i = 7; i >= 0; --i) {
			buffer.push_back((data >> i) & 1);
		}
	}
	std::shared_ptr<huffman_node> huffman_tree::deserialize_tree(byte_array& buffer, size_t& bit_index) const {
		if (bit_index >= buffer.size()) return nullptr;
		if (buffer.bit(bit_index++)) {
			byte data = deserialize_data(buffer, bit_index);
			return std::make_shared<huffman_node>(data, 0);
		}
		else {
			auto left = deserialize_tree(buffer, bit_index);
			auto right = deserialize_tree(buffer, bit_index);
			return std::make_shared<huffman_node>(0, left, right);
		}
	}
	byte huffman_tree::deserialize_data(byte_array& buffer, size_t& bit_index) const {
		byte data = 0;
		for (int i = 7; i >= 0; --i) {
			if (bit_index >= buffer.size()) {
				throw std::runtime_error("预期长度外的树数据");
			}
			if (buffer.bit(bit_index++)) {
				data |= (1 << i);
			}
		}
		return data;
	}
	const byte_array& huffman_tree::encode(byte data) const {
		auto it = m_codes.find(data);
		if (it != m_codes.end()) {
			return it->second;
		}
		throw std::invalid_argument("未找到相应编码");
	}
	byte_array huffman_tree::encode(const std::vector<byte>& vec_data) const {
		byte_array result;
		for (byte data : vec_data) {
			result += encode(data);
		}
		return result;
	}
	std::pair<byte_array, std::string> huffman_tree::encode_with_info(const std::vector<byte>& vec_data) const {
		byte_array encoded = encode(vec_data);
		size_t original_size = vec_data.size() * 8;
		size_t encoded_size = encoded.size();
		double compression_ratio = (1 - (double)encoded_size / original_size) * 100;
		std::ostringstream oss;
		oss << "数据数量：" << vec_data.size() << "\n";
		oss << "原始大小：" << original_size << " 位\n";
		oss << "编码大小：" << encoded_size << " 位\n";
		oss << "压缩率：" << std::fixed << std::setprecision(2) << compression_ratio << "%\n";
		return { encoded,oss.str() };
	}
	std::vector<byte> huffman_tree::decode(const byte_array& encoded) const{
		std::vector<byte> result;
		if (m_root == nullptr || encoded.empty()) return result;
		auto it = std::back_inserter(result);
		if (m_root->is_leaf()) {
			for (std::size_t i = 0; i < encoded.size(); ++i) {
				*it++ = m_root->data;
			}
			return result;
		}
		std::size_t bit_index = 0;
		while (bit_index < encoded.size()) {
			*it++ = decode_single(m_root, encoded, bit_index);
		}
		return result;
	}
	std::vector<byte> huffman_tree::fast_decode(const byte_array& encoded) const {
		std::vector<byte> result;
		auto it = std::back_inserter(result);
		byte_array current_code;
		for (std::size_t i = 0; i < encoded.size(); ++i) {
			current_code.push_back(encoded.bit(i));
			auto at = m_reverse_codes.find(current_code);
			if (at != m_reverse_codes.end()) {
				*it++ = at->second;
				current_code.clear();
			}
		}
		if (!current_code.empty()) {
			throw std::invalid_argument("不完整的编码");
		}
		return result;
	}
	byte_array huffman_tree::to_byte_array() const {
		byte_array buffer;
		serialize_tree(m_root, buffer);
		return buffer;
	}
	std::string huffman_tree::code_table() const {
		std::ostringstream oss;
		for (const auto& pair : m_codes) {
			oss << "[" << std::to_string(pair.first) << "]:" << pair.second << "\n";
		}
		return oss.str();
	}

}