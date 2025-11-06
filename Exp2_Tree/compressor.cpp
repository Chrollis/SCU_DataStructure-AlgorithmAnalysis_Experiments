#include "compressor.hpp"

namespace chr {
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
				oss<< std::hex << std::setw(2) << std::setfill('0')
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
	bool operator==(const byte_array& a, const byte_array& b)
	{
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
	size_t byte_array_hash::operator()(const byte_array& ba) const {
		size_t seed = ba.size();
		for (byte b : ba.data()) {
			seed ^= std::hash<byte>{}(b)+0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
}