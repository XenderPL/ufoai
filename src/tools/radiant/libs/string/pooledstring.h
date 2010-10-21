
#if !defined(INCLUDED_POOLEDSTRING_H)
#define INCLUDED_POOLEDSTRING_H

#include <map>
#include "generic/static.h"
#include "string/string.h"
#include "container/hashtable.h"
#include "container/hashfunc.h"

/// \brief The string pool class.
class StringPool : public HashTable<char*, std::size_t, RawStringHash, RawStringEqual> {
};

/// \brief A string which can be copied with zero memory cost and minimal runtime cost.
///
/// \param PoolContext The string pool context to use.
template<typename PoolContext>
class PooledString {
	StringPool::iterator m_i;
	static StringPool::iterator increment(StringPool::iterator i) {
		++(*i).value;
		return i;
	}
	static StringPool::iterator insert(const char* string) {
		StringPool::iterator i = PoolContext::instance().find(const_cast<char*>(string));
		if (i == PoolContext::instance().end()) {
			return PoolContext::instance().insert(string_clone(string), 1);
		}
		return increment(i);
	}
	static void erase(StringPool::iterator i) {
		if (--(*i).value == 0) {
			char* string = (*i).key;
			PoolContext::instance().erase(i);
			string_release(string, string_length(string));
		}
	}
public:
	PooledString() : m_i(insert("")) {
	}
	PooledString(const PooledString& other) : m_i(increment(other.m_i)) {
	}
	PooledString(const char* string) : m_i(insert(string)) {
	}
	~PooledString() {
		erase(m_i);
	}
	PooledString& operator=(const PooledString& other) {
		PooledString tmp(other);
		tmp.swap(*this);
		return *this;
	}
	PooledString& operator=(const char* string) {
		PooledString tmp(string);
		tmp.swap(*this);
		return *this;
	}
	void swap(PooledString& other) {
		std::swap(m_i, other.m_i);
	}
	bool operator==(const PooledString& other) const {
		return m_i == other.m_i;
	}
	const char* c_str() const {
		return (*m_i).key;
	}
};


#endif
